#pragma once

using std::wstring_view;


// Closes the supplied DirectX adapter handle
inline NTSTATUS CloseAdapter(D3DKMT_HANDLE adapter)
{
	D3DKMT_CLOSEADAPTER close;
	close.hAdapter = adapter;
	return D3DKMTCloseAdapter(&close);
}

// Auto-releasing resource wrapper type for DirectX adapter handles
typedef wil::unique_any<D3DKMT_HANDLE, decltype(&::CloseAdapter), ::CloseAdapter> unique_adapter_handle;


// Encapsulates a D3DDDI_QUERYREGISTRY_INFO struct, along with its trailing buffer for receiving output data
class QueryD3DRegistryInfo
{
	public:
		
		// Use this to access the struct's member fields
		D3DDDI_QUERYREGISTRY_INFO* RegistryInfo;
		
		QueryD3DRegistryInfo();
		
		// Populates the struct fields for querying a filesystem path
		void SetFilesystemQuery(D3DDDI_QUERYREGISTRY_TYPE queryType);
		
		// Populates the struct fields for querying a registry value from the adapter key
		void SetAdapterKeyQuery(wstring_view name, ULONG valueType, bool translatePaths);
		
		// Resizes the trailing buffer
		void Resize(size_t trailingBuffer);
		
		// Performs a registry query against the specified adapter, resizing the trailing buffer to accommodate the output data size as needed
		void PerformQuery(unique_adapter_handle& adapter);
		
	private:
		
		// The underlying data and size for the struct along with its trailing buffer
		std::unique_ptr<uint8_t[]> PrivateData;
		size_t PrivateDataSize;
		
		// Creates a D3DKMT_QUERYADAPTERINFO struct that wraps struct and its trailing buffer
		D3DKMT_QUERYADAPTERINFO CreateAdapterQuery(unique_adapter_handle& adapter);
};
