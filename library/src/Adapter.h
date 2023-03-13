#pragma once

// Represents a DirectX adapter as enumerated by DXCore
// (For additional details, see: <https://docs.microsoft.com/en-us/windows/win32/dxcore/dxcore_interface/ne-dxcore_interface-dxcoreadapterproperty>)
struct Adapter
{
	inline Adapter() :
		InstanceLuid(0),
		IsHardware(false),
		IsIntegrated(false),
		IsDetachable(false),
		SupportsDisplay(false),
		SupportsCompute(false)
	{}
	
	// The locally unique identifier (LUID) for the adapter
	int64_t InstanceLuid;
	
	// The PnP hardware ID information for the adapter
	DXCoreHardwareID HardwareID;
	
	// Specifies whether the adapter is a hardware device (as opposed to a software device)
	bool IsHardware;
	
	// Specifies whether the adapter is an integrated GPU (as opposed to a discrete GPU)
	bool IsIntegrated;
	
	// Specifies whether the adapter is a detachable device (i.e. the device can be removed at runtime)
	bool IsDetachable;
	
	// Specifies whether the adapter supports display
	// (i.e. supports either the DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS or DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS attributes)
	bool SupportsDisplay;
	
	// Specifies whether the adapter supports compute (i.e. supports the DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE attribute)
	bool SupportsCompute;
};
