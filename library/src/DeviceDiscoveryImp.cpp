#include "DeviceDiscoveryImp.h"
#include "ErrorHandling.h"
#include "RegistryQuery.h"

#include <roapi.h>
#include <stdexcept>

#define RETURN_ERROR(sentinel, message) this->SetLastErrorMessage(message); return sentinel
#define RETURN_SUCCESS(value) this->SetLastErrorMessage(L""); return value

#define VERIFY_DEVICE(sentinel) try { this->ValidateRequestedDevice(device); } catch (const DeviceDiscoveryError& err) { RETURN_ERROR(sentinel, err.message); }
#define VERIFY_FILE() if (file >= files.size()) { RETURN_ERROR(nullptr, L"requested runtime file index is invalid: " + std::to_wstring(file)); }

const wchar_t* DeviceDiscoveryImp::GetLastErrorMessage() const {
	return this->lastError.c_str();
}

bool DeviceDiscoveryImp::IsRefreshRequired()
{
	// Make sure WinRT is initialised for the calling thread
	Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);
	
	// We require a refresh if we have no data or we have stale data
	return (this->HaveDevices()) ? this->enumeration->IsStale() : true;
}

bool DeviceDiscoveryImp::DiscoverDevices(DeviceFilter filter, bool includeIntegrated, bool includeDetachable)
{
	// Make sure WinRT is initialised for the calling thread
	Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);
	
	try
	{
		// If this is the first time we're performing device discovery then create our helper objects
		if (!this->HaveDevices())
		{
			this->enumeration = std::make_unique<AdapterEnumeration>();
			this->wmi = std::make_unique<WmiQuery>();
		}
		
		// Enumerate the DirectX adapters that meet the supplied filtering criteria
		this->enumeration->EnumerateAdapters(filter, includeIntegrated, includeDetachable);
		
		// Retrieve the PnP device details from WMI for each of the enumerated adapters
		this->devices = this->wmi->GetDevicesForAdapters(this->enumeration->GetUniqueAdapters());
		
		// Retrieve the driver details from the registry for each of the devices
		for (auto& device : this->devices) {
			RegistryQuery::FillDriverDetails(device);
		}
		
		RETURN_SUCCESS(true);
	}
	catch (const DeviceDiscoveryError& err) {
		RETURN_ERROR(false, err.Pretty());
	}
	catch (const std::runtime_error& err) {
		RETURN_ERROR(false, winrt::to_hstring(err.what()));
	}
}

int DeviceDiscoveryImp::GetNumDevices()
{
	// Verify that we have a device list
	if (!this->HaveDevices()) {
		RETURN_ERROR(-1, L"attempted to retrieve device count before performing device discovery");
	}
	
	RETURN_SUCCESS(this->devices.size());
}

long long DeviceDiscoveryImp::GetDeviceAdapterLUID(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(-1);
	
	// Retrieve the adapter LUID of the specified device
	RETURN_SUCCESS(this->devices[device].DeviceAdapter.InstanceLuid);
}

const wchar_t* DeviceDiscoveryImp::GetDeviceID(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Retrieve the ID of the specified device
	RETURN_SUCCESS(this->devices[device].ID.c_str());
}

const wchar_t* DeviceDiscoveryImp::GetDeviceDescription(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Retrieve the human-readable description of the specified device
	RETURN_SUCCESS(this->devices[device].Description.c_str());
}

const wchar_t* DeviceDiscoveryImp::GetDeviceDriverRegistryKey(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Retrieve the path of the registry key with the driver details for the specified device
	RETURN_SUCCESS(this->devices[device].DriverRegistryKey.c_str());
}

const wchar_t* DeviceDiscoveryImp::GetDeviceDriverStorePath(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Retrieve the absolute path to the driver store directory for the specified device
	RETURN_SUCCESS(this->devices[device].DriverStorePath.c_str());
}

const wchar_t* DeviceDiscoveryImp::GetDeviceLocationPath(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Retrieve the physical location path of the specified device
	RETURN_SUCCESS(this->devices[device].LocationPath.c_str());
}

