#pragma once

#include "Adapter.h"
#include "Device.h"

using std::map;
using std::vector;
using std::wstring;
using winrt::com_ptr;

// Provides functionality for querying Windows Management Instrumentation (WMI)
class WmiQuery
{
	public:
		
		WmiQuery();
		
		// Retrieves the device details for the underlying PnP devices associated with the supplied DirectX adapters
		vector<Device> GetDevicesForAdapters(const map<int64_t, Adapter>& adapters);
		
	private:
		
		// Extracts the details from a PnP device
		Device ExtractDeviceDetails(const com_ptr<IWbemClassObject>& device) const;
		
		// Our COM objects for communicating with WMI
		com_ptr<IWbemLocator> wbemLocator;
		com_ptr<IWbemServices> wbemServices;
		com_ptr<IWbemClassObject> pnpEntityClass;
		com_ptr<IWbemClassObject> inputParameters;
		
		// The string identifier for the DEVPROPKEY_GPU_LUID device property key
		wstring devPropKeyLUID;
};
