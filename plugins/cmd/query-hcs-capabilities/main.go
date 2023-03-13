//go:build windows

package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"os/exec"
	"strings"
	"unsafe"

	"golang.org/x/sys/windows"
	"golang.org/x/sys/windows/registry"
)

var (
	vmcompute               = windows.NewLazyDLL("vmcompute.dll")
	hcsGetServiceProperties = vmcompute.NewProc("HcsGetServiceProperties")
)

// HCS schema Version structure: <https://learn.microsoft.com/en-us/virtualization/api/hcs/schemareference#version>
type Version struct {
	Major uint32
	Minor uint32
}

// HCS schema BasicInformation structure: <https://learn.microsoft.com/en-us/virtualization/api/hcs/schemareference#BasicInformation>
type BasicInformation struct {
	SupportedSchemaVersions []*Version
}

// Modified version of the HCS schema ServiceProperties structure: <https://learn.microsoft.com/en-us/virtualization/api/hcs/schemareference#ServiceProperties>
// Note that we just treat the array as containing BasicInformation objects, since that's what our specific query returns
type ServiceProperties struct {
	Properties []*BasicInformation
}

func getSupportedSchemas() ([]*Version, error) {

	// Attempt to load vmcompute.dll
	if err := vmcompute.Load(); err != nil {
		return nil, fmt.Errorf("failed to load %s: %s", vmcompute.Name, err.Error())
	}

	// Convert our query string into a UTF-16 pointer
	queryPtr, err := windows.UTF16PtrFromString("{\"PropertyTypes\": [\"Basic\"]}")
	if err != nil {
		return nil, fmt.Errorf("failed to convert string to UTF-16: %s", err.Error())
	}

	// Call HcsGetServiceProperties() to query the supported schema version
	var resultPtr *uint16 = nil
	retval, _, _ := hcsGetServiceProperties.Call(
		uintptr(unsafe.Pointer(queryPtr)),
		uintptr(unsafe.Pointer(&resultPtr)),
	)

	// Verify that the query was successful
	if retval != 0 {
		return nil, fmt.Errorf("HcsGetServiceProperties() failed: %v", windows.Errno(retval))
	}

	// Convert the result into a JSON string
	result := windows.UTF16PtrToString((*uint16)(unsafe.Pointer(resultPtr)))

	// Parse the JSON
	serviceProperties := &ServiceProperties{}
	if err := json.Unmarshal([]byte(result), &serviceProperties); err != nil {
		return nil, err
	}

	// Verify that we have at least one supported schema version
	if len(serviceProperties.Properties) == 0 || len(serviceProperties.Properties[0].SupportedSchemaVersions) == 0 {
		return nil, errors.New("HcsGetServiceProperties() returned zero supported schema versions")
	}

	// Return the list of supported schema versions
	return serviceProperties.Properties[0].SupportedSchemaVersions, nil
}

func getWindowsVersion() (string, error) {

	// Use `RtlGetVersion()` to query the Windows version number, so manifest semantics are ignored
	versionInfo := windows.RtlGetVersion()

	// Use PowerShell to query WMI for the system caption, since `ProductName` in the registry is no longer reliable
	productName, err := exec.Command("powershell", "-Command", "(Get-WmiObject -Class Win32_OperatingSystem).Caption").Output()
	if err != nil {
		return "", fmt.Errorf("failed to query WMI for the product version string: %s", err.Error())
	}

	// Open the registry key for the Windows version information
	key, err := registry.OpenKey(registry.LOCAL_MACHINE, `SOFTWARE\Microsoft\Windows NT\CurrentVersion`, registry.QUERY_VALUE)
	if err != nil {
		return "", fmt.Errorf("failed to open the registry key \"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\": %s", err.Error())
	}
	defer key.Close()

	// Attempt to retrieve the display version on newer systems, falling back to the old release ID value on older systems
	displayVersion, _, err := key.GetStringValue("DisplayVersion")
	if err != nil {
		displayVersion, _, err = key.GetStringValue("ReleaseId")
		if err != nil {
			return "", fmt.Errorf("failed to retrieve either the \"DisplayVersion\" or \"ReleaseId\" registry value: %s", err.Error())
		}
	}

	// Retrieve the revision number, since this isn't included in the `RtlGetVersion()` output
	revisionNumber, _, err := key.GetIntegerValue("UBR")
	if err != nil {
		return "", fmt.Errorf("failed to retrieve the \"UBR\" registry value: %s", err.Error())
	}

	// Build an aggregated version string from the retrieved values
	return fmt.Sprintf(
		"%s, version %s (OS build %d.%d.%d.%d)",
		strings.TrimSpace(string(productName)),
		displayVersion,
		versionInfo.MajorVersion,
		versionInfo.MinorVersion,
		versionInfo.BuildNumber,
		revisionNumber,
	), nil
}

func main() {

	// Retrieve the Windows version information
	windowsVersion, err := getWindowsVersion()
	if err != nil {
		log.Fatalf("Failed to retrieve Windows version information: %s", err.Error())
	}

	// Query the Host Compute Service (HCS) for the list of supported schema versions
	supportedSchemas, err := getSupportedSchemas()
	if err != nil {
		log.Fatalf("Failed to retrieve the supported HCS schema version: %s", err.Error())
	}

	// Print the Windows version details and supported schema version
	fmt.Println("Operating system version:")
	fmt.Println(windowsVersion)
	fmt.Println()
	fmt.Println("Supported HCS schema versions:")
	for _, version := range supportedSchemas {
		fmt.Printf("- %d.%d", version.Major, version.Minor)
	}
}
