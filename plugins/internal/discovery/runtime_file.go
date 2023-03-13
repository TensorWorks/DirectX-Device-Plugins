//go:build windows

package discovery

// Represents an additional file that needs to be copied from the driver store to the system directory in order to use a device with non-DirectX runtimes
// (For details, see: <https://docs.microsoft.com/en-us/windows-hardware/drivers/display/container-non-dx#driver-modifications-to-registry-and-file-paths>)
type RuntimeFile struct {

	// The relative path to the file in the driver store
	SourcePath string `mapstructure:"source"`

	// The filename that the file should be given when copied to the destination directory
	DestinationFilename string `mapstructure:"destination"`
}
