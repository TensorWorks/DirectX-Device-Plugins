# Kubernetes Device Plugins for DirectX

This repository contains Kubernetes device plugins for exposing DirectX devices to Windows containers. The plugins include the following features:

- **Individual devices** are mounted based on their unique [PCIe Location Path](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/plan/plan-for-deploying-devices-using-discrete-device-assignment#pcie-location-path), rather than mounting entire classes of devices based on [device interface class](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/overview-of-device-interface-classes) GUIDs

- Plugins are provided for both **graphics devices** and **compute-only devices**

- Support for devices from **all hardware vendors** (e.g. AMD, Intel, NVIDIA, etc.)

- Support for automatically mounting the runtime files required to **use non-DirectX APIs inside containers** (e.g. OpenGL, Vulkan, OpenCL, NVIDIA CUDA, etc.)

- Support for **multitenancy** (i.e. sharing a single device between multiple containers)


## Contents

- [Core components](#core-components)
- [Important notes on support for non-DirectX APIs](#important-notes-on-support-for-non-directx-apis)
- [Usage](#usage)
    - [Prerequisites](#prerequisites)
    - [Cluster setup](#cluster-setup)
    - [Node setup](#node-setup)
    - [Installation](#installation)
    - [Testing the installation](#testing-the-installation)
- [Building from source](#building-from-source)
    - [Build requirements](#build-requirements)
    - [Building the core components](#building-the-core-components)
    - [Building container images](#building-container-images)
    - [Building the examples](#building-the-examples)
- [Known limitations](#known-limitations)
- [Legal](#legal)


## Core components

The source code in this repository consists of three main components:

- **Device Discovery Library:** a native C++/WinRT shared library that interacts with [DXCore](https://learn.microsoft.com/en-us/windows/win32/dxcore/dxcore) and other Windows APIs to enumerate DirectX adapters and retrieve information about with the underlying [Plug and Play (PnP)](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/introduction-to-plug-and-play) hardware device for each adapter. The device discovery library is also responsible for querying Windows registry information for device drivers in order to determine which runtime files need to be mounted into containers to make use of non-DirectX APIs.

- **Device plugin for MCDM:** a Kubernetes device plugin that provides access to DirectX compute-only devices (or "Core devices") that comply with the [Microsoft Compute Driver Model (MCDM)](https://learn.microsoft.com/en-us/windows/win32/direct3d12/core-feature-levels), under the resource name `directx.microsoft.com/compute`. Compute-only devices support the Direct3D 12 Core 1.0 Feature Level for running compute and machine learning workloads with the DirectML API. Compute-only devices may also support other compute APIs such as OpenCL and OpenMP, or vendor-specific compute APIs such as AMD HIP and NVIDIA CUDA.

- **Device plugin for WDDM:** a Kubernetes device plugin that provides access to DirectX display devices that comply with the full [Windows Display Driver Model (WDDM)](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/windows-vista-display-driver-model-design-guide), under the resource name `directx.microsoft.com/display`. Display devices support the full Direct3D 12 feature set for performing both graphics rendering and compute workloads with the Direct2D, Direct3D and DirectML APIs. Display devices may also support other graphics APIs such as OpenGL and Vulkan, compute APIs such as OpenCL and OpenMP, or various vendor-specific APIs for compute (AMD HIP/NVIDIA CUDA), low-level hardware access (AMD ADL/NVIDIA NVAPI), video encoding (AMD AMF/Intel Quick Sync/NVIDIA NVENC), etc.


## Important notes on support for non-DirectX APIs

**DirectX APIs (DirectML for compute-only devices, and Direct2D/Direct3D/DirectML for display devices) are the only APIs guaranteed to be supported by all devices regardless of vendor and device driver version.** Support for other APIs is entirely dependent on vendor support at the device driver level, and may vary significantly between devices from different vendors or even between different versions of an individual device driver. TensorWorks strongly recommends testing the functionality of any non-DirectX APIs that your applications require in order to verify that they are supported by the hardware and device drivers present in your target execution environment.

Kubernetes device plugins are unable to influence scheduling decisions based on factors other than requested resource name and [NUMA node topology](https://kubernetes.io/docs/concepts/extend-kubernetes/compute-storage-net/device-plugins/#device-plugin-integration-with-the-topology-manager). As such, the device plugins for DirectX cannot select devices based on requirements such as a specific hardware vendor, model-specific details such as VRAM capacity, or support for a specific non-DirectX API. If heterogeneous devices are present across different worker nodes in a Kubernetes cluster and applications need to target a specific vendor, device model or API then the recommended mechanism for enforcing these requirements is to [add labels to your worker nodes and use a label selector to control which nodes any given application Pod is scheduled to](https://kubernetes.io/docs/concepts/scheduling-eviction/assign-pod-node/). Some cloud platforms automatically apply node labels that are suitable as a starting point:

- Worker nodes in Azure Kubernetes Service (AKS) node pools whose underlying VM series includes a GPU will be automatically labelled with the `kubernetes.azure.com/accelerator` label, with a value indicating the hardware vendor (e.g. `nvidia` for NVIDIA devices.) See [*Reserved system labels*](https://learn.microsoft.com/en-us/azure/aks/use-labels#reserved-system-labels) from the AKS documentation.

Note that there is no way to target specific device characteristics when heterogeneous devices are present on a single worker node, since the Kubelet will treat all devices with a given NUMA affinity as equivalent and simply [select whichever devices are available](https://github.com/kubernetes/kubernetes/blob/v1.24.2/pkg/kubelet/cm/devicemanager/manager.go#L686) when allocating them to a container. There is also no guaranteed way to prevent an individual container from being allocated a set of heterogeneous devices when requesting multiple DirectX device resources, which would lead to clashes between the runtime file mounts for different device drivers and may result in some non-DirectX APIs becoming non-functional (e.g. if the device drivers for two heterogeneous devices both attempt to mount their own version of `OpenCL.dll` then only one will succeed and OpenCL may not function for the other device.) For these reasons, it is recommended that each worker node in a cluster include only homogeneous devices.


## Usage

### Prerequisites

The following prerequisites are necessary in order to use the Kubernetes device plugins for DirectX:

- Kubernetes v1.22 or newer (v1.23 or newer recommended, see the [cluster setup notes](#cluster-setup) below)
- Windows Server 2022 or newer
- containerd v1.7.0 or newer
- Up-to-date device drivers for all DirectX devices

### Cluster setup

If your cluster is running Kubernetes v1.22 then you will need to enable support for Windows HostProcess containers by setting the `WindowsHostProcessContainers` [feature gate](https://kubernetes.io/docs/reference/command-line-tools-reference/feature-gates/) to true. HostProcess containers are enabled by default in Kubernetes v1.23 and newer.

### Node setup

Windows nodes will need to be configured to use containerd v1.7.0 or newer as the container runtime, and GPU drivers will need to be installed for the DirectX devices that are present on each node.

It is important to note that many managed Kubernetes offerings **do not** provide support for using custom worker node VM images, which precludes the use of the Kubernetes device plugins for DirectX on those platforms until official support is implemented. One managed Kubernetes offering that does support custom worker node VM images is [Amazon EKS](https://aws.amazon.com/eks/), and demo code for running the Kubernetes device plugins for DirectX on EKS is provided in the [cloud/aws](./cloud/aws) subdirectory of this repository.

### Installation

Once one or more compatible worker nodes have been added to a Kubernetes cluster then the device plugins can be installed. The recommended method of installing the Kubernetes device plugins is to use the [provided deployment YAML](./deployments/default-daemonsets.yml), which deploys each plugin as a Kubernetes [DaemonSet](https://kubernetes.io/docs/concepts/workloads/controllers/daemonset/) that runs on every Windows Server 2022 worker node as a [HostProcess](https://kubernetes.io/docs/tasks/configure-pod-container/create-hostprocess-pod/) container:

```bash
kubectl apply -f "https://raw.githubusercontent.com/TensorWorks/directx-device-plugins/main/deployments/default-daemonsets.yml"
```

Configuration options can be passed to the device plugins by either setting environment variable values directly on the Pod spec or by creating and consuming a [ConfigMap](https://kubernetes.io/docs/concepts/configuration/configmap/). Examples of both of these mechanisms are provided:

- [multitenancy-inline.yml](./deployments/multitenancy-inline.yml)
- [multitenancy-configmap.yml](./deployments/multitenancy-configmap.yml)

### Testing the installation

A number of example workloads are provided in the [examples](./examples/) directory to test that the device plugins are allocating and mounting DirectX devices correctly. These examples include basic tests to verify device detection, simple workloads to test that DirectX APIs function correctly, and vendor-specific workloads that test non-DirectX APIs.

The following examples should work when running on all DirectX devices:

- [**device-discovery**](./examples/device-discovery/): runs a test program that uses the device discovery library enumerate DirectX devices and print their details. This is useful for verifying that a container can see the GPUs that were allocated to it. Versions of this example are available for requesting [compute-only devices](./examples/device-discovery/device-discovery-mcdm.yml) and [display devices](./examples/device-discovery/device-discovery-wddm.yml).

- [**directml**](./examples/directml/): runs the [HelloDirectML](https://github.com/microsoft/DirectML/tree/master/Samples/HelloDirectML) "hello world" tensor data copy sample from the [official DirectML samples](https://github.com/microsoft/DirectML/tree/master/Samples). This is useful for verifying that the DirectML API is functioning correctly. Versions of this example are available for requesting [compute-only devices](./examples/directml/directml-mcdm.yml) and [display devices](./examples/directml/directml-wddm.yml).

The following examples will only work when running on GPUs whose device drivers support using the OpenCL API inside a container:

- [**opencl-enum**](./examples/opencl-enum/): runs the [enumopencl](https://github.com/KhronosGroup/OpenCL-SDK/tree/main/samples/core/enumopencl) device enumeration sample from the [official OpenCL samples](https://github.com/KhronosGroup/OpenCL-SDK/tree/main/samples). This is useful for verifying that the OpenCL API can see the devices that were allocated to a container. Versions of this example are available for requesting [compute-only devices](./examples/opencl-enum/opencl-enum-mcdm.yml) and [display devices](./examples/opencl-enum/opencl-enum-wddm.yml).

The following examples will only work when running on GPUs whose device drivers support using the Vulkan API inside a container:

- [**vulkaninfo**](./examples/vulkaninfo/vulkaninfo.yml): runs the [Vulkan Information](https://vulkan.lunarg.com/doc/view/latest/windows/vulkaninfo.html) tool from the [Vulkan SDK](https://vulkan.lunarg.com/), which enumerates Vulkan-compatible devices and prints their details. This is useful for verifying that the Vulkan API is functioning correctly. This example only supports display devices.

The following examples will only work when running on AMD GPUs:

- [**ffmpeg-amf**](./examples/ffmpeg-amf/ffmpeg-amf.yml): uses the [FFmpeg](https://ffmpeg.org/) video transcoding tool to encode a H.264 video stream with the [AMD AMF](https://gpuopen.com/advanced-media-framework/) hardware video encoder. This is useful for verifying that AMF is functioning correctly when running on AMD GPUs. This example only supports display devices.

The following examples will only work when running on Intel GPUs:

- [**ffmpeg-quicksync**](./examples/ffmpeg-quicksync/ffmpeg-quicksync.yml): uses the FFmpeg video transcoding tool to encode a H.264 video stream with the [Intel Quick Sync](https://www.intel.com/content/www/us/en/architecture-and-technology/quick-sync-video/quick-sync-video-general.html) hardware video encoder. This is useful for verifying that Quick Sync is functioning correctly when running on Intel GPUs. This example only supports display devices.

The following examples will only work when running on NVIDIA GPUs:

- [**cuda-devicequery**](./examples/cuda-devicequery/): runs the [deviceQuery](https://github.com/NVIDIA/cuda-samples/tree/master/Samples/1_Utilities/deviceQuery) device enumeration sample from the [official CUDA samples](https://github.com/NVIDIA/cuda-samples). This is useful for verifying that the CUDA API can see the NVIDIA GPUs that were allocated to a container. Versions of this example are available for requesting [compute-only devices](./examples/cuda-devicequery/cuda-devicequery-mcdm.yml) and [display devices](./examples/cuda-devicequery/cuda-devicequery-wddm.yml).

- [**cuda-montecarlo**](./examples/cuda-montecarlo/): runs the [MC_EstimatePiP](https://github.com/NVIDIA/cuda-samples/tree/master/Samples/2_Concepts_and_Techniques/MC_EstimatePiP) Monte Carlo estimation of Pi sample from the [official CUDA samples](https://github.com/NVIDIA/cuda-samples). This is useful for verifying that the CUDA API can run compute kernels when running on NVIDIA GPUs. Versions of this example are available for requesting [compute-only devices](./examples/cuda-montecarlo/cuda-montecarlo-mcdm.yml) and [display devices](./examples/cuda-montecarlo/cuda-montecarlo-wddm.yml).

- [**ffmpeg-nvenc**](./examples/ffmpeg-nvenc/ffmpeg-nvenc.yml): uses the FFmpeg video transcoding tool to encode a H.264 video stream with the [NVIDIA NVENC](https://developer.nvidia.com/nvidia-video-codec-sdk) hardware video encoder. This is useful for verifying that NVENC is functioning correctly when running on NVIDIA GPUs. This example only supports display devices.

- [**nvidia-smi**](./examples/nvidia-smi/): runs the [NVIDIA System Management Interface](https://developer.nvidia.com/nvidia-system-management-interface) tool to enumerate NVIDIA GPUs and print their details. This is useful for verifying that the NVIDIA drivers can see the NVIDIA GPUs that were allocated to a container. Versions of this example are available for requesting [compute-only devices](./examples/nvidia-smi/nvidia-smi-mcdm.yml) and [display devices](./examples/nvidia-smi/nvidia-smi-wddm.yml).

The following examples will only work when running on AMD, Intel or NVIDIA GPUs:

- [**ffmpeg-autodetect**](./examples/ffmpeg-autodetect/ffmpeg-autodetect.yml): attempts to automatically detect the availability of AMD AMF, Intel Quick Sync or NVIDIA NVENC and uses the FFmpeg video transcoding tool to encode a H.264 video stream with the detected hardware video encoder. This is useful for verifying that hardware transcoding is functioning correctly when running on AMD, Intel or NVIDIA GPUs. This example only supports display devices.


## Building from source

### Build requirements

Building the device plugins from source requires the following:

- Windows 10 version 2004 or newer / Windows Server 2022 or newer
- Visual Studio 2019 or newer (including at a minimum the [Desktop development with C++](https://docs.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=vs-2022&preserve-view=true#desktop-development-with-c) workload from the Build Tools or its equivalent for the full IDE)
- Windows 10 SDK version 10.0.18362.0 or newer (included by default with the *Desktop development with C++* workload)
- [CMake](https://cmake.org/) 3.22 or newer
- [Go](https://go.dev/) 1.18 or newer

### Building the core components

To build the device discovery library and the device plugins, simply run the following command from the root of the source tree:

```
build
```

The [build script](./build.ps1) will automatically download a number of external tools (such as the [NuGet CLI](https://docs.microsoft.com/en-us/nuget/reference/nuget-exe-cli-reference), [vcpkg](https://vcpkg.io/en/index.html) and [vswhere](https://github.com/microsoft/vswhere)) to the `external` subdirectory, use CMake and vcpkg to build the device discovery library (caching intermediate build files in the `build` subdirectory), and then use the Go compiler to build the Kubernetes device plugins. Once the build process is complete, you should see a number of binaries in the `bin` subdirectory:

- `device-plugin-mcdm.exe`: the device plugin for MCDM

- `device-plugin-wddm.exe`: the device plugin for WDDM

- `directx-device-discovery.dll`: the device discovery library

- `gen-device-mounts.exe`: a tool that generates flags for mounting devices and their accompanying DLL files into standalone containers without needing to deploy them to a Kubernetes cluster, for use when developing and testing container images

- `query-hcs-capabilities.exe`: a utility to query the version of the Host Compute Service (HCS) schema supported by the operating system

- `test-device-discovery-cpp.exe`: a test program that uses the device discovery library's C/C++ API to enumerate DirectX devices

- `test-device-discovery-go.exe`: a test program that uses the device discovery library's Go API bindings to enumerate DirectX devices

To clean all build artifacts, run the following command:

```
build -clean
```

This empties the `bin` subdirectory and removes the `build` subdirectory in its entirety.

### Building container images

The build script uses the [crane](https://github.com/google/go-containerregistry/tree/main/cmd/crane) image manipulation tool to build container images. Crane interacts directly with remote container registries, which means it does not require a container runtime like containerd or Docker to be installed locally. However, it does require the user to be authenticated with the remote container registry prior to building any images. To authenticate, run the following command (replacing the placeholders for username, password and container registry URL with the appropriate values):

```bash
go run "github.com/google/go-containerregistry/cmd/crane@latest" auth login -u <USERNAME> -p <PASSWORD> <REGISTRY>
```

For example, to authenticate to Docker Hub with the username "[contoso](https://devblogs.microsoft.com/oldnewthing/20061013-05/?p=29393)" and the password "example":

```bash
go run "github.com/google/go-containerregistry/cmd/crane@latest" auth login -u contoso -p example index.docker.io
```

Container images can be built for both the Kubernetes device plugins and for the example workloads by specifying the `-images` flag or the `-examples` flag, respectively, when invoking the build script. Irrespective of the images being built, you will need to use the `-TagPrefix` flag to specify the tag prefix for the built container images and ensure they are pushed to the correct container registry. For most container registries, the tag prefix represents the registry URL and a username or organisation that acts as a namespace for images. For example, to build the images for the device plugins and push them to Docker Hub under the user "contoso", you would run the following command:

```
build -images -TagPrefix=index.docker.io/contoso
```

This will build and push the following container images:

- `index.docker.io/contoso/device-plugin-mcdm:<VERSION>`
- `index.docker.io/contoso/device-plugin-wddm:<VERSION>`

By default, `<VERSION>` is the tag name if you are building a release, or the Git commit hash when not building a release. You can override the version suffix by specifying the `-version` flag when invoking the build script:

```
build -images -TagPrefix=index.docker.io/contoso -version 0.0.0-dev
```

This will build and push the following container images:

- `index.docker.io/contoso/device-plugin-mcdm:0.0.0-dev`
- `index.docker.io/contoso/device-plugin-wddm:0.0.0-dev`

### Building the examples

As stated in the section above, you can build the example applications and their container images by specifying the `-examples` flag when invoking the build script. Just as when building the container images for the device plugins, you will need to use the `-TagPrefix` flag to specify the tag prefix for the built container images, and the version suffix can optionally be controlled via the `-version` flag. For example, to push images to Docker Hub under the user "contoso" with the version suffix "0.0.0-dev":

```
build -examples -TagPrefix=index.docker.io/contoso -version 0.0.0-dev
```

This will build and push the following container images:

- `index.docker.io/contoso/example-cuda-devicequery:0.0.0-dev`
- `index.docker.io/contoso/example-cuda-montecarlo:0.0.0-dev`
- `index.docker.io/contoso/example-device-discovery:0.0.0-dev`
- `index.docker.io/contoso/example-directml:0.0.0-dev`
- `index.docker.io/contoso/example-ffmpeg:0.0.0-dev`

Note that you will need the [NVIDIA CUDA Toolkit](https://developer.nvidia.com/cuda-toolkit) version 11.6 installed in order to build the CUDA examples. If the build script detects that the CUDA Toolkit is not installed then it will skip building these examples.

## Known limitations

TensorWorks is aware of the following limitations that currently apply when using the Kubernetes device plugins for DirectX:

- When running the device plugins on Kubernetes worker nodes with multiple DirectX devices, containers will sometimes see all host devices even when only a single device is mounted. We believe this is a bug in the underlying Host Compute Service (HCS) in Windows Server 2022 and investigation is ongoing. In the meantime, we recommend deploying the device plugins on worker nodes with only a single DirectX device, and limiting device requests to one per Kubernetes Pod.

- The current version of the NVIDIA GPU driver does not appear to support Vulkan inside Windows containers. This is a limitation of the device driver itself, not the Kubernetes device plugins.


## Legal

Copyright &copy; 2022-2023, TensorWorks Pty Ltd. Licensed under the MIT License, see the file [LICENSE](./LICENSE) for details.
