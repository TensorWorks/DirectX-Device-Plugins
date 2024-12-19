#include "WmiQuery.h"
#include "ErrorHandling.h"
#include "SafeArray.h"

#include <devpropdef.h>
#include <fmt/core.h>

using std::set;
using wil::unique_variant;
using winrt::hstring;

namespace
{
	// Device property key for retrieving the DirectX adapter LUID
	const DEVPROPKEY DEVPKEY_Device_AdapterLuid = {
		{ 0x60b193cb, 0x5276, 0x4d0f, { 0x96, 0xfc, 0xf1, 0x73, 0xab, 0xad, 0x3e, 0xc6 } },
		2
	};
	
	// Formats a DEVPROPKEY as a string in the form "{00000000-0000-0000-0000-000000000000} 0"
	wstring DevPropKeyToString(const DEVPROPKEY& key)
	{
		return fmt::format(
			L"{{{:0>8X}-{:0>4X}-{:0>4X}-{:0>2X}{:0>2X}-{:0>2X}{:0>2X}{:0>2X}{:0>2X}{:0>2X}{:0>2X}}} {}",
			key.fmtid.Data1,
			key.fmtid.Data2,
			key.fmtid.Data3,
			key.fmtid.Data4[0],
			key.fmtid.Data4[1],
			key.fmtid.Data4[2],
			key.fmtid.Data4[3],
			key.fmtid.Data4[4],
			key.fmtid.Data4[5],
			key.fmtid.Data4[6],
			key.fmtid.Data4[7],
			key.pid
		);
	}
	
	// Formats a PnP hardware ID for use in a WQL query
	wstring FormatHardwareID(const DXCoreHardwareID& dxHardwareID)
	{
		// Build a PCI hardware identifier string as per:
		// <https://docs.microsoft.com/en-us/windows-hardware/drivers/install/identifiers-for-pci-devices>
		// and insert a trailing wildcard for the device instance
		return fmt::format(
			L"PCI\\\\VEN_{:0>4X}&DEV_{:0>4X}&SUBSYS_{:0>8X}&REV_{:0>2X}%",
			dxHardwareID.vendorID,
			dxHardwareID.deviceID,
			dxHardwareID.subSysID,
			dxHardwareID.revision
		);
	}
}

WmiQuery::WmiQuery()
{
	// Create a reusable error object
	DeviceDiscoveryError error;
	
	// Generate the string identifier for the DEVPKEY_Device_AdapterLuid device property key
	this->devPropKeyLUID = DevPropKeyToString(DEVPKEY_Device_AdapterLuid);
	
	// Create our IWbemLocator instance
	CatchHresult(error, this->wbemLocator = winrt::create_instance<IWbemLocator>(CLSID_WbemLocator));
	if (error) {
		throw error.Wrap(L"failed to create an IWbemLocator instance");
	}
	
	// Connect to the WMI service and retrieve a service proxy object
	error = CheckHresult(this->wbemLocator->ConnectServer(
		wil::make_bstr(L"ROOT\\CIMV2").get(),
		nullptr,
		nullptr,
		nullptr,
		0,
		nullptr,
		nullptr,
		this->wbemServices.put()
	));
	if (error) {
		throw error.Wrap(L"failed to connect to the WMI service");
	}
	
	// Set the security level for the service proxy
	error = CheckHresult(CoSetProxyBlanket(
		this->wbemServices.get(),
		RPC_C_AUTHN_WINNT,
		RPC_C_AUTHZ_NONE,
		nullptr,
		RPC_C_AUTHN_LEVEL_CALL,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		nullptr,
		EOAC_NONE
	));
	if (error) {
		throw error.Wrap(L"failed to set the security level for the WMI service proxy");
	}
	
	// Retrieve the CIM class definition for the Win32_PnPEntity class
	error = CheckHresult(this->wbemServices->GetObject(
		wil::make_bstr(L"Win32_PnPEntity").get(),
		0,
		nullptr,
		this->pnpEntityClass.put(),
		nullptr
	));
	if (error) {
		throw error.Wrap(L"failed to retrieve the CIM class definition for the Win32_PnPEntity class");
	}
	
	// Retrieve the input parameters class for the `GetDeviceProperties` method of the CIM class definition
	error = CheckHresult(this->pnpEntityClass->GetMethod(L"GetDeviceProperties", 0, this->inputParameters.put(), nullptr));
	if (error) {
		throw error.Wrap(L"failed to retrieve the input parameters class for Win32_PnPEntity::GetDeviceProperties");
	}
}