const wchar_t* DeviceDiscoveryImp::GetDeviceVendor(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Retrieve the vendor of the specified device
	RETURN_SUCCESS(this->devices[device].Vendor.c_str());
}

int DeviceDiscoveryImp::GetNumRuntimeFiles(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(-1);
	
	// Retrieve the number of additional runtime files for the device
	RETURN_SUCCESS(this->devices[device].RuntimeFiles.size());
}

const wchar_t* DeviceDiscoveryImp::GetRuntimeFileSource(unsigned int device, unsigned int file)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Verify that the requested file entry exists
	const vector<RuntimeFile>& files = this->devices[device].RuntimeFiles;
	VERIFY_FILE();
	
	// Retrieve the source path for the file
	RETURN_SUCCESS(files[file].SourcePath.c_str());
}

const wchar_t* DeviceDiscoveryImp::GetRuntimeFileDestination(unsigned int device, unsigned int file)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Verify that the requested file entry exists
	const vector<RuntimeFile>& files = this->devices[device].RuntimeFiles;
	VERIFY_FILE();
	
	// Retrieve the destination filename for the file
	RETURN_SUCCESS(files[file].DestinationFilename.c_str());
}

int DeviceDiscoveryImp::GetNumRuntimeFilesWow64(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(-1);
	
	// Retrieve the number of additional SysWOW64 runtime files for the device
	RETURN_SUCCESS(this->devices[device].RuntimeFilesWow64.size());
}

const wchar_t* DeviceDiscoveryImp::GetRuntimeFileSourceWow64(unsigned int device, unsigned int file)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Verify that the requested file entry exists
	const vector<RuntimeFile>& files = this->devices[device].RuntimeFilesWow64;
	VERIFY_FILE();
	
	// Retrieve the source path for the file
	RETURN_SUCCESS(files[file].SourcePath.c_str());
}

const wchar_t* DeviceDiscoveryImp::GetRuntimeFileDestinationWow64(unsigned int device, unsigned int file)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(nullptr);
	
	// Verify that the requested file entry exists
	const vector<RuntimeFile>& files = this->devices[device].RuntimeFilesWow64;
	VERIFY_FILE();
	
	// Retrieve the destination filename for the file
	RETURN_SUCCESS(files[file].DestinationFilename.c_str());
}

int DeviceDiscoveryImp::IsDeviceIntegrated(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(-1);
	
	// Determine whether the specified device is an integrated GPU
	RETURN_SUCCESS(this->devices[device].DeviceAdapter.IsIntegrated);
}

int DeviceDiscoveryImp::IsDeviceDetachable(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(-1);
	
	// Determine whether the specified device is detachable
	RETURN_SUCCESS(this->devices[device].DeviceAdapter.IsDetachable);
}

int DeviceDiscoveryImp::DoesDeviceSupportDisplay(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(-1);
	
	// Determine whether the specified device supports display
	RETURN_SUCCESS(this->devices[device].DeviceAdapter.SupportsDisplay);
}

int DeviceDiscoveryImp::DoesDeviceSupportCompute(unsigned int device)
{
	// Verify that the requested device exists
	VERIFY_DEVICE(-1);
	
	// Determine whether the specified device supports compute
	RETURN_SUCCESS(this->devices[device].DeviceAdapter.SupportsCompute);
}

bool DeviceDiscoveryImp::HaveDevices() const {
	return (this->enumeration && this->wmi);
}

void DeviceDiscoveryImp::SetLastErrorMessage(std::wstring_view message) {
	this->lastError = message;
}

void DeviceDiscoveryImp::ValidateRequestedDevice(unsigned int device)
{
	// Verify that we have a device list
	if (!this->HaveDevices()) {
		throw CreateError(L"attempted to retrieve device details before performing device discovery");
	}
	
	// Verify that the specified device index is valid
	if (device >= this->GetNumDevices()) {
		throw CreateError(L"requested device index is invalid: " + std::to_wstring(device));
	}
}
