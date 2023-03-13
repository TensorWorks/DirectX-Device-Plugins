//go:build windows

package discovery

type DeviceFilter int32

const (
	AllDevices        DeviceFilter = 0
	DisplaySupported  DeviceFilter = 1
	ComputeSupported  DeviceFilter = 2
	DisplayOnly       DeviceFilter = 3
	ComputeOnly       DeviceFilter = 4
	DisplayAndCompute DeviceFilter = 5
)
