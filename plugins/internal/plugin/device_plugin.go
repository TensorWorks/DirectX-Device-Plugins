//go:build windows

package plugin

import (
	"context"
	"fmt"
	"net"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"

	"github.com/tensorworks/directx-device-plugins/plugins/internal/discovery"
	"github.com/tensorworks/directx-device-plugins/plugins/internal/mount"
)

type DevicePlugin struct {

	// The name of the plugin
	name string

	// The configuration data for the plugin
	config *PluginConfig

	// The Unix socket on which the plugin's gRPC server listens for connections
	endpoint        string
	endpointDeleted *DeletionWatcher

	// The resource name that the plugin advertises to the Kubelet
	resourceName string

	// The device watcher that monitors the available DirectX devices
	watcher *DeviceWatcher

	// The most recent device list received from the device watcher, and a mutex to protect concurrent access
	currentDevices []*discovery.Device
	devicesMutex   sync.Mutex

	// The logger used to log diagnostic information
	logger *zap.SugaredLogger

	// The gRPC server that services requests from the Kubelet
	server *grpc.Server

	// The channel used to trigger a restart of the gRPC server in the event of a Kubelet restart
	restart chan struct{}

	// The channel used to stop the ListAndWatch streaming RPC during server shutdown
	stopListWatch chan struct{}

	// The channel used for reporting errors while the gRPC server is running
	Errors chan error
}

// Creates a new device plugin
func NewDevicePlugin(pluginName string, pluginVersion string, resourceName string, filter discovery.DeviceFilter, config *PluginConfig, logger *zap.SugaredLogger) (*DevicePlugin, error) {

	// Attempt to create a new DeviceWatcher
	watcher, err := NewDeviceWatcher(
		pluginVersion,
		filter,
		config.IncludeIntegrated,
		config.IncludeDetachable,
		config.AdditionalMounts,
		config.AdditionalMountsWow64,
		logger,
	)
	if err != nil {
		return nil, err
	}

	// Verify that device watcher can successfully list devices
	select {
	case <-watcher.Updates:
		logger.Info("Initial device list retrieved successfully")

	case <-watcher.Errors:
		watcher.Destroy()
		return nil, fmt.Errorf("failed to perform device discovery: %v", err)
	}

	// Create a new device plugin instance with the device watcher
	plugin := &DevicePlugin{
		name:            pluginName,
		config:          config,
		endpoint:        "",
		endpointDeleted: nil,
		resourceName:    resourceName,
		watcher:         watcher,
		currentDevices:  []*discovery.Device{},
		devicesMutex:    sync.Mutex{},
		logger:          logger,
		server:          nil,
		restart:         make(chan struct{}, 1),
		stopListWatch:   nil,
		Errors:          make(chan error, 1),
	}

	// Forward any device watcher errors to the plugin's error channel
	go func() {
		for err := range plugin.watcher.Errors {
			plugin.Errors <- err
		}
	}()

	// Restart the plugin's gRPC server and perform plugin registration again in the event of a Kubelet restart
	go func() {
		for range plugin.restart {

			// Restart the gRPC server with a new Unix socket filename since the Kubelet will delete the old one
			if err := plugin.RestartServer(); err != nil {
				plugin.Errors <- err
			}

			// Register the device plugin with the new Kubelet instance
			if err := plugin.RegisterWithKubelet(); err != nil {
				plugin.Errors <- err
			}
		}
	}()

	return plugin, nil
}

