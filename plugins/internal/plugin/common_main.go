//go:build windows

package plugin

import (
	"log"
	"os"
	"os/signal"
	"strings"
	"syscall"

	"github.com/tensorworks/directx-device-plugins/plugins/internal/discovery"
	"go.uber.org/zap"
)

// The version number for the plugin
const version = "0.0.1"

func CommonMain(pluginName string, resourceName string, filter discovery.DeviceFilter) {

	// Create a logger that prints debug and higher verbosity level messages
	logger, err := zap.NewDevelopment()
	if err != nil {
		log.Fatalln("Error: failed to create the logger:", err)
	}

	// Sugar the logger
	sugar := logger.Sugar()
	defer sugar.Sync()

	// Log the plugin name and version
	sugar.Infof("Kubernetes device plugin for %s, version %s", strings.ToUpper(pluginName), version)

	// Load the plugin configuration data
	config, err := LoadConfig(pluginName, sugar)
	if err != nil {
		sugar.Errorf("Error: failed to load the device plugin configuration: %v", err)
		return
	}

	// Create the device plugin and start the device watcher
	server, err := NewDevicePlugin(pluginName, version, resourceName, filter, config, sugar)
	if err != nil {
		sugar.Errorf("Error: failed to create the device plugin: %v", err)
		return
	}

	//Ensure the plugin is destroyed and the device watcher stopped when we complete execution
	defer server.Destroy()

	// Attempt to start the plugin's gRPC server
	if err := server.StartServer(); err != nil {
		sugar.Errorf("Error: failed to start the gRPC server: %v", err)
		return
	}

	// Ensure we perform a graceful shutdown of the gRPC server before we destroy the plugin
	defer server.StopServer()

	// Attempt to register the device plugin with the Kubelet
	if err := server.RegisterWithKubelet(); err != nil {
		sugar.Errorf("Error: failed to register the device plugin with the Kubelet: %v", err)
		return
	}

	// Wire up a signal handler to receive shutdown requests
	signals := make(chan os.Signal, 1)
	signal.Notify(signals, syscall.SIGINT, syscall.SIGTERM)

	// Serve requests until we receive a shutdown request or an error occurs
	sugar.Info("Serving until a shutdown request is received")
	for {
		select {
		case sig := <-signals:
			sugar.Infow("Received signal", "signal", sig)
			return

		case err := <-server.Errors:
			sugar.Errorf("Error: %v", err)
			return
		}
	}
}
