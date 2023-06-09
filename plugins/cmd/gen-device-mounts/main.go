//go:build windows

package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"strings"

	"github.com/spf13/pflag"
	"github.com/tensorworks/directx-device-plugins/plugins/internal/discovery"
	"github.com/tensorworks/directx-device-plugins/plugins/internal/mount"
)

// Prints output to stderr
func ePrint(a ...any) (n int, err error) {
	return fmt.Fprint(os.Stdout, a...)
}

// Prints output to stderr, with spaces between operands and a trailing newline
func ePrintln(a ...any) (n int, err error) {
	return fmt.Fprintln(os.Stdout, a...)
}

// Determines whether the specified value exists in the supplied list of values
func contains[T comparable](values []T, value T) bool {
	for _, existing := range values {
		if existing == value {
			return true
		}
	}

	return false
}

// Formats a list of numeric values
func formatNumbers[T uint | int64](values []T) string {
	formatted := []string{}
	for _, value := range values {
		formatted = append(formatted, fmt.Sprint(value))
	}
	return strings.Join(formatted, ", ")
}

// Formats a list of string values
func formatStrings(values []string, delimiter string) string {
	formatted := []string{}
	for _, str := range values {
		formatted = append(formatted, fmt.Sprint("\"", str, "\""))
	}
	return strings.Join(formatted, delimiter)
}