// Starts the gRPC server for the device plugin
func (p *DevicePlugin) StartServer() error {

	// Create a new gRPC server instance
	// (Note that this is necessary to support restarts, since a server instance cannot be reused after it has stopped serving)
	p.server = grpc.NewServer()

	// Register our service implementation with the gRPC server
	p.logger.Info("Registering the service implementation with the gRPC server")
	pluginapi.RegisterDevicePluginServer(p.server, p)

	// Append a timestamp to the filename for the gRPC server's Unix socket to ensure it is unique
	p.endpoint = filepath.Join(pluginapi.DevicePluginPathWindows, fmt.Sprintf("%s-%d.sock", p.name, time.Now().UnixMilli()))

	// Attempt to listen for connections on our Unix socket
	p.logger.Infow("Listening on endpoint", "endpoint", p.endpoint)
	listener, err := net.Listen("unix", p.endpoint)
	if err != nil {
		return err
	}

	// Create the shutdown channel for stopping the ListAndWatch streaming RPC
	p.stopListWatch = make(chan struct{})

	// Create a file deletion watcher for our Unix socket
	endpointDeleted, err := WatchForDeletion(p.endpoint)
	if err != nil {
		return err
	}

	// We detect Kubelet restarts by detecting the deletion of our socket
	p.endpointDeleted = endpointDeleted
	go func() {
		for {
			select {

			case err, ok := <-p.endpointDeleted.Errors:
				if !ok {
					p.logger.Info("DeletionWatcher error channel closed")
					return
				}
				p.Errors <- err

			case _, ok := <-p.endpointDeleted.Deleted:
				if !ok {
					p.logger.Info("DeletionWatcher deletion channel closed")
					return
				}

				p.logger.Info("Endpoint deletion detected, triggering a restart of the gRPC server")
				p.restart <- struct{}{}
			}
		}
	}()

	// Start the gRPC server in a new goroutine and send any errors back through our error channel
	go func() {
		p.logger.Info("Starting the gRPC server")
		if err := p.server.Serve(listener); err != nil {
			p.Errors <- err
		}
	}()

	return nil
}

// Gracefully stops the gRPC server for the device plugin
func (p *DevicePlugin) StopServer() {

	// If StopServer() is called before StartServer() then do nothing
	if p.server == nil {
		return
	}

	// Stop the ListAndWatch streaming RPC if it is running
	close(p.stopListWatch)

	// Stop watching our Unix socket for deletion events
	p.endpointDeleted.Cancel()

	// Attempt to perform a graceful shutdown of the server (this will delete the Unix socket)
	p.logger.Info("Gracefully stopping the gRPC server")
	p.server.GracefulStop()
	p.server = nil
}

// Restarts the gRPC server for the device plugin, generating a new Unix socket filename
func (p *DevicePlugin) RestartServer() error {
	p.StopServer()
	return p.StartServer()
}

// Destroys our underlying resources
func (p *DevicePlugin) Destroy() {
	p.watcher.Destroy()
	close(p.restart)
	close(p.Errors)
}

// Registers the device plugin with the Kubelet
func (p *DevicePlugin) RegisterWithKubelet() error {

	// Set a 60 second timeout when attempting to connect to the Kubelet
	ctxConnect, cancelConnect := context.WithTimeout(context.Background(), time.Minute)
	defer cancelConnect()

	// Create a dialler that treats the Kubelet's endpoint as a Unix socket rather than a TCP address
	dialler := grpc.WithContextDialer(func(ctx context.Context, address string) (net.Conn, error) {
		return (&net.Dialer{}).DialContext(ctx, "unix", address)
	})

	// Attempt to connect to the Kubelet's gRPC service using the socket path for Windows
	p.logger.Infow("Connecting to the Kubelet", "endpoint", pluginapi.KubeletSocketWindows)
	conn, err := grpc.DialContext(
		ctxConnect,
		pluginapi.KubeletSocketWindows,
		grpc.WithBlock(),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		dialler,
	)
	if err != nil {
		return fmt.Errorf("failed to connect to the Kubelet's gRPC service: %v", err)
	}
	defer conn.Close()

	// Prepare a registration request
	request := &pluginapi.RegisterRequest{
		Version:      pluginapi.Version,
		Endpoint:     filepath.Base(p.endpoint),
		ResourceName: p.resourceName,
	}

	// Set a 60 second timeout when attempting to register with the Kubelet
	ctxRegister, cancelRegister := context.WithTimeout(context.Background(), time.Minute)
	defer cancelRegister()

	// Create a registration client and attempt to send our registration request
	p.logger.Infow("Sending registration request to the Kubelet", "request", request)
	client := pluginapi.NewRegistrationClient(conn)
	if _, err := client.Register(ctxRegister, request); err != nil {
		return fmt.Errorf("failed to register the device plugin with the Kubelet: %v", err)
	}

	p.logger.Info("Successfully registered the device plugin with the Kubelet")
	return nil
}

func (p *DevicePlugin) GetDevicePluginOptions(ctx context.Context, request *pluginapi.Empty) (*pluginapi.DevicePluginOptions, error) {

	// Instruct the Kubelet not to call the GetPreferredAllocation or PreStartContainer RPCs, since they aren't necessary
	p.logger.Info("GetDevicePluginOptions RPC invoked")
	return &pluginapi.DevicePluginOptions{
		GetPreferredAllocationAvailable: false,
		PreStartRequired:                false,
	}, nil
}

