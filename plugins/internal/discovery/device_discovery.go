//go:build windows

package discovery

import (
	"errors"
	"fmt"
	"unsafe"

	"golang.org/x/sys/windows"
)

var (
	discoverydll                       = windows.NewLazyDLL("directx-device-discovery.dll")
	procGetDiscoveryLibraryVersion     = discoverydll.NewProc("GetDiscoveryLibraryVersion")
	procDisableDiscoveryLogging        = discoverydll.NewProc("DisableDiscoveryLogging")
	procEnableDiscoveryLogging         = discoverydll.NewProc("EnableDiscoveryLogging")
	procCreateDeviceDiscoveryInstance  = discoverydll.NewProc("CreateDeviceDiscoveryInstance")
	procDestroyDeviceDiscoveryInstance = discoverydll.NewProc("DestroyDeviceDiscoveryInstance")
	procGetLastErrorMessage            = discoverydll.NewProc("DeviceDiscovery_GetLastErrorMessage")
	procIsRefreshRequired              = discoverydll.NewProc("DeviceDiscovery_IsRefreshRequired")
	procDiscoverDevices                = discoverydll.NewProc("DeviceDiscovery_DiscoverDevices")
	procGetNumDevices                  = discoverydll.NewProc("DeviceDiscovery_GetNumDevices")
	procGetDeviceAdapterLUID           = discoverydll.NewProc("DeviceDiscovery_GetDeviceAdapterLUID")
	procGetDeviceID                    = discoverydll.NewProc("DeviceDiscovery_GetDeviceID")
	procGetDeviceDescription           = discoverydll.NewProc("DeviceDiscovery_GetDeviceDescription")
	procGetDeviceDriverRegistryKey     = discoverydll.NewProc("DeviceDiscovery_GetDeviceDriverRegistryKey")
	procGetDeviceDriverStorePath       = discoverydll.NewProc("DeviceDiscovery_GetDeviceDriverStorePath")
	procGetDeviceLocationPath          = discoverydll.NewProc("DeviceDiscovery_GetDeviceLocationPath")
	procGetDeviceVendor                = discoverydll.NewProc("DeviceDiscovery_GetDeviceVendor")
	procGetNumRuntimeFiles             = discoverydll.NewProc("DeviceDiscovery_GetNumRuntimeFiles")
	procGetRuntimeFileSource           = discoverydll.NewProc("DeviceDiscovery_GetRuntimeFileSource")
	procGetRuntimeFileDestination      = discoverydll.NewProc("DeviceDiscovery_GetRuntimeFileDestination")
	procGetNumRuntimeFilesWow64        = discoverydll.NewProc("DeviceDiscovery_GetNumRuntimeFilesWow64")
	procGetRuntimeFileSourceWow64      = discoverydll.NewProc("DeviceDiscovery_GetRuntimeFileSourceWow64")
	procGetRuntimeFileDestinationWow64 = discoverydll.NewProc("DeviceDiscovery_GetRuntimeFileDestinationWow64")
	procIsDeviceIntegrated             = discoverydll.NewProc("DeviceDiscovery_IsDeviceIntegrated")
	procIsDeviceDetachable             = discoverydll.NewProc("DeviceDiscovery_IsDeviceDetachable")
	procDoesDeviceSupportDisplay       = discoverydll.NewProc("DeviceDiscovery_DoesDeviceSupportDisplay")
	procDoesDeviceSupportCompute       = discoverydll.NewProc("DeviceDiscovery_DoesDeviceSupportCompute")
)

type DeviceDiscovery struct {

	// The handle to the underlying DeviceDiscovery object
	handle uintptr

	// The list of discovered devices
	Devices []*Device
}

// Attempts to load the DirectX device discovery library and returns an error if loading fails
// (This is useful for catching failures gracefully rather than triggering a panic upon lazy-load)
func LoadDiscoveryLibrary() error {

	if err := discoverydll.Load(); err != nil {
		return fmt.Errorf("failed to load %s: %s", discoverydll.Name, err.Error())
	}

	return nil
}

