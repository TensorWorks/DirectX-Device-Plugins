#pragma once

// Enumerate all devices
#define DEVICEFILTER_ALL 0

// Enumerate devices that support display, irrespective of whether they also support compute
#define DEVICEFILTER_DISPLAY_SUPPORTED 1

// Enumerate devices that support compute, irrespective of whether they also support display
#define DEVICEFILTER_COMPUTE_SUPPORTED 2

// Enumerate devices that support display and do not support compute (e.g. legacy DirectX 11 devices)
#define DEVICEFILTER_DISPLAY_ONLY 3

// Enumerate devices that support compute and do not support display (i.e. compute-only DirectX 12 devices)
#define DEVICEFILTER_COMPUTE_ONLY 4

// Enumerate devices that support both display and compute (i.e. fully-featured DirectX 12 devices)
#define DEVICEFILTER_DISPLAY_AND_COMPUTE 5


#ifdef __cplusplus

#include <string>

// Device filter enum for C++ clients
enum class DeviceFilter : int
{
	AllDevices = DEVICEFILTER_ALL,
	DisplaySupported = DEVICEFILTER_DISPLAY_SUPPORTED,
	ComputeSupported = DEVICEFILTER_COMPUTE_SUPPORTED,
	DisplayOnly = DEVICEFILTER_DISPLAY_ONLY,
	ComputeOnly = DEVICEFILTER_COMPUTE_ONLY,
	DisplayAndCompute = DEVICEFILTER_DISPLAY_AND_COMPUTE
};

// Returns a string representation of a device filter
inline std::wstring DeviceFilterName(DeviceFilter filter)
{
	switch (filter)
	{
		case DeviceFilter::AllDevices:
			return L"AllDevices";
			
		case DeviceFilter::DisplaySupported:
			return L"DisplaySupported";
			
		case DeviceFilter::ComputeSupported:
			return L"ComputeSupported";
			
		case DeviceFilter::DisplayOnly:
			return L"DisplayOnly";
			
		case DeviceFilter::ComputeOnly:
			return L"ComputeOnly";
			
		case DeviceFilter::DisplayAndCompute:
			return L"DisplayAndCompute";
			
		default:
			return L"<Unknown DeviceFilter enum value>";
	}
}

#endif
