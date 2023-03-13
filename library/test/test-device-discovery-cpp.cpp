#include "DeviceDiscovery.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using std::endl;
using std::vector;
using std::wstring;
using std::wclog;
using std::wcout;

wstring FormatBoolean(bool value) {
	return (value ? L"true" : L"false");
}

int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
	// Gather our command-line arguments
	vector<wstring> args;
	for (int i = 0; i < argc; ++i) {
		args.push_back(argv[i]);
	}
	
	// Enable verbose logging for the device discovery library if it has been requested
	if (std::find(args.begin(), args.end(), L"--verbose") != args.end()) {
		EnableDiscoveryLogging();
	}
	
	try
	{
		// Perform device discovery
		DeviceDiscovery discovery;
		discovery.DiscoverDevices(DeviceFilter::AllDevices, true, true);
		int numDevices = discovery.GetNumDevices();
		wcout << L"DirectX device discovery library version " << GetDiscoveryLibraryVersion() << endl;
		wcout << L"Discovered " << numDevices << L" devices.\n" << endl;
		
		// Print the details for each device
		for (int device = 0; device < numDevices; ++device)
		{
			wcout << L"[Device " << device << L" details]\n\n";
			wcout << L"PnP Hardware ID:     " << discovery.GetDeviceID(device) << L"\n";
			wcout << L"DX Adapter LUID:     " << discovery.GetDeviceAdapterLUID(device) << L"\n";
			wcout << L"Description:         " << discovery.GetDeviceDescription(device) << L"\n";
			wcout << L"Driver Registry Key: " << discovery.GetDeviceDriverRegistryKey(device) << L"\n";
			wcout << L"DriverStore Path:    " << discovery.GetDeviceDriverStorePath(device) << L"\n";
			wcout << L"LocationPath:        " << discovery.GetDeviceLocationPath(device) << L"\n";
			wcout << L"Vendor:              " << discovery.GetDeviceVendor(device) << L"\n";
			wcout << L"Is Integrated:       " << FormatBoolean(discovery.IsDeviceIntegrated(device)) << L"\n";
			wcout << L"Is Detachable:       " << FormatBoolean(discovery.IsDeviceDetachable(device)) << L"\n";
			wcout << L"Supports Display:    " << FormatBoolean(discovery.DoesDeviceSupportDisplay(device)) << L"\n";
			wcout << L"Supports Compute:    " << FormatBoolean(discovery.DoesDeviceSupportCompute(device)) << L"\n";
			
			int numRuntimeFiles = discovery.GetNumRuntimeFiles(device);
			wcout << L"\n" << numRuntimeFiles << L" Additional System32 runtime files:\n";
			for (int file = 0; file < numRuntimeFiles; ++file)
			{
				wcout << L"    "
				     << discovery.GetRuntimeFileSource(device, file) << " => "
				     << discovery.GetRuntimeFileDestination(device, file) << "\n";
			}
			
			int numRuntimeFilesWow64 = discovery.GetNumRuntimeFilesWow64(device);
			wcout << L"\n" << numRuntimeFilesWow64 << L" Additional SysWOW64 runtime files:\n";
			for (int file = 0; file < numRuntimeFilesWow64; ++file)
			{
				wcout << L"    "
				     << discovery.GetRuntimeFileSourceWow64(device, file) << L" => "
				     << discovery.GetRuntimeFileDestinationWow64(device, file) << L"\n";
			}
			
			wcout << endl;
		}
	}
	catch (const DeviceDiscoveryException& err) {
		wclog << L"Error: " << err.what() << endl;
	}
	
	return 0;
}