func (p *DevicePlugin) ListAndWatch(request *pluginapi.Empty, stream pluginapi.DevicePlugin_ListAndWatchServer) error {

	// Force a device list refresh to ensure we have an initial list for the Kubelet
	p.logger.Info("ListAndWatch streaming RPC started, refreshing the device list")
	p.watcher.ForceRefresh()

	// Continue sending updates as our device list changes or until shutdown is requested
	for {
		select {

		case <-p.stopListWatch:
			p.logger.Info("Shutdown requested, stopping ListAndWatch streaming RPC")
			return nil

		case <-stream.Context().Done():
			p.logger.Info("Kubelet disconnect detected, stopping ListAndWatch streaming RPC")
			return nil

		case devices := <-p.watcher.Updates:
			p.logger.Infow("Received new device list", "devices", devices)

			// Store the device list
			p.devicesMutex.Lock()
			p.currentDevices = devices
			p.devicesMutex.Unlock()

			// Convert the device discovery devices to Kubernetes device plugin API devices
			kubeletDevices := []*pluginapi.Device{}
			for _, device := range devices {

				// Advertise each device multiple times, as per our multitenancy setting
				for i := uint32(0); i < p.config.Multitenancy; i += 1 {
					kubeletDevices = append(kubeletDevices, &pluginapi.Device{
						ID:     fmt.Sprintf("%s\\%d", device.ID, i),
						Health: pluginapi.Healthy,
					})
				}
			}

			// Send the device list to the Kubelet
			p.logger.Infow("Sending device list to Kubelet", "devices", kubeletDevices)
			stream.Send(&pluginapi.ListAndWatchResponse{
				Devices: kubeletDevices,
			})
		}
	}
}

func (p *DevicePlugin) GetPreferredAllocation(context.Context, *pluginapi.PreferredAllocationRequest) (*pluginapi.PreferredAllocationResponse, error) {

	// This RPC should never be called
	return nil, status.Error(codes.Unimplemented, "GetPreferredAllocation is not implemented")
}

// Retrieves the device with the specified ID
func (p *DevicePlugin) GetDeviceForID(deviceID string) (*discovery.Device, error) {

	// Strip the multitenancy suffix from the device ID
	backslash := strings.LastIndex(deviceID, "\\")
	if backslash == -1 {
		return nil, fmt.Errorf("malformed device ID \"%s\"", deviceID)
	}
	stripped := deviceID[0:backslash]

	// Lock the mutex for the device list
	p.devicesMutex.Lock()
	defer p.devicesMutex.Unlock()

	// Search for a device with the specified ID
	for _, device := range p.currentDevices {
		if device.ID == stripped {
			return device, nil
		}
	}

	return nil, fmt.Errorf("could not find device with ID \"%s\"", stripped)
}

func (p *DevicePlugin) Allocate(ctx context.Context, request *pluginapi.AllocateRequest) (*pluginapi.AllocateResponse, error) {

	p.logger.Infow("Allocate RPC invoked, processing allocation request", "request", request)
	response := &pluginapi.AllocateResponse{}

	// Process each of the container requests
	for _, containerReq := range request.ContainerRequests {

		// Gather the list of requested devices for the container
		devices := []*discovery.Device{}
		for _, deviceID := range containerReq.DevicesIDs {

			// Verify that the requested device exists
			device, err := p.GetDeviceForID(deviceID)
			if err != nil {
				return nil, err
			}

			// Add the device to the list
			devices = append(devices, device)
		}

		// Generate the device specs and runtime file mounts for the requested devices, appending the container response to our overall response
		response.ContainerResponses = append(response.ContainerResponses, &pluginapi.ContainerAllocateResponse{
			Devices: mount.SpecsForDevices(devices),
			Mounts:  mount.MountsForDevices(devices),
		})
	}

	p.logger.Infow("Sending allocation response", "response", response)
	return response, nil
}

func (p *DevicePlugin) PreStartContainer(context.Context, *pluginapi.PreStartContainerRequest) (*pluginapi.PreStartContainerResponse, error) {

	// This RPC should never be called
	return nil, status.Error(codes.Unimplemented, "PreStartContainer is not implemented")
}
