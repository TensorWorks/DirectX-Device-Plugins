#include "DeviceDiscovery.h"
#include "DeviceDiscoveryImp.h"

#define LIBRARY_VERSION L"0.0.1"

#define INSTANCE (reinterpret_cast<DeviceDiscoveryImp*>(instance))

const wchar_t* GetDiscoveryLibraryVersion() {
	return LIBRARY_VERSION;
}

void DisableDiscoveryLogging() {
	spdlog::set_level(spdlog::level::off);
}

void EnableDiscoveryLogging()
{
	spdlog::set_pattern("%^[directx-device-discovery.dll %Y-%m-%dT%T%z]%$ [%s:%# %!] %v", spdlog::pattern_time_type::local);
	spdlog::set_level(spdlog::level::info);
	spdlog::flush_on(spdlog::level::info);
}

DeviceDiscoveryInstance CreateDeviceDiscoveryInstance() {
	return new DeviceDiscoveryImp();
}

void DestroyDeviceDiscoveryInstance(DeviceDiscoveryInstance instance) {
	delete INSTANCE;
}

const wchar_t* DeviceDiscovery_GetLastErrorMessage(DeviceDiscoveryInstance instance) {
	return INSTANCE->GetLastErrorMessage();
}

int DeviceDiscovery_IsRefreshRequired(DeviceDiscoveryInstance instance) {
	return INSTANCE->IsRefreshRequired();
}

int DeviceDiscovery_DiscoverDevices(DeviceDiscoveryInstance instance, int filter, int includeIntegrated, int includeDetachable)
{
	bool success = INSTANCE->DiscoverDevices(static_cast<DeviceFilter>(filter), includeIntegrated, includeDetachable);
	return (success ? 0 : -1);
}

int DeviceDiscovery_GetNumDevices(DeviceDiscoveryInstance instance) {
	return INSTANCE->GetNumDevices();
}

long long DeviceDiscovery_GetDeviceAdapterLUID(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->GetDeviceAdapterLUID(device);
}

const wchar_t* DeviceDiscovery_GetDeviceID(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->GetDeviceID(device);
}

const wchar_t* DeviceDiscovery_GetDeviceDescription(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->GetDeviceDescription(device);
}

const wchar_t* DeviceDiscovery_GetDeviceDriverRegistryKey(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->GetDeviceDriverRegistryKey(device);
}

const wchar_t* DeviceDiscovery_GetDeviceDriverStorePath(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->GetDeviceDriverStorePath(device);
}

const wchar_t* DeviceDiscovery_GetDeviceLocationPath(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->GetDeviceLocationPath(device);
}

const wchar_t* DeviceDiscovery_GetDeviceVendor(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->GetDeviceVendor(device);
}

int DeviceDiscovery_GetNumRuntimeFiles(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->GetNumRuntimeFiles(device);
}

const wchar_t* DeviceDiscovery_GetRuntimeFileSource(DeviceDiscoveryInstance instance, unsigned int device, unsigned int file) {
	return INSTANCE->GetRuntimeFileSource(device, file);
}

const wchar_t* DeviceDiscovery_GetRuntimeFileDestination(DeviceDiscoveryInstance instance, unsigned int device, unsigned int file) {
	return INSTANCE->GetRuntimeFileDestination(device, file);
}

int DeviceDiscovery_GetNumRuntimeFilesWow64(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->GetNumRuntimeFilesWow64(device);
}

const wchar_t* DeviceDiscovery_GetRuntimeFileSourceWow64(DeviceDiscoveryInstance instance, unsigned int device, unsigned int file) {
	return INSTANCE->GetRuntimeFileSourceWow64(device, file);
}

const wchar_t* DeviceDiscovery_GetRuntimeFileDestinationWow64(DeviceDiscoveryInstance instance, unsigned int device, unsigned int file) {
	return INSTANCE->GetRuntimeFileDestinationWow64(device, file);
}

int DeviceDiscovery_IsDeviceIntegrated(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->IsDeviceIntegrated(device);
}

int DeviceDiscovery_IsDeviceDetachable(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->IsDeviceDetachable(device);
}

int DeviceDiscovery_DoesDeviceSupportDisplay(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->DoesDeviceSupportDisplay(device);
}

int DeviceDiscovery_DoesDeviceSupportCompute(DeviceDiscoveryInstance instance, unsigned int device) {
	return INSTANCE->DoesDeviceSupportCompute(device);
}