vector<Device> WmiQuery::GetDevicesForAdapters(const map<int64_t, Adapter>& adapters)
{
	// If we don't have any adapters then don't query WMI
	if (adapters.empty())
	{
		LOG(L"Empty adapter list provided, skipping WMI query");
		return {};
	}
	
	// Gather the unique PnP hardware IDs from the DirectX adapters for use in our WQL query string
	set<wstring> hardwareIDs;
	for (auto const& adapter : adapters) {
		hardwareIDs.insert(FormatHardwareID(adapter.second.HardwareID));
	}
	
	// Build the WQL query string to retrieve the PnP devices associated with the adapters
	wstring query = L"SELECT * FROM Win32_PnPEntity WHERE Present = TRUE AND (";
	int index = 0;
	int last = hardwareIDs.size() - 1;
	for (auto const& id : hardwareIDs)
	{
		query += L"DeviceID LIKE \"" + id + L"\"" + ((index < last) ? L" OR " : L"");
		index++;
	}
	query += L")";
	
	// Log the query string
	LOG(L"Executing WQL query: {}", query);
	
	// Execute the query
	com_ptr<IEnumWbemClassObject> enumerator;
	auto error = CheckHresult(wbemServices->ExecQuery(
		wil::make_bstr(L"WQL").get(),
		wil::make_bstr(query.c_str()).get(),
		0,
		nullptr,
		enumerator.put()
	));
	if (error) {
		throw error.Wrap(L"WQL query execution failed");
	}
	
	// Iterate over the retrieved PnP devices and match them to their corresponding DirectX adapters
	vector<Device> devices;
	for (int index = 0; ; index++)
	{
		// Retrieve the device for the current loop iteration
		ULONG numReturned = 0;
		com_ptr<IWbemClassObject> device;
		auto error = CheckHresult(enumerator->Next(WBEM_INFINITE, 1, device.put(), &numReturned));
		if (error) {
			throw error.Wrap(L"enumerating PnP devices failed");
		}
		if (numReturned == 0) {
			break;
		}
		
		// Extract the details for the device and determine whether it matches any of our adapters
		Device details = this->ExtractDeviceDetails(device);
		auto matchingAdapter = adapters.find(details.DeviceAdapter.InstanceLuid);
		if (matchingAdapter != adapters.end())
		{
			// Log the match
			LOG(L"Matched adapter LUID {} to PnP device {}", details.DeviceAdapter.InstanceLuid, details.ID);
			
			// Replace the device's adapter details with the matching adapter
			details.DeviceAdapter = matchingAdapter->second;
			
			// Include the device in our results
			devices.push_back(details);
		}
	}
	
	return devices;
}

