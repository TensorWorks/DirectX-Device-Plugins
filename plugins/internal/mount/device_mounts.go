//go:build windows

package mount

import (
	"os"
	"path/filepath"

	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"

	"github.com/tensorworks/directx-device-plugins/plugins/internal/discovery"
)

// Generates the device specs for the supplied list of devices
func SpecsForDevices(devices []*discovery.Device) []*pluginapi.DeviceSpec {

	specs := []*pluginapi.DeviceSpec{}

	// Provide the physical location path for each device, avoiding duplicates (duplicate paths can occur when
	// multitenancy is enabled and two requested device IDs map to the same underlying physical device)
	for _, device := range devices {
		specs = appendUniqueSpec(specs, &pluginapi.DeviceSpec{
			HostPath:      "vpci-location-path://" + device.LocationPath,
			ContainerPath: "",
			Permissions:   "",
		})
	}

	return specs
}

// Generates the runtime file mounts for the supplied list of devices
func MountsForDevices(devices []*discovery.Device) []*pluginapi.Mount {

	mounts := []*pluginapi.Mount{}

	for _, device := range devices {

		// Generates the mounts for a list of runtime files
		generateMounts := func(files []*discovery.RuntimeFile, destinationRoot string) {
			for _, file := range files {

				// Resolve the absolute paths to the host source file and the container destination file
				source := filepath.Join(device.DriverStorePath, file.SourcePath)
				destination := filepath.Join(destinationRoot, file.DestinationFilename)

				// Only mount the file if it exists on the host and can be accessed, and isn't a duplicate
				// (Note that duplicate container paths can occur not only when mounting multiple devices
				// from a single vendor, but also when device drivers from different vendors mount files
				// to the same target path, which means that a container will only see the files from the
				// first device's vendor when collisions occur between different device drivers)
				_, err := os.Stat(source)
				if err == nil {
					mounts = appendUniqueMount(mounts, &pluginapi.Mount{
						HostPath:      source,
						ContainerPath: destination,
						ReadOnly:      true,
					})
				}
			}
		}

		// Generate the mounts for both the System32 and SysWOW64 runtime files
		generateMounts(device.RuntimeFiles, "C:\\Windows\\System32")
		generateMounts(device.RuntimeFilesWow64, "C:\\Windows\\SysWOW64")
	}

	return mounts
}

// Appends a device spec to an existing list of device specs if it's not already present in the list
func appendUniqueSpec(specs []*pluginapi.DeviceSpec, newSpec *pluginapi.DeviceSpec) []*pluginapi.DeviceSpec {
	for _, existing := range specs {
		if existing.HostPath == newSpec.HostPath {
			return specs
		}
	}

	return append(specs, newSpec)
}

// Appends a mount to an existing list of mounts if it's not already present in the list
func appendUniqueMount(mounts []*pluginapi.Mount, newMount *pluginapi.Mount) []*pluginapi.Mount {
	for _, existing := range mounts {
		if existing.ContainerPath == newMount.ContainerPath {
			return mounts
		}
	}

	return append(mounts, newMount)
}
