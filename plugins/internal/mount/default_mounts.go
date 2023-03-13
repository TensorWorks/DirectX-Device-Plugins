//go:build windows

package mount

import (
	"github.com/tensorworks/directx-device-plugins/plugins/internal/discovery"
)

// The default 64-bit runtime mounts that we add for each device vendor, to supplement those specified in the registry
var DefaultMounts = map[string][]*discovery.RuntimeFile{
	VendorNvidia: {
		{
			SourcePath:          "nvidia-smi.exe",
			DestinationFilename: "nvidia-smi.exe",
		},
		{
			SourcePath:          "vulkaninfo-x64.exe",
			DestinationFilename: "vulkaninfo.exe",
		},
	},
}

// The default 32-bit runtime mounts that we add for each device vendor, to supplement those specified in the registry
var DefaultMountsWow64 = map[string][]*discovery.RuntimeFile{
	VendorNvidia: {
		{
			SourcePath:          "vulkaninfo-x86.exe",
			DestinationFilename: "vulkaninfo.exe",
		},
	},
}