// Wrapper function for GetDiscoveryLibraryVersion
func GetDiscoveryLibraryVersion() string {
	result, _, _ := procGetDiscoveryLibraryVersion.Call()
	return windows.UTF16PtrToString((*uint16)(unsafe.Pointer(result)))
}

// Wrapper function for DisableDiscoveryLogging
func DisableDiscoveryLogging() {
	procDisableDiscoveryLogging.Call()
}

// Wrapper function for EnableDiscoveryLogging
func EnableDiscoveryLogging() {
	procEnableDiscoveryLogging.Call()
}

func NewDeviceDiscovery() (*DeviceDiscovery, error) {

	// Attempt to create a DeviceDiscovery instance
	result, _, _ := procCreateDeviceDiscoveryInstance.Call()
	if result == 0 {
		return nil, errors.New("failed to create the DeviceDiscovery instance")
	}

	return &DeviceDiscovery{
		handle:  result,
		Devices: []*Device{},
	}, nil
}

func (d *DeviceDiscovery) Destroy() {
	procDestroyDeviceDiscoveryInstance.Call(d.handle)
}

// Formats a boolean argument for passing to a library function
func (d *DeviceDiscovery) booleanArgument(arg bool) uintptr {
	if arg {
		return 1
	} else {
		return 0
	}
}

// Handles the result of a library function that returns a boolean
func (d *DeviceDiscovery) handleBooleanResult(result uintptr, r2 uintptr, lastError error) (bool, error) {
	if int32(result) == -1 {
		return false, d.getLastErrorMessage()
	}

	return result == 1, nil
}

// Handles the result of a library function that returns an unsigned 32-bit integer
func (d *DeviceDiscovery) handleUint32Result(result uintptr, r2 uintptr, lastError error) (uint32, error) {
	if int32(result) == -1 {
		return 0, d.getLastErrorMessage()
	}

	return uint32(result), nil
}

// Handles the result of a library function that returns a signed 64-bit integer
func (d *DeviceDiscovery) handleInt64Result(result uintptr, r2 uintptr, lastError error) (int64, error) {
	if int64(result) == -1 {
		return 0, d.getLastErrorMessage()
	}

	return int64(result), nil
}

// Handles the result of a library function that returns a UTF-16 string
func (d *DeviceDiscovery) handleStringResult(result uintptr, r2 uintptr, lastError error) (string, error) {
	if result == 0 {
		return "", d.getLastErrorMessage()
	}

	return windows.UTF16PtrToString((*uint16)(unsafe.Pointer(result))), nil
}

// Retrieves the error message for the last library function call
func (d *DeviceDiscovery) getLastErrorMessage() error {

	// Retrieve the last error message from the library and convert it to a Go error
	result, _, _ := procGetLastErrorMessage.Call(d.handle)
	errorMessage := windows.UTF16PtrToString((*uint16)(unsafe.Pointer(result)))

	if errorMessage != "" {
		return errors.New(errorMessage)
	}

	return nil
}

// Performs device discovery and populates our list of devices
func (d *DeviceDiscovery) DiscoverDevices(filter DeviceFilter, includeIntegrated bool, includeDetachable bool) error {

	// Attempt to perform device discovery
	result, _, _ := procDiscoverDevices.Call(d.handle, uintptr(filter), d.booleanArgument(includeIntegrated), d.booleanArgument(includeDetachable))
	if int32(result) == -1 {
		return d.getLastErrorMessage()
	}

	// Determine the number of discovered devices
	numDevices, err := d.getNumDevices()
	if err != nil {
		return err
	}

	// Clear the existing list of devices
	d.Devices = []*Device{}

	// Retrieve the details of each device in turn
	for index := 0; index < int(numDevices); index += 1 {

		// Attempt to retrieve the device details
		device, err := d.getDevice(index)
		if err != nil {
			return err
		}

		// Add the device to our list
		d.Devices = append(d.Devices, device)
	}

	return nil
}

