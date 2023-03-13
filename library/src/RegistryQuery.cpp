#include "RegistryQuery.h"
#include "D3DHelpers.h"
#include "ErrorHandling.h"
#include "ObjectHelpers.h"

#include <algorithm>
#include <Windows.h>
#include <Windows.Devices.Display.Core.Interop.h>

map< wstring, vector<wstring> > RegistryQuery::EnumerateMultiStringValues(unique_hkey& key)
{
	map< wstring, vector<wstring> > values;
	
	LSTATUS result = ERROR_SUCCESS;
	for (int i = 0; ; ++i)
	{
		// Receives the type of the enumerated value
		DWORD valueType = 0;
		
		// Receives the name of the enumerated value
		DWORD nameBufsize = 256;
		auto valueName = std::make_unique<wchar_t[]>(nameBufsize);
		
		// Receives the data of the enumerated value
		DWORD dataBufsize = 1024;
		auto valueData = std::make_unique<uint8_t[]>(dataBufsize);
		
		// Retrieve the next value and check to see if we have processed all available values
		result = RegEnumValueW(key.get(), i, valueName.get(), &nameBufsize, nullptr, &valueType, valueData.get(), &dataBufsize);
		if (result == ERROR_NO_MORE_ITEMS) {
			break;
		}
		
		// Report any errors
		auto error = CheckWin32(result);
		if (error) {
			throw error.Wrap(L"RegEnumValueW failed");
		}
		
		// Verify that the value data is of type REG_MULTI_SZ
		wstring name = wstring(valueName.get(), nameBufsize);
		if (valueType != REG_MULTI_SZ) {
			throw CreateError(L"enumerated value was not of type REG_MULTI_SZ: " + name);
		}
		
		// Parse the value data and add it to our mapping
		auto strings = RegistryQuery::ExtractMultiStringValue((wchar_t*)(valueData.get()), dataBufsize);
		values.insert(std::make_pair(name, strings));
	}
	
	return values;
}

vector<wstring> RegistryQuery::ExtractMultiStringValue(const wchar_t* data, size_t numBytes)
{
	vector<wstring> strings;
	
	size_t offset = 0;
	size_t upperBound = numBytes / sizeof(wchar_t);
	while (offset < upperBound)
	{
		// Extract the next string and check that it's not empty
		wstring nextString(data + offset);
		if (nextString.size() == 0) { break; }
		
		// Add the string to our list and proceed to the next one
		strings.push_back(nextString);
		offset += strings.back().size() + 1;
	}
	
	return strings;
}

