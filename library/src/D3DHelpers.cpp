#include "D3DHelpers.h"
#include "ErrorHandling.h"
#include "ObjectHelpers.h"

QueryD3DRegistryInfo::QueryD3DRegistryInfo()
{
	this->Resize(0);
	this->RegistryInfo->PhysicalAdapterIndex = 0;
}

void QueryD3DRegistryInfo::SetFilesystemQuery(D3DDDI_QUERYREGISTRY_TYPE queryType)
{
	ZeroMemory(this->RegistryInfo->ValueName, sizeof(wchar_t) * MAX_PATH);
	this->RegistryInfo->QueryFlags.TranslatePath = 0;
	this->RegistryInfo->QueryType = queryType;
	this->RegistryInfo->ValueType = 0;
}

void QueryD3DRegistryInfo::SetAdapterKeyQuery(wstring_view name, ULONG valueType, bool translatePaths)
{
	memcpy(this->RegistryInfo->ValueName, name.data(), sizeof(wchar_t) * name.size());
	this->RegistryInfo->QueryFlags.TranslatePath = (translatePaths ? 1 : 0);
	this->RegistryInfo->QueryType = D3DDDI_QUERYREGISTRY_ADAPTERKEY;
	this->RegistryInfo->ValueType = valueType;
}

void QueryD3DRegistryInfo::Resize(size_t trailingBuffer)
{
	// Allocate memory for the new struct + buffer
	this->PrivateDataSize = sizeof(D3DDDI_QUERYREGISTRY_INFO) + trailingBuffer;
	auto newData = std::make_unique<uint8_t[]>(this->PrivateDataSize);
	
	// If we have existing struct values then copy them over to the new struct
	if (this->PrivateData) {
		memcpy(newData.get(), this->PrivateData.get(), sizeof(D3DDDI_QUERYREGISTRY_INFO));
	}
	
	// Release the existing data (if any) and update our struct pointer
	this->PrivateData = std::move(newData);
	this->RegistryInfo = reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO*>(this->PrivateData.get());
}

void QueryD3DRegistryInfo::PerformQuery(unique_adapter_handle& adapter)
{
	while (true)
	{
		// Attempt to perform the query
		auto adapterQuery = this->CreateAdapterQuery(adapter);
		auto error = CheckNtStatus(D3DKMTQueryAdapterInfo(&adapterQuery));
		if (error) {
			throw error.Wrap(L"D3DKMTQueryAdapterInfo failed");
		}
		
		// Determine whether we need to resize the trailing buffer and try again
		if (this->RegistryInfo->Status == D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW) {
			this->Resize(this->RegistryInfo->OutputValueSize);
		}
		else {
			return;
		}
	}
}

D3DKMT_QUERYADAPTERINFO QueryD3DRegistryInfo::CreateAdapterQuery(unique_adapter_handle& adapter)
{
	auto adapterQuery = ObjectHelpers::GetZeroedStruct<D3DKMT_QUERYADAPTERINFO>();
	adapterQuery.hAdapter = adapter.get();
	adapterQuery.Type = KMTQAITYPE_QUERYREGISTRY;
	adapterQuery.pPrivateDriverData = this->PrivateData.get();
	adapterQuery.PrivateDriverDataSize = this->PrivateDataSize;
	return adapterQuery;
}
