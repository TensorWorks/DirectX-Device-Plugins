//go:build windows

package plugin

import (
	"context"
	"fmt"
	"strings"
	"time"

	"github.com/tensorworks/directx-device-plugins/plugins/internal/discovery"
	"go.uber.org/zap"
)

// Watches for device updates
type DeviceWatcher struct {

	// Our interface to the underlying DeviceDiscovery object from the DirectX device discovery library
	deviceDiscovery *discovery.DeviceDiscovery

	// The filter used to control which devices are reported
	deviceFilter discovery.DeviceFilter

	// Whether to include integrated GPUs when reporting devices
	includeIntegrated bool

	// Whether to include detachable devices when reporting devices
	includeDetachable bool

	// The list of additional runtime files for each device vendor that will be added to each device's list for System32
	additionalRuntimeFiles map[string][]*discovery.RuntimeFile

	// The list of additional runtime files for each device vendor that will be added to each device's list for SysWOW64
	additionalRuntimeFilesWow64 map[string][]*discovery.RuntimeFile

	// The logger used to log diagnostic information
	logger *zap.SugaredLogger

	// The channel used to request a forced refresh of the device list
	refresh chan struct{}

	// The channel used to stop the device discovery goroutine
	shutdown chan struct{}

	// The channel used to report errors
	Errors chan error

	// The channel used to report device updates
	Updates chan []*discovery.Device
}

func NewDeviceWatcher(
	expectedVersion string,
	deviceFilter discovery.DeviceFilter,
	includeIntegrated bool,
	includeDetachable bool,
	additionalRuntimeFiles map[string][]*discovery.RuntimeFile,
	additionalRuntimeFilesWow64 map[string][]*discovery.RuntimeFile,
	logger *zap.SugaredLogger,
) (*DeviceWatcher, error) {

	// Attempt to load the DirectX device discovery library
	if err := discovery.LoadDiscoveryLibrary(); err != nil {
		return nil, err
	}

	// Verify that the version of the device discovery library matches our expected version
	libraryVersion := discovery.GetDiscoveryLibraryVersion()
	if libraryVersion != expectedVersion {
		return nil, fmt.Errorf(
			"device discovery library version mismatch (found %s, expected %s)",
			libraryVersion,
			expectedVersion,
		)
	}

	// Enable verbose logging for the device discovery library
	discovery.EnableDiscoveryLogging()

	// Create a new DeviceDiscovery object
	deviceDiscovery, err := discovery.NewDeviceDiscovery()
	if err != nil {
		return nil, err
	}

	// Create the DeviceWatcher
	watcher := &DeviceWatcher{
		deviceDiscovery:             deviceDiscovery,
		deviceFilter:                deviceFilter,
		includeIntegrated:           includeIntegrated,
		includeDetachable:           includeDetachable,
		additionalRuntimeFiles:      additionalRuntimeFiles,
		additionalRuntimeFilesWow64: additionalRuntimeFilesWow64,
		logger:                      logger,
		refresh:                     make(chan struct{}, 1),
		shutdown:                    make(chan struct{}),
		Errors:                      make(chan error, 1),
		Updates:                     make(chan []*discovery.Device, 1),
	}

	// Start the watcher goroutine
	go watcher.watchDevices()

	return watcher, nil
}

// Stops our goroutine and destroys the underlying DeviceDiscovery object
func (d *DeviceWatcher) Destroy() {
	close(d.shutdown)
	close(d.refresh)
}

// Forces a refresh of the device list, irrespective of whether the current list is stale
func (d *DeviceWatcher) ForceRefresh() {
	d.refresh <- struct{}{}
}

// Merges any additional runtime files into the list for a device
func (d *DeviceWatcher) mergeRuntimeFiles(device *discovery.Device) {

	// Determine whether we have any additional runtime files for the device vendor
	files, haveFiles := d.additionalRuntimeFiles[strings.ToLower(device.Vendor)]
	filesWow64, haveFilesWow64 := d.additionalRuntimeFilesWow64[strings.ToLower(device.Vendor)]

	// Merge any additions for System32
	if haveFiles {
		ignored := device.AppendRuntimeFiles(files)
		for _, file := range ignored {
			d.logger.Infow("Ignoring additional 64-bit runtime file because it clashes with an existing filename", "file", file)
		}
	}

	// Merge any additions for SysWOW64
	if haveFilesWow64 {
		ignored := device.AppendRuntimeFilesWow64(filesWow64)
		for _, file := range ignored {
			d.logger.Infow("Ignoring additional 32-bit runtime file because it clashes with an existing filename", "file", file)
		}
	}
}

// Refreshes the list of devices and reports the new list
func (d *DeviceWatcher) refreshDevices() error {

	// Refresh the list of devices
	if err := d.deviceDiscovery.DiscoverDevices(d.deviceFilter, d.includeIntegrated, d.includeDetachable); err != nil {
		return err
	}

	// Process any additional runtime files for each device
	for _, device := range d.deviceDiscovery.Devices {
		d.mergeRuntimeFiles(device)
	}

	// Report the new device list
	d.Updates <- d.deviceDiscovery.Devices
	return nil
}

// The main device watch loop
func (d *DeviceWatcher) watchDevices() {

	// Destroy the underlying DeviceDiscovery object when the loop completes
	defer d.deviceDiscovery.Destroy()

	// Use a context for waiting between polling operations rather than sleeping, so we remain responsive to shutdown and refresh events
	sleep, cancelSleep := context.WithTimeout(context.Background(), time.Second*0)
	defer cancelSleep()

	// Continue sending device updates until shutdown is requested:
	forceRefresh := false
	for {
		select {

		case <-d.shutdown:
			return

		case <-d.refresh:
			forceRefresh = true
			cancelSleep()

		case <-sleep.Done():

			// Poll for device list changes
			refresh, err := d.deviceDiscovery.IsRefreshRequired()
			if err != nil {
				d.Errors <- err
				return
			}

			// Retrieve the updated device list if one is available or if a forced refresh has been requested
			if refresh || forceRefresh {
				if err := d.refreshDevices(); err != nil {
					d.Errors <- err
					return
				}
			}

			// Wait 10 seconds before polling again
			forceRefresh = false
			sleep, cancelSleep = context.WithTimeout(context.Background(), time.Second*10)
			defer cancelSleep()
		}
	}
}
