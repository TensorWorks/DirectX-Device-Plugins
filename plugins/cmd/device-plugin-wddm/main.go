//go:build windows

package main

import (
	"github.com/tensorworks/directx-device-plugins/plugins/internal/discovery"
	"github.com/tensorworks/directx-device-plugins/plugins/internal/plugin"
)

func main() {
	plugin.CommonMain("wddm", "directx.microsoft.com/display", discovery.DisplayAndCompute)
}
