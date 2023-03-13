//go:build windows

package main

import (
	"flag"
	"fmt"
	"log"

	"github.com/tensorworks/directx-device-plugins/plugins/internal/discovery"
)

func main() {

	// Parse our command-line arguments
	verbose := flag.Bool("verbose", false, "enable verbose logging")
	flag.Parse()

	// Attempt to load the DirectX device discovery library
	if err := discovery.LoadDiscoveryLibrary(); err != nil {
		log.Fatalln("Error:", err)
	}

	// Enable verbose logging for the device discovery library if it has been requested
	if *verbose {
		discovery.EnableDiscoveryLogging()
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

	// Print the library version string and the number of discovered devices
	fmt.Print("DirectX device discovery library version ", discovery.GetDiscoveryLibraryVersion(), "\n")
	fmt.Print("Discovered ", len(deviceDiscovery.Devices), " devices.\n\n")

	// Print the details for each device
	for index, device := range deviceDiscovery.Devices {
		fmt.Print("[Device ", index, " details]\n\n")
		fmt.Println("PnP Hardware ID:    ", device.ID)
		fmt.Println("DX Adapter LUID:    ", device.AdapterLUID)
		fmt.Println("Description:        ", device.Description)
		fmt.Println("Driver Registry Key:", device.DriverRegistryKey)
		fmt.Println("DriverStore Path:   ", device.DriverStorePath)
		fmt.Println("LocationPath:       ", device.LocationPath)
		fmt.Println("Vendor:             ", device.Vendor)
		fmt.Println("Is Integrated:      ", device.IsIntegrated)
		fmt.Println("Is Detachable:      ", device.IsDetachable)
		fmt.Println("Supports Display:   ", device.SupportsDisplay)
		fmt.Println("Supports Compute:   ", device.SupportsCompute)

		fmt.Print("\n", len(device.RuntimeFiles), " Additional System32 runtime files:\n")
		for _, file := range device.RuntimeFiles {
			fmt.Println("   ", file.SourcePath, "=>", file.DestinationFilename)
		}

		fmt.Print("\n", len(device.RuntimeFilesWow64), " Additional SysWOW64 runtime files:\n")
		for _, file := range device.RuntimeFilesWow64 {
			fmt.Println("   ", file.SourcePath, "=>", file.DestinationFilename)
		}

		fmt.Print("\n")
	}
}
