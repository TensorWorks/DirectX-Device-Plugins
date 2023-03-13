#pragma once
#include "DeviceFilter.h"

#define DLLEXPORT __declspec(dllexport)

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointer type for DeviceDiscovery instances
typedef void* DeviceDiscoveryInstance;

// Returns the version string for the device discovery library
DLLEXPORT const wchar_t* GetDiscoveryLibraryVersion();

// Disables verbose logging for the device discovery library (this is the default)
DLLEXPORT void DisableDiscoveryLogging();

// Enables verbose logging for the device discovery library
DLLEXPORT void EnableDiscoveryLogging();

// Creates a new DeviceDiscovery instance
DLLEXPORT DeviceDiscoveryInstance CreateDeviceDiscoveryInstance();

// Frees the memory for a DeviceDiscovery instance
DLLEXPORT void DestroyDeviceDiscoveryInstance(DeviceDiscoveryInstance instance);

// Retrieves the error message for the last operation performed by the DeviceDiscovery instance.
// If the last operation succeeded then an empty string will be returned.
DLLEXPORT const wchar_t* DeviceDiscovery_GetLastErrorMessage(DeviceDiscoveryInstance instance);

// Determines whether the current device list is stale and needs to be refreshed by performing device discovery again
DLLEXPORT int DeviceDiscovery_IsRefreshRequired(DeviceDiscoveryInstance instance);

// Performs device discovery. Returns 0 on success and -1 on failure.
// Call GetLastErrorMessage to retrieve the error details for a failure.
DLLEXPORT int DeviceDiscovery_DiscoverDevices(DeviceDiscoveryInstance instance, int filter, int includeIntegrated, int includeDetachable);

// Returns the number of devices found by the last device discovery, or -1 if device discovery has not been performed
DLLEXPORT int DeviceDiscovery_GetNumDevices(DeviceDiscoveryInstance instance);

DLLEXPORT long long DeviceDiscovery_GetDeviceAdapterLUID(DeviceDiscoveryInstance instance, unsigned int device);