Device WmiQuery::ExtractDeviceDetails(const com_ptr<IWbemClassObject>& device) const
{
	Device details;
	DeviceDiscoveryError error;
	
	// Retrieve the unique PnP device ID of the device
	unique_variant vtDeviceID;
	error = CheckHresult(device->Get(L"DeviceID", 0, &vtDeviceID, nullptr, nullptr));
	if (error) {
		throw error.Wrap(L"failed to retrieve DeviceID property of PnP device");
	}
	details.ID = winrt::to_hstring(vtDeviceID.bstrVal);
	
	// Retrieve the human-readable description of the device
	unique_variant vtDescription;
	error = CheckHresult(device->Get(L"Description", 0, &vtDescription, nullptr, nullptr));
	if (error) {
		throw error.Wrap(L"failed to retrieve Description property of PnP device");
	}
	details.Description = winrt::to_hstring(vtDescription.bstrVal);
	
	// Retrieve the vendor of the device
	unique_variant vtVendor;
	error = CheckHresult(device->Get(L"Manufacturer", 0, &vtVendor, nullptr, nullptr));
	if (error) {
		throw error.Wrap(L"failed to retrieve Manufacturer property of PnP device");
	}
	details.Vendor = winrt::to_hstring(vtVendor.bstrVal);
	
	// Retrieve the object path for the instance so we can call instance methods with it
	unique_variant vtPath;
	error = CheckHresult(device->Get(L"__Path", 0, &vtPath, nullptr, nullptr));
	if (error) {
		throw error.Wrap(L"failed to retrieve __Path property of PnP device");
	}
	
	// Create an instance of the input parameters type for the `GetDeviceProperties` instance method
	com_ptr<IWbemClassObject> inputArgs;
	error = CheckHresult(this->inputParameters->SpawnInstance(0, inputArgs.put()));
	if (error) {
		throw error.Wrap(L"failed to spawn input parameters instance for Win32_PnPEntity::GetDeviceProperties");
	}
	
	// Populate the input parameters with the list of decive property keys we want to retrieve
	unique_variant vtPropertyKeys = SafeArrayFactory::CreateStringArray({
		L"DEVPKEY_Device_Driver",
		L"DEVPKEY_Device_LocationPaths",
		this->devPropKeyLUID
	});
	error = CheckHresult(inputArgs->Put(L"devicePropertyKeys", 0, &vtPropertyKeys, CIM_FLAG_ARRAY | CIM_STRING));
	if (error) {
		throw error.Wrap(L"failed to assign input parameters array for Win32_PnPEntity::GetDeviceProperties");
	}
	
	// Call the `GetDeviceProperties` instance method
	com_ptr<IWbemCallResult> callResult;
	error = CheckHresult(this->wbemServices->ExecMethod(
		vtPath.bstrVal,
		wil::make_bstr(L"GetDeviceProperties").get(),
		0,
		nullptr,
		inputArgs.get(),
		nullptr,
		callResult.put()
	));
	if (error) {
		throw error.Wrap(L"failed to invoke Win32_PnPEntity::GetDeviceProperties()");
	}
	
	// Retrieve the return value
	com_ptr<IWbemClassObject> returnValue;
	error = CheckHresult(callResult->GetResultObject(WBEM_INFINITE, returnValue.put()));
	if (error) {
		throw error.Wrap(L"failed to retrieve return value for Win32_PnPEntity::GetDeviceProperties");
	}
	
	// Extract the device properties array and verify that it matches the expected type
	unique_variant vtPropertiesArray;
	error = CheckHresult(returnValue->Get(L"deviceProperties", 0, &vtPropertiesArray, nullptr, nullptr));
	if (error) {
		throw error.Wrap(L"failed to retrieve deviceProperties property of Win32_PnPEntity::GetDeviceProperties return value");
	}
	if (vtPropertiesArray.vt != (VT_ARRAY | VT_UNKNOWN)) {
		throw CreateError(L"deviceProperties value was not an array of IUnknown objects");
	}
	
	// Iterate over the device properties array
	SafeArrayIterator<IUnknown*> propertiesIterator(vtPropertiesArray.parray);
	for (auto element : propertiesIterator)
	{
		// Cast the property object to an IWbemClassObject
		IWbemClassObject* object = nullptr;
		error = CheckHresult(element->QueryInterface(&object));
		if (error) {
			throw error.Wrap(L"IUnknown::QueryInterface() failed for Win32_PnPDeviceProperty object");
		}
		
		// Retrieve the key name of the property
		unique_variant vtKeyName;
		error = CheckHresult(object->Get(L"KeyName", 0, &vtKeyName, nullptr, nullptr));
		if (error) {
			throw error.Wrap(L"failed to retrieve KeyName property of PnP device property");
		}
		wstring keyName(winrt::to_hstring(vtKeyName.bstrVal));
		
		// Attempt to retrieve the value of the property
		unique_variant data;
		HRESULT result = object->Get(L"Data", 0, &data, nullptr, nullptr);
		if (FAILED(result))
		{
			// The property has no value, so ignore it
			continue;
		}
		
		// Determine which device property we are dealing with
		if (keyName == L"DEVPKEY_Device_Driver")
		{
			// Verify that the device driver value is of the expected type
			if (data.vt != VT_BSTR) {
				throw CreateError(L"DeviceDriver value was not a string");
			}
			
			// Construct the full path to the registry key for the device's driver
			details.DriverRegistryKey =
				L"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\" +
				winrt::to_hstring(data.bstrVal);
		}
		else if (keyName == L"DEVPKEY_Device_LocationPaths")
		{
			// Verify that the LocationPaths array is of the expected type
			if (data.vt != (VT_ARRAY | VT_BSTR)) {
				throw CreateError(L"LocationPaths value was not an array of strings");
			}
			
			// Retrieve the first element from the LocationPaths array
			SafeArrayIterator<BSTR> locationIterator(data.parray);
			details.LocationPath = winrt::to_hstring(*locationIterator.begin());
		}
		else if (keyName == this->devPropKeyLUID)
		{
			// Determine whether the LUID value is represented as a raw 64-bit integer or a string representation
			if (data.vt == VT_I8) {
				details.DeviceAdapter.InstanceLuid = data.llVal;
			}
			else if (data.vt == VT_BSTR)
			{
				// Parse the string back into a 64-bit integer
				details.DeviceAdapter.InstanceLuid = std::stoll(winrt::to_string(data.bstrVal));
			}
			else {
				throw CreateError(L"LUID value was not a 64-bit integer or a string");
			}
		}
	}
	
	return details;
}
