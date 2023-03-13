#pragma once

#include "AdapterEnumeration.h"
#include "Device.h"
#include "DeviceFilter.h"
#include "WmiQuery.h"

using std::wstring;
using std::wstring_view;
using std::unique_ptr;
using std::vector;

class DeviceDiscoveryImp
{
	public:
		
		DeviceDiscoveryImp() {}
		const wchar_t* GetLastErrorMessage() const;
		bool IsRefreshRequired();
		bool DiscoverDevices(DeviceFilter filter, bool includeIntegrated, bool includeDetachable);
		int GetNumDevices();
		long long GetDeviceAdapterLUID(unsigned int device);
		const wchar_t* GetDeviceID(unsigned int device);
		const wchar_t* GetDeviceDescription(unsigned int device);
		const wchar_t* GetDeviceDriverRegistryKey(unsigned int device);
		const wchar_t* GetDeviceDriverStorePath(unsigned int device);
		const wchar_t* GetDeviceLocationPath(unsigned int device);
		const wchar_t* GetDeviceVendor(unsigned int device);
		int GetNumRuntimeFiles(unsigned int device);
		const wchar_t* GetRuntimeFileSource(unsigned int device, unsigned int file);
		const wchar_t* GetRuntimeFileDestination(unsigned int device, unsigned int file);
		int GetNumRuntimeFilesWow64(unsigned int device);
		const wchar_t* GetRuntimeFileSourceWow64(unsigned int device, unsigned int file);
		const wchar_t* GetRuntimeFileDestinationWow64(unsigned int device, unsigned int file);
		int IsDeviceIntegrated(unsigned int device);
		int IsDeviceDetachable(unsigned int device);
		int DoesDeviceSupportDisplay(unsigned int device);
		int DoesDeviceSupportCompute(unsigned int device);
		
	private:
		
		bool HaveDevices() const;
		void SetLastErrorMessage(wstring_view message);
		void ValidateRequestedDevice(unsigned int device);
		
		vector<Device> devices;
		wstring lastError;
		
		unique_ptr<AdapterEnumeration> enumeration;
		unique_ptr<WmiQuery> wmi;
};