// Returns the unique ID of the device with the specified index, or a NULL pointer if the specified device index is invalid
DLLEXPORT const wchar_t* DeviceDiscovery_GetDeviceID(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT const wchar_t* DeviceDiscovery_GetDeviceDescription(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT const wchar_t* DeviceDiscovery_GetDeviceDriverRegistryKey(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT const wchar_t* DeviceDiscovery_GetDeviceDriverStorePath(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT const wchar_t* DeviceDiscovery_GetDeviceLocationPath(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT const wchar_t* DeviceDiscovery_GetDeviceVendor(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT int DeviceDiscovery_GetNumRuntimeFiles(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT const wchar_t* DeviceDiscovery_GetRuntimeFileSource(DeviceDiscoveryInstance instance, unsigned int device, unsigned int file);

DLLEXPORT const wchar_t* DeviceDiscovery_GetRuntimeFileDestination(DeviceDiscoveryInstance instance, unsigned int device, unsigned int file);

DLLEXPORT int DeviceDiscovery_GetNumRuntimeFilesWow64(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT const wchar_t* DeviceDiscovery_GetRuntimeFileSourceWow64(DeviceDiscoveryInstance instance, unsigned int device, unsigned int file);

DLLEXPORT const wchar_t* DeviceDiscovery_GetRuntimeFileDestinationWow64(DeviceDiscoveryInstance instance, unsigned int device, unsigned int file);

DLLEXPORT int DeviceDiscovery_IsDeviceIntegrated(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT int DeviceDiscovery_IsDeviceDetachable(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT int DeviceDiscovery_DoesDeviceSupportDisplay(DeviceDiscoveryInstance instance, unsigned int device);

DLLEXPORT int DeviceDiscovery_DoesDeviceSupportCompute(DeviceDiscoveryInstance instance, unsigned int device);

#ifdef __cplusplus
} // extern "C"


#include <stdexcept>
#include <string>

// API wrapper classes for C++ clients

class DeviceDiscoveryException
{
	public:
		DeviceDiscoveryException(const wchar_t* message) {
			this->message = message;
		}
		
		DeviceDiscoveryException(const DeviceDiscoveryException& other) = default;
		DeviceDiscoveryException(DeviceDiscoveryException&& other) = default;
		DeviceDiscoveryException& operator=(const DeviceDiscoveryException& other) = default;
		DeviceDiscoveryException& operator=(DeviceDiscoveryException&& other) = default;
		
		std::wstring what() const {
			return this->message;
		}
		
	private:
		std::wstring message;
};

class DeviceDiscovery
{
	private:
		DeviceDiscoveryInstance instance;
	
	public:
		
		inline DeviceDiscovery() {
			this->instance = CreateDeviceDiscoveryInstance();
		}
		
		inline ~DeviceDiscovery()
		{
			DestroyDeviceDiscoveryInstance(this->instance);
			this->instance = nullptr;
		}
		
		inline const wchar_t* GetLastErrorMessage() {
			return DeviceDiscovery_GetLastErrorMessage(this->instance);
		}
		
		inline bool IsRefreshRequired() {
			return DeviceDiscovery_IsRefreshRequired(this->instance);
		}
		
		#define THROW_IF_ERROR(sentinel) if (result == sentinel) { throw DeviceDiscoveryException(DeviceDiscovery_GetLastErrorMessage(this->instance)); }
		
		inline bool DiscoverDevices(DeviceFilter filter, bool includeIntegrated, bool includeDetachable)
		{
			int result = DeviceDiscovery_DiscoverDevices(this->instance, static_cast<int>(filter), includeIntegrated, includeDetachable);
			THROW_IF_ERROR(-1);
			return (result == 0);
		}
		
		inline int GetNumDevices()
		{
			int result = DeviceDiscovery_GetNumDevices(this->instance);
			THROW_IF_ERROR(-1);
			return result;
		}
		
		inline long long GetDeviceAdapterLUID(unsigned int device)
		{
			long long result = DeviceDiscovery_GetDeviceAdapterLUID(this->instance, device);
			THROW_IF_ERROR(-1);
			return result;
		}
		
		inline const wchar_t* GetDeviceID(unsigned int device)
		{
			const wchar_t* result = DeviceDiscovery_GetDeviceID(this->instance, device);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline const wchar_t* GetDeviceDescription(unsigned int device)
		{
			const wchar_t* result = DeviceDiscovery_GetDeviceDescription(this->instance, device);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline const wchar_t* GetDeviceDriverRegistryKey(unsigned int device)
		{
			const wchar_t* result = DeviceDiscovery_GetDeviceDriverRegistryKey(this->instance, device);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline const wchar_t* GetDeviceDriverStorePath(unsigned int device)
		{
			const wchar_t* result = DeviceDiscovery_GetDeviceDriverStorePath(this->instance, device);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline const wchar_t* GetDeviceLocationPath(unsigned int device)
		{
			const wchar_t* result = DeviceDiscovery_GetDeviceLocationPath(this->instance, device);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline const wchar_t* GetDeviceVendor(unsigned int device)
		{
			const wchar_t* result = DeviceDiscovery_GetDeviceVendor(this->instance, device);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline int GetNumRuntimeFiles(unsigned int device)
		{
			int result = DeviceDiscovery_GetNumRuntimeFiles(this->instance, device);
			THROW_IF_ERROR(-1);
			return result;
		}
		
		inline const wchar_t* GetRuntimeFileSource(unsigned int device, unsigned int file)
		{
			const wchar_t* result = DeviceDiscovery_GetRuntimeFileSource(this->instance, device, file);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline const wchar_t* GetRuntimeFileDestination(unsigned int device, unsigned int file)
		{
			const wchar_t* result = DeviceDiscovery_GetRuntimeFileDestination(this->instance, device, file);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline int GetNumRuntimeFilesWow64(unsigned int device)
		{
			int result = DeviceDiscovery_GetNumRuntimeFilesWow64(this->instance, device);
			THROW_IF_ERROR(-1);
			return result;
		}
		
		inline const wchar_t* GetRuntimeFileSourceWow64(unsigned int device, unsigned int file)
		{
			const wchar_t* result = DeviceDiscovery_GetRuntimeFileSourceWow64(this->instance, device, file);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline const wchar_t* GetRuntimeFileDestinationWow64(unsigned int device, unsigned int file)
		{
			const wchar_t* result = DeviceDiscovery_GetRuntimeFileDestinationWow64(this->instance, device, file);
			THROW_IF_ERROR(nullptr);
			return result;
		}
		
		inline bool IsDeviceIntegrated(unsigned int device)
		{
			int result = DeviceDiscovery_IsDeviceIntegrated(this->instance, device);
			THROW_IF_ERROR(-1);
			return result;
		}
		
		inline bool IsDeviceDetachable(unsigned int device)
		{
			int result = DeviceDiscovery_IsDeviceDetachable(this->instance, device);
			THROW_IF_ERROR(-1);
			return result;
		}
		
		inline bool DoesDeviceSupportDisplay(unsigned int device)
		{
			int result = DeviceDiscovery_DoesDeviceSupportDisplay(this->instance, device);
			THROW_IF_ERROR(-1);
			return result;
		}
		
		inline bool DoesDeviceSupportCompute(unsigned int device)
		{
			int result = DeviceDiscovery_DoesDeviceSupportCompute(this->instance, device);
			THROW_IF_ERROR(-1);
			return result;
		}
		
		#undef THROW_IF_ERROR
};

#endif