// Retrieves the details for an individual device
func (d *DeviceDiscovery) getDevice(device int) (*Device, error) {

	// Attempt to retrieve the device ID
	id, err := d.getDeviceID(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the device description
	description, err := d.getDeviceDescription(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the device driver registry key
	registry, err := d.getDeviceDriverRegistryKey(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the device driver store path
	driverStore, err := d.getDeviceDriverStorePath(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the device location path
	location, err := d.getDeviceLocationPath(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the device vendor
	vendor, err := d.getDeviceVendor(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the device adapter LUID
	luid, err := d.getDeviceAdapterLUID(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the integrated device hardware flag
	integrated, err := d.isDeviceIntegrated(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the detachable device hardware flag
	detachable, err := d.isDeviceDetachable(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the display support flag
	display, err := d.doesDeviceSupportDisplay(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the compute support flag
	compute, err := d.doesDeviceSupportCompute(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the number of additional runtime files for System32
	numRuntimeFiles, err := d.getNumRuntimeFiles(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the list of additional runtime files for System32
	runtimeFiles := []*RuntimeFile{}
	for file := 0; file < int(numRuntimeFiles); file += 1 {

		// Attempt to retrieve the source path for the file
		sourcePath, err := d.getRuntimeFileSource(device, file)
		if err != nil {
			return nil, err
		}

		// Attempt to retrieve the destination filename for the file
		destinationFilename, err := d.getRuntimeFileDestination(device, file)
		if err != nil {
			return nil, err
		}

		// Add the file details to the list
		runtimeFiles = append(runtimeFiles, &RuntimeFile{
			SourcePath:          sourcePath,
			DestinationFilename: destinationFilename,
		})
	}

	// Attempt to retrieve the number of additional runtime files for SysWOW64
	numRuntimeFilesWow64, err := d.getNumRuntimeFilesWow64(device)
	if err != nil {
		return nil, err
	}

	// Attempt to retrieve the list of additional runtime files for System32
	runtimeFilesWow64 := []*RuntimeFile{}
	for file := 0; file < int(numRuntimeFilesWow64); file += 1 {

		// Attempt to retrieve the source path for the file
		sourcePath, err := d.getRuntimeFileSourceWow64(device, file)
		if err != nil {
			return nil, err
		}

		// Attempt to retrieve the destination filename for the file
		destinationFilename, err := d.getRuntimeFileDestinationWow64(device, file)
		if err != nil {
			return nil, err
		}

		// Add the file details to the list
		runtimeFilesWow64 = append(runtimeFilesWow64, &RuntimeFile{
			SourcePath:          sourcePath,
			DestinationFilename: destinationFilename,
		})
	}

	// Construct a Device object from the retrieved data
	return &Device{
		ID:                id,
		Description:       description,
		DriverRegistryKey: registry,
		DriverStorePath:   driverStore,
		LocationPath:      location,
		RuntimeFiles:      runtimeFiles,
		RuntimeFilesWow64: runtimeFilesWow64,
		Vendor:            vendor,
		AdapterLUID:       luid,
		IsIntegrated:      integrated,
		IsDetachable:      detachable,
		SupportsDisplay:   display,
		SupportsCompute:   compute,
	}, nil
}

// Wrapper function for DeviceDiscovery_IsRefreshRequired
func (d *DeviceDiscovery) IsRefreshRequired() (bool, error) {
	return d.handleBooleanResult(
		procIsRefreshRequired.Call(d.handle),
	)
}

// Wrapper function for DeviceDiscovery_GetNumDevices
func (d *DeviceDiscovery) getNumDevices() (uint32, error) {
	return d.handleUint32Result(
		procGetNumDevices.Call(d.handle),
	)
}

// Wrapper function for DeviceDiscovery_GetDeviceAdapterLUID
func (d *DeviceDiscovery) getDeviceAdapterLUID(device int) (int64, error) {
	return d.handleInt64Result(
		procGetDeviceAdapterLUID.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_GetDeviceID
func (d *DeviceDiscovery) getDeviceID(device int) (string, error) {
	return d.handleStringResult(
		procGetDeviceID.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_GetDeviceDescription
func (d *DeviceDiscovery) getDeviceDescription(device int) (string, error) {
	return d.handleStringResult(
		procGetDeviceDescription.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_GetDeviceDriverRegistryKey
func (d *DeviceDiscovery) getDeviceDriverRegistryKey(device int) (string, error) {
	return d.handleStringResult(
		procGetDeviceDriverRegistryKey.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_GetDeviceDriverStorePath
func (d *DeviceDiscovery) getDeviceDriverStorePath(device int) (string, error) {
	return d.handleStringResult(
		procGetDeviceDriverStorePath.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_GetDeviceLocationPath
func (d *DeviceDiscovery) getDeviceLocationPath(device int) (string, error) {
	return d.handleStringResult(
		procGetDeviceLocationPath.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_GetDeviceVendor
func (d *DeviceDiscovery) getDeviceVendor(device int) (string, error) {
	return d.handleStringResult(
		procGetDeviceVendor.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_GetNumRuntimeFiles
func (d *DeviceDiscovery) getNumRuntimeFiles(device int) (uint32, error) {
	return d.handleUint32Result(
		procGetNumRuntimeFiles.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_GetRuntimeFileSource
func (d *DeviceDiscovery) getRuntimeFileSource(device int, file int) (string, error) {
	return d.handleStringResult(
		procGetRuntimeFileSource.Call(d.handle, uintptr(device), uintptr(file)),
	)
}

// Wrapper function for DeviceDiscovery_GetRuntimeFileDestination
func (d *DeviceDiscovery) getRuntimeFileDestination(device int, file int) (string, error) {
	return d.handleStringResult(
		procGetRuntimeFileDestination.Call(d.handle, uintptr(device), uintptr(file)),
	)
}

// Wrapper function for DeviceDiscovery_GetNumRuntimeFilesWow64
func (d *DeviceDiscovery) getNumRuntimeFilesWow64(device int) (uint32, error) {
	return d.handleUint32Result(
		procGetNumRuntimeFilesWow64.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_GetRuntimeFileSourceWow64
func (d *DeviceDiscovery) getRuntimeFileSourceWow64(device int, file int) (string, error) {
	return d.handleStringResult(
		procGetRuntimeFileSourceWow64.Call(d.handle, uintptr(device), uintptr(file)),
	)
}

// Wrapper function for DeviceDiscovery_GetRuntimeFileDestinationWow64
func (d *DeviceDiscovery) getRuntimeFileDestinationWow64(device int, file int) (string, error) {
	return d.handleStringResult(
		procGetRuntimeFileDestinationWow64.Call(d.handle, uintptr(device), uintptr(file)),
	)
}

// Wrapper function for DeviceDiscovery_IsDeviceIntegrated
func (d *DeviceDiscovery) isDeviceIntegrated(device int) (bool, error) {
	return d.handleBooleanResult(
		procIsDeviceIntegrated.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_IsDeviceDetachable
func (d *DeviceDiscovery) isDeviceDetachable(device int) (bool, error) {
	return d.handleBooleanResult(
		procIsDeviceDetachable.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_DoesDeviceSupportDisplay
func (d *DeviceDiscovery) doesDeviceSupportDisplay(device int) (bool, error) {
	return d.handleBooleanResult(
		procDoesDeviceSupportDisplay.Call(d.handle, uintptr(device)),
	)
}

// Wrapper function for DeviceDiscovery_DoesDeviceSupportCompute
func (d *DeviceDiscovery) doesDeviceSupportCompute(device int) (bool, error) {
	return d.handleBooleanResult(
		procDoesDeviceSupportCompute.Call(d.handle, uintptr(device)),
	)
}
