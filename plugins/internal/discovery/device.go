//go:build windows

package discovery

// Represents a DirectX device
type Device struct {

	// The unique PNP hardware identifier for the device
	ID string

	// A human-readable description of the device (e.g. the model name)
	Description string

	// The registry key that contains the driver details for the device
	DriverRegistryKey string

	// The absolute path to the directory in the driver store that contains the driver files for the device
	DriverStorePath string

	// The path to the physical location of the device in the system
	LocationPath string

	// The list of additional files that need to be copied from the driver store to the System32 directory in order to use the device with non-DirectX runtimes
	RuntimeFiles []*RuntimeFile

	// The list of additional files that need to be copied from the driver store to the SysWOW64 directory in order to use the device with non-DirectX runtimes
	RuntimeFilesWow64 []*RuntimeFile

	// The vendor of the device (e.g. AMD, Intel, NVIDIA)
	Vendor string

	// The DirectX adapter LUID associated with the PnP device
	AdapterLUID int64

	// Specifies whether the device is an integrated GPU (as opposed to a discrete GPU)
	IsIntegrated bool

	// Specifies whether the device is a detachable device (i.e. the device can be removed at runtime)
	IsDetachable bool

	// Specifies whether the device supports display
	// (i.e. supports either the DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS or DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS attributes)
	SupportsDisplay bool

	// Specifies whether the device supports compute (i.e. supports the DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE attribute)
	SupportsCompute bool
}

// Appends the supplied list of 64-bit runtime files, ignoring and returning any files that clash with existing mount destinations
func (d *Device) AppendRuntimeFiles(files []*RuntimeFile) []*RuntimeFile {
	merged, ignored := mergeRuntimeFiles(d.RuntimeFiles, files)
	d.RuntimeFiles = merged
	return ignored
}

// Appends the supplied list of 32-bit runtime files, ignoring and returning any files that clash with existing mount destinations
func (d *Device) AppendRuntimeFilesWow64(files []*RuntimeFile) []*RuntimeFile {
	merged, ignored := mergeRuntimeFiles(d.RuntimeFilesWow64, files)
	d.RuntimeFilesWow64 = merged
	return ignored
}

// Merges two lists of runtime files, ignoring any files that clash with existing mount destinations. Returns both the merged list and the list of ignored files.
func mergeRuntimeFiles(files []*RuntimeFile, additions []*RuntimeFile) ([]*RuntimeFile, []*RuntimeFile) {
	merged := files
	ignored := []*RuntimeFile{}

	// Add each additional file to the list if it doesn't clash with an existing destination filename
outer:
	for _, additionalFile := range additions {

		// Determine whether we have an existing file with the same destination as the new file
		for _, existingFile := range merged {
			if existingFile.DestinationFilename == additionalFile.DestinationFilename {
				ignored = append(ignored, additionalFile)
				continue outer
			}
		}

		// Add the file to the list
		merged = append(merged, additionalFile)
	}

	return merged, ignored
}
