#pragma once

#include "Adapter.h"
#include "DeviceFilter.h"

using std::map;
using std::vector;
using winrt::com_ptr;

class AdapterEnumeration
{
	public:
		AdapterEnumeration();
		
		// Enumerates the DirectX adapters that meet the specified filtering criteria
		void EnumerateAdapters(const DeviceFilter& filter, bool includeIntegrated, bool includeDetachable);
		
		// Retrieves the list of unique adapters retrieved during the last enumeration operation
		const map<int64_t, Adapter>& GetUniqueAdapters() const;
		
		// Determines whether the list of adapters is stale and needs to be refreshed by performing enumeration again
		bool IsStale() const;
		
	private:
		
		// Extracts the details from a DXCore adapter object
		Adapter ExtractAdapterDetails(const com_ptr<IDXCoreAdapter>& adapter) const;
		
		// Our DXCore adapter factory
		com_ptr<IDXCoreAdapterFactory> adapterFactory;
		
		// Our collection of DXCore adapter lists, used for enumerating adapters with various capabilities
		vector< com_ptr<IDXCoreAdapterList> > adapterLists;
		
		// The list of unique adapters retrieved during the last enumeration operation, keyed by adapter LUID
		map<int64_t, Adapter> uniqueAdapters;
};