unique_hkey RegistryQuery::OpenKeyFromString(wstring_view key)
{
	// Our list of supported root keys
	static map<wstring, HKEY> rootKeys = {
		{ L"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT },
		{ L"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG },
		{ L"HKEY_CURRENT_USER", HKEY_CURRENT_USER },
		{ L"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE },
		{ L"HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA },
		{ L"HKEY_USERS", HKEY_USERS }
	};
	
	// Verify that the supplied key path is well-formed
	size_t backslash = key.find_first_of(L"\\");
	if (backslash == wstring_view::npos || backslash >= (key.size()-1)) {
		throw CreateError(L"invalid registry key path: " + wstring(key));
	}
	
	// Split the root key name from the rest of the path
	wstring rootKeyName = wstring(key.substr(0, backslash));
	wstring keyPath = wstring(key.substr(backslash + 1));
	
	// Identify the handle for the specified root key
	auto rootKey = rootKeys.find(rootKeyName);
	if (rootKey == rootKeys.end()) {
		throw CreateError(L"unknown registry root key: " + rootKeyName);
	}
	
	// Attempt to open the key
	unique_hkey keyHandle;
	auto error = CheckWin32(RegOpenKeyExW(rootKey->second, keyPath.c_str(), 0, KEY_READ, keyHandle.put()));
	if (error) {
		throw error.Wrap(L"failed to open registry key " + wstring(key));
	}
	
	return keyHandle;
}

void RegistryQuery::ProcessRuntimeFiles(Device& device, wstring_view key, bool isWow64)
{
	try
	{
		// Determine whether we are adding runtime files to the device's System32 list or SysWOW64 list
		auto& list = (isWow64) ? device.RuntimeFilesWow64 : device.RuntimeFiles;
		
		// Attempt to open the specified registry key and enumerate its REG_MULTI_SZ values
		unique_hkey registryKey = RegistryQuery::OpenKeyFromString(device.DriverRegistryKey + L"\\" + wstring(key));
		auto files = RegistryQuery::EnumerateMultiStringValues(registryKey);
		for (const auto& pair : files)
		{
			if (!pair.second.empty())
			{
				// Construct a RuntimeFile from the string values
				RuntimeFile newFile(pair.second[0], ((pair.second.size() == 2) ? pair.second[1] : L""));
				
				// Check whether the destination filename for the runtime file clashes with an existing file
				auto existing = std::find_if(list.begin(), list.end(), [newFile](RuntimeFile f) {
					return f.DestinationFilename == newFile.DestinationFilename;
				});
				
				// Only add the new runtime file to the list if there's no clash
				if (existing == list.end()) {
					list.push_back(newFile);
				}
				else {
					LOG(L"{}: ignoring runtime file with duplicate destination filename {}", key, newFile.DestinationFilename);
				}
			}
		}
	}
	catch (const DeviceDiscoveryError& err) {
		LOG(L"Could not enumerate runtime files for the {} key: {}", key, err.message);
	}
}

void RegistryQuery::FillDriverDetails(Device& device)
{
	// Log the device ID to provide context for any subsequent log messages and errors
	LOG(L"Querying device driver registry details for device {}", device.ID);
	
	// Attempt to open the DirectX adapter for the device
	auto adapterDetails = ObjectHelpers::GetZeroedStruct<D3DKMT_OPENADAPTERFROMLUID>();
	adapterDetails.AdapterLuid = LuidFromInt64(device.DeviceAdapter.InstanceLuid);
	auto error = CheckNtStatus(D3DKMTOpenAdapterFromLuid(&adapterDetails));
	if (error)
	{
		throw error.Wrap(
			L"D3DKMTOpenAdapterFromLuid failed to open adapter with LUID " +
			std::to_wstring(device.DeviceAdapter.InstanceLuid)
		);
	}
	
	// Ensure we automatically close the adapter handle when we finish
	unique_adapter_handle adapter(adapterDetails.hAdapter);
	
	// Retrieve the path to the driver store directory for the adapter
	QueryD3DRegistryInfo queryDriverStore;
	queryDriverStore.SetFilesystemQuery(D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH);
	queryDriverStore.PerformQuery(adapter);
	device.DriverStorePath = wstring(queryDriverStore.RegistryInfo->OutputString);
	
	// If the driver store path begins with the "\SystemRoot" prefix then expand it
	wstring prefix = L"\\SystemRoot";
	wstring systemRoot = wstring(wil::GetEnvironmentVariableW(L"SystemRoot").get());
	if (device.DriverStorePath.find(prefix, 0) == 0) {
		device.DriverStorePath = device.DriverStorePath.replace(0, prefix.size(), systemRoot);
	}
	
	// Determine whether we're running on the host or inside a container
	// (e.g. when using a client tool to verify that a device has been mounted correctly)
	if (device.DriverStorePath.find(L"HostDriverStore", 0) != wstring::npos)
	{
		// We have no way of enumerating the CopyToVmWhenNewer subkey inside a container, so stop processing here
		LOG(L"Running inside a container, skipping runtime file enumeration");
		return;
	}
	
	// Retrieve the list of additional runtime files that need to be copied to the System32 directory
	RegistryQuery::ProcessRuntimeFiles(device, L"CopyToVmOverwrite", false);
	RegistryQuery::ProcessRuntimeFiles(device, L"CopyToVmWhenNewer", false);
	
	// Retrieve the list of additional runtime files that need to be copied to the SysWOW64 directory
	RegistryQuery::ProcessRuntimeFiles(device, L"CopyToVmOverwriteWow64", true);
	RegistryQuery::ProcessRuntimeFiles(device, L"CopyToVmWhenNewerWow64", true);
}
