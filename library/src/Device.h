#pragma once

#include "Adapter.h"

using std::vector;
using std::wstring;


// Represents an additional file that needs to be copied from the driver store to the system directory in order to use a device with non-DirectX runtimes
// (For details, see: <https://docs.microsoft.com/en-us/windows-hardware/drivers/display/container-non-dx#driver-modifications-to-registry-and-file-paths>)
struct RuntimeFile
{
	RuntimeFile(wstring SourcePath, wstring DestinationFilename)
	{
		this->SourcePath = SourcePath;
		this->DestinationFilename = DestinationFilename;
		
		// If no destination filename was specified then use the filename from the source path
		if (this->DestinationFilename.empty()) {
			this->DestinationFilename = std::filesystem::path(this->SourcePath).filename().wstring();
		}
	}
	
	// The relative path to the file in the driver store
	wstring SourcePath;
	
	// The filename that the file should be given when copied to the destination directory
	wstring DestinationFilename;
};


// Represents the underlying PnP device associated with a DirectX adapter
struct Device
{
	// The DirectX adapter associated with the PnP device
	Adapter DeviceAdapter;
	
	// The unique PNP hardware identifier for the device
	wstring ID;
	
	// A human-readable description of the device (e.g. the model name)
	wstring Description;
	
	// The registry key that contains the driver details for the device
	wstring DriverRegistryKey;
	
	// The absolute path to the directory in the driver store that contains the driver files for the device
	wstring DriverStorePath;
	
	// The path to the physical location of the device in the system
	wstring LocationPath;
	
	// The list of additional files that need to be copied from the driver store to the System32 directory in order to use the device with non-DirectX runtimes
	vector<RuntimeFile> RuntimeFiles;
	
	// The list of additional files that need to be copied from the driver store to the SysWOW64 directory in order to use the device with non-DirectX runtimes
	vector<RuntimeFile> RuntimeFilesWow64;
	
	// The vendor of the device (e.g. AMD, Intel, NVIDIA)
	wstring Vendor;
};