func main() {

	// Configure our custom help message
	pflag.CommandLine.SetOutput(os.Stderr)
	pflag.Usage = func() {
		ePrintln("gen-device-mounts: generates flags for exposing devices to containers with `ctr run`")
		ePrintln("\nUsage syntax:", os.Args[0], "[-h] [--format text|json] [--all] [--index <INDEX>] [--luid <LUID>] [--path <PATH>] [--run] [--verbose] [<FLAGS TO USE WITH --run>]")
		ePrintln("\nOptions:")
		pflag.PrintDefaults()
		ePrintln(strings.Join([]string{
			"",
			"The list of available DirectX devices (including their enumeration indices, LUID values, and PCI paths)",
			"can be retrieved by running either `test-device-discovery-cpp.exe` or `test-device-discovery-go.exe`.",
			"",
			"NOTES REGARDING OTHER FRONTENDS",
			"-------------------------------",
			"",
			"Docker:",
			"",
			"Although Docker version 23.0.0 introduced support for exposing individial devices using their PCI location",
			"paths, it still lacks the ability to bind-mount individual files rather than directories, which prevents",
			"it from using the flags generated by `gen-device-mounts`. This is due to its continued use of the HCSv1",
			"API rather than the newer HCSv2 API used by containerd. When Docker eventually migrates to using HCSv2",
			"(or using containerd under Windows the way it does under Linux) then `gen-device-mounts` will be updated",
			"to add an option to invoke `docker run` instead of `ctr run` when --run is specified.",
			"",
			"nerdctl:",
			"",
			"nerdctl is currently blocked by two outstanding issues that prevent it from using the flags generated by",
			"`gen-device-mounts`:",
			"",
			"- https://github.com/containerd/nerdctl/pull/2079",
			"- https://github.com/containerd/nerdctl/issues/759",
			"",
			"Once these blockers have been resolved then `gen-device-mounts` will be updated to add an option to invoke",
			"`nerdctl run` instead of `ctr run` when --run is specified.",
		}, "\n"))
		os.Exit(1)
	}

	// Parse our command-line arguments
	allDevices := pflag.Bool("all", false, "Expose all available DirectX devices")
	outputFormat := pflag.String("format", "text", "The output format for generated flags (\"text\" or \"json\")")
	devicesByIndex := pflag.UintSlice("index", []uint{}, "Expose the DirectX device with the specified enumeration index (can be specified multiple times)")
	devicesByLUID := pflag.Int64Slice("luid", []int64{}, "Expose the DirectX device with the specified LUID (can be specified multiple times)")
	devicesByPath := pflag.StringSlice("path", []string{}, "Expose the DirectX device with the specified PCI path (can be specified multiple times)")
	runContainer := pflag.Bool("run", false, "run")
	verbose := pflag.Bool("verbose", false, "Enable verbose output")
	pflag.Parse()

	// Verify that a valid output format was specified
	if !contains([]string{"text", "json"}, *outputFormat) {
		log.Fatalln("Error: unknown output format \"", *outputFormat, "\" (supported formats are \"text\" and \"json\")")
	}

	// Print our device selection criteria
	if *verbose {
		ePrintln("Device selection criteria:")
		if *allDevices {
			ePrintln("- Include all available DirectX devices")
		} else {
			if len(*devicesByIndex) > 0 {
				ePrintln("- Include DirectX devices with the following enumeration indices:", formatNumbers(*devicesByIndex))
			}
			if len(*devicesByLUID) > 0 {
				ePrintln("- Include DirectX devices with the following LUID values:", formatNumbers(*devicesByLUID))
			}
			if len(*devicesByPath) > 0 {
				ePrintln("- Include DirectX devices with the following PCI paths:", formatStrings(*devicesByPath, ", "))
			}
		}
		ePrintln()
	}

	// Attempt to load the DirectX device discovery library
	if err := discovery.LoadDiscoveryLibrary(); err != nil {
		log.Fatalln("Error:", err)
	}

	// Create a new DeviceDiscovery object
	deviceDiscovery, err := discovery.NewDeviceDiscovery()
	if err != nil {
		log.Fatalln("Error:", err)
	}

	// Perform device discovery
	if err := deviceDiscovery.DiscoverDevices(discovery.AllDevices, true, true); err != nil {
		log.Fatalln("Error:", err)
	}

	// Filter the list of devices based on the specified selection criteria
	filtered := []*discovery.Device{}
	for index, device := range deviceDiscovery.Devices {
		if *allDevices || contains(*devicesByIndex, uint(index)) || contains(*devicesByLUID, device.AdapterLUID) || contains(*devicesByPath, device.LocationPath) {
			filtered = append(filtered, device)
		}
	}

	// Print the details of the selected devices
	if *verbose {
		ePrint("Selected ", len(filtered), " device(s) based on selection criteria:\n")
		for index, device := range filtered {
			ePrint("- Index ", index, ", LUID ", device.AdapterLUID, ", PCI Path ", device.LocationPath, "\n")
		}
		ePrintln()
	}

	// Append our default runtime file mounts to the lists for each device
	for _, device := range deviceDiscovery.Devices {

		// Determine whether we have any additional runtime files for the device vendor
		files, haveFiles := mount.DefaultMounts[strings.ToLower(device.Vendor)]
		filesWow64, haveFilesWow64 := mount.DefaultMountsWow64[strings.ToLower(device.Vendor)]

		// Merge any additions for System32
		if haveFiles {
			ignored := device.AppendRuntimeFiles(files)
			for _, file := range ignored {
				ePrintln("Ignoring additional 64-bit runtime file because it clashes with an existing filename: ", file)
			}
		}

		// Merge any additions for SysWOW64
		if haveFilesWow64 {
			ignored := device.AppendRuntimeFilesWow64(filesWow64)
			for _, file := range ignored {
				ePrintln("Ignoring additional 32-bit runtime file because it clashes with an existing filename: ", file)
			}
		}
	}

	// Generate the device specs and runtime file mounts for the selected devices
	specs := mount.SpecsForDevices(filtered)
	mounts := mount.MountsForDevices(filtered)

	// Generate the flags for mounting the devices
	flags := []string{}
	for _, spec := range specs {
		flags = append(flags, "--device", spec.HostPath)
	}
	for _, mount := range mounts {
		flags = append(flags, "--mount", fmt.Sprint("src=", mount.HostPath, ",dst=", mount.GetContainerPath()))
	}

	// Determine whether we are just printing the flags, or running a container with them
	if *runContainer {

		// Create a command object to represent our `ctr run` invocation
		cmd := exec.Command("ctr", "run", "--rm")

		// Allow the child process to inherit all standard streams
		cmd.Stdin = os.Stdin
		cmd.Stderr = os.Stderr
		cmd.Stdout = os.Stdout

		// Append both our generated flags and any loose command-line arguments to the invocation
		cmd.Args = append(cmd.Args, flags...)
		cmd.Args = append(cmd.Args, pflag.Args()...)

		// Print the generated command to stderr, wrapping each flag in quotes
		ePrintln(formatStrings(cmd.Args, " "))

		// Attempt to run `ctr run`
		if err := cmd.Run(); err != nil {
			log.Fatalln("Error:", err)
		}

	} else {

		// Determine which format we are using to print the list of flags
		if *outputFormat == "json" {

			// Attempt to format the flags as a JSON array
			formatted, err := json.Marshal(flags)
			if err != nil {
				log.Fatalln("Error:", err)
			}

			// Print the JSON array to stdout
			fmt.Println(formatted)

		} else {

			// Print the list of flags to stdout, wrapping each flag in quotes
			fmt.Println(formatStrings(flags, " "))

		}
	}
}
