#include "AdapterEnumeration.h"
#include "ErrorHandling.h"
#include "ObjectHelpers.h"

#include <Windows.Devices.Display.Core.Interop.h>

AdapterEnumeration::AdapterEnumeration()
{
	// Create our DXCore adapter factory
	auto error = CheckHresult(DXCoreCreateAdapterFactory(this->adapterFactory.put()));
	if (error) {
		throw error.Wrap(L"DXCoreCreateAdapterFactory failed");
	}
}

void AdapterEnumeration::EnumerateAdapters(const DeviceFilter& filter, bool includeIntegrated, bool includeDetachable)
{
	// Log our enumeration parameters
	LOG(
		L"Enumerating DirectX adapters using parameters: {{ filter:{}, includeIntegrated:{}, includeDetachable:{} }}",
		DeviceFilterName(filter),
		includeIntegrated,
		includeDetachable
	);
	
	// Clear our adapter lists and our set of unique adapters
	this->adapterLists.clear();
	this->uniqueAdapters.clear();
	
	#define ENUMERATE_ADAPTERS(attribute)\
	{\
		GUID attributes[]{ attribute };\
		this->adapterLists.push_back(nullptr); \
		auto error = CheckHresult(this->adapterFactory->CreateAdapterList(_countof(attributes), attributes, this->adapterLists.back().put()));\
		if (error) { \
			throw error.Wrap(L"IDXCoreAdapterFactory::CreateAdapterList() failed for attribute " + wstring(L#attribute));\
		}\
	}
	
	// Enumerate adapters that support Direct3D 11
	if (filter != DeviceFilter::ComputeOnly && filter != DeviceFilter::DisplayAndCompute) {
		ENUMERATE_ADAPTERS(DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS);
	}
	
	// Enumerate adapters that support Direct3D 12
	if (filter != DeviceFilter::ComputeOnly) {
		ENUMERATE_ADAPTERS(DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS);
	}
	
	// Enumerate adapters that support Direct3D 12 Core
	if (filter != DeviceFilter::DisplayOnly) {
		ENUMERATE_ADAPTERS(DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE);
	}
	
	#undef ENUMERATE_ADAPTERS
	
	// Process each of the enumerated adapters and apply our filtering criteria
	for (auto const& adapters : this->adapterLists)
	{
		const uint32_t count = adapters->GetAdapterCount();
		for (uint32_t index = 0; index < count; ++index)
		{
			// Extract the details for the current adapter
			com_ptr<IDXCoreAdapter> adapter;
			auto error = CheckHresult(adapters->GetAdapter(index, adapter.put()));
			if (error) {
				throw error.Wrap(L"IDXCoreAdapterList::GetAdapter() failed for index " + std::to_wstring(index));
			}
			Adapter details = this->ExtractAdapterDetails(adapter);
			
			// Ignore software devices
			if (!details.IsHardware) {
				continue;
			}
			
			// If the adapter does not match our filter mode then ignore it
			if ((filter == DeviceFilter::DisplayOnly && details.SupportsCompute) ||
			    (filter == DeviceFilter::ComputeOnly && details.SupportsDisplay) ||
			    (filter == DeviceFilter::DisplayAndCompute && (!details.SupportsDisplay || !details.SupportsCompute))) {
				continue;
			}
			
			// If the adapter is integrated and we are not including integrated devices then ignore it
			if (details.IsIntegrated && !includeIntegrated) {
				continue;
			}
			
			// If the adapter is detachable and we are not including detachable devices then ignore it
			if (details.IsDetachable && !includeDetachable) {
				continue;
			}
			
			// Add the adapter to our set of unique adapters
			this->uniqueAdapters.insert(std::make_pair(details.InstanceLuid, details));
		}
	}
	
	// Log the list of unique adapter LUIDs
	LOG(L"Enumerated DirectX adapters with LUIDs: {}", FMT(ObjectHelpers::GetMappingKeys(this->uniqueAdapters)));
}

const map<int64_t, Adapter>& AdapterEnumeration::GetUniqueAdapters() const {
	return this->uniqueAdapters;
}

bool AdapterEnumeration::IsStale() const
{
	// If we have not yet performed enumeration then report that our data is stale
	if (this->adapterLists.empty())
	{
		LOG(L"No adapter lists yet, need to perform enumeration");
		return true;
	}
	
	// If any of our adapter lists are stale then our data is stale
	for (auto const& list : this->adapterLists)
	{
		if (list->IsStale())
		{
			LOG(L"Found stale adapter list");
			return true;
		}
	}
	
	return false;
}

Adapter AdapterEnumeration::ExtractAdapterDetails(const com_ptr<IDXCoreAdapter>& adapter) const
{
	Adapter details;
	DeviceDiscoveryError error;
	
	// Extract the adapter LUID and convert it to an int64_t
	LUID instanceLuid;
	error = CheckHresult(adapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &instanceLuid));
	if (error) {
		throw error.Wrap(L"IDXCoreAdapter::GetProperty() failed for property InstanceLuid");
	}
	details.InstanceLuid = Int64FromLuid(instanceLuid);
	
	// Extract the PnP hardware ID information
	error = CheckHresult(adapter->GetProperty(DXCoreAdapterProperty::HardwareID, &details.HardwareID));
	if (error) {
		throw error.Wrap(L"IDXCoreAdapter::GetProperty() failed for property HardwareID");
	}
	
	// Extract the boolean specifying whether the adapter is a hardware device
	error = CheckHresult(adapter->GetProperty(DXCoreAdapterProperty::IsHardware, &details.IsHardware));
	if (error) {
		throw error.Wrap(L"IDXCoreAdapter::GetProperty() failed for property IsHardware");
	}
	
	// Extract the boolean specifying whether the adapter is an integrated GPU
	error = CheckHresult(adapter->GetProperty(DXCoreAdapterProperty::IsIntegrated, &details.IsIntegrated));
	if (error) {
		throw error.Wrap(L"IDXCoreAdapter::GetProperty() failed for property IsIntegrated");
	}
	
	// Extract the boolean specifying whether the adapter is detachable
	error = CheckHresult(adapter->GetProperty(DXCoreAdapterProperty::IsDetachable, &details.IsDetachable));
	if (error) {
		throw error.Wrap(L"IDXCoreAdapter::GetProperty() failed for property IsDetachable");
	}
	
	// Determine whether the adapter supports display
	details.SupportsDisplay =
		adapter->IsAttributeSupported(DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS) ||
		adapter->IsAttributeSupported(DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS);
	
	// Determine whether the adapter supports compute
	details.SupportsCompute = adapter->IsAttributeSupported(DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS);
	
	return details;
}
