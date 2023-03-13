#pragma once

#include "Device.h"

using std::map;
using std::vector;
using std::wstring;
using std::wstring_view;
using wil::unique_hkey;

// Provides functionality for querying the Windows registry
namespace RegistryQuery
{
	// Enumerates the values of the supplied registry key and parses their data as REG_MULTI_SZ
	map< wstring, vector<wstring> > EnumerateMultiStringValues(unique_hkey& key);
	
	// Extracts the individual strings of a REG_MULTI_SZ registry value
	vector<wstring> ExtractMultiStringValue(const wchar_t* data, size_t numBytes);
	
	// Parses a registry key path and opens it using the appropriate root key
	unique_hkey OpenKeyFromString(wstring_view key);
	
	// Enumerates the runtime files for a device as listed under the specified registry key
	void ProcessRuntimeFiles(Device& device, wstring_view key, bool isWow64);
	
	// Queries the registry to retrieve driver-related details for the supplied PnP device
	void FillDriverDetails(Device& device);
}
