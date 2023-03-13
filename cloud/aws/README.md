# Amazon EKS demo deployment

This directory contains scripts that can be used to deploy the Kubernetes device plugins for DirectX to an [Amazon EKS](https://aws.amazon.com/eks/) Kubernetes cluster for demonstration purposes. Note that the deployment created by these scripts **is not intended for production use**, and lacks important functionality such as auto-scaling the Windows node group based on requests for DirectX devices.

The [main deployment script](./deploy.ps1) performs the following steps:

- Builds a custom [Amazon Machine Image (AMI)](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/AMIs.html) based on Windows Server 2022 for use by Kubernetes worker nodes, with the NVIDIA GPU drivers and containerd v1.7.0 installed. The supporting scripts for building the AMI are located in the [node](./node) subdirectory.

- Creates a EKS cluster with a Windows node group of `g4dn.xlarge` instances that is configured to use the custom AMI. The supporting configuration files for creating the cluster are located in the [cluster](./cluster) subdirectory.

- Deploys the Kubernetes device plugins for DirectX to the EKS cluster using the [default HostProcess DaemonSets for the MCDM device plugin and the WDDM device plugin](../../deployments/default-daemonsets.yml).


## Contents

- [Requirements](#requirements)
- [Running the deployment script](#running-the-deployment-script)
- [Testing the cluster](#testing-the-cluster)
- [Cleaning up](#cleaning-up)


## Requirements

To use the deployment scripts, the following requirements must be met:

- The AWS region that you are using needs to have sufficient quota to run at least one `g4dn.xlarge` EC2 instance. To view or change the relevant limit, login to the AWS web console and navigate to the [*Running On-Demand G and VT instances*](https://console.aws.amazon.com/servicequotas/home/services/ec2/quotas/L-DB2E81BA) service quota page. The minimum required value is 4 vCPUs.

- The AWS region that you are using needs to have a default VPC configured with at least one subnet. If you have deleted the default VPC for the target region then you will need to [create a new one](https://docs.aws.amazon.com/vpc/latest/userguide/default-vpc.html#create-default-vpc).

- [Microsoft PowerShell](https://github.com/PowerShell/PowerShell) needs to be installed when running the deployment scripts under Linux or macOS systems. (Under Windows, the built-in Windows PowerShell is used instead.)

- The [AWS CLI](https://docs.aws.amazon.com/cli/) needs to be installed and configured with credentials that permit the creation of AMIs and EKS clusters. For details, see [*Configuring the AWS CLI*](https://docs.aws.amazon.com/cli/latest/userguide/cli-chap-configure.html).

- [eksctl](https://eksctl.io/) version [0.111.0](https://github.com/weaveworks/eksctl/blob/v0.111.0/docs/release_notes/0.110.0.md) or newer needs to be installed (older versions will refuse to create Windows node groups with GPUs).

- [HashiCorp Packer](https://www.packer.io/) needs to be installed.

- [kubectl](https://kubernetes.io/docs/reference/kubectl/) needs to be installed.


## Running the deployment script

Under Windows, run the main deployment script using the following command:

```
deploy.bat
```

Under Linux and macOS, use this command instead:

```bash
pwsh deploy.ps1
```

The following optional flags can be used to control the deployment options:

- `-Region`: specifies the AWS region into which resources will be deployed. The default region is `us-east-1`.

- `-AmiName`: specifies the name to use for the custom worker node AMI. The default name is `eks-worker-node`.

- `-ClusterName`: specifies the name to use for the EKS cluster. The default name is `demo-cluster`.

An example usage of these flags is shown below:

```bash
# Deploys to the Sydney (ap-southeast-2) AWS region and uses custom names for both the AMI and the EKS cluster
pwsh deploy.ps1 -Region "ap-southeast-2" -AmiName "my-custom-ami" -ClusterName "my-test-cluster"
```


## Testing the cluster

Once the EKS cluster has been created, eksctl will configure kubectl to communicate with that cluster by default. This means you can start using kubectl to deploy examples from the top-level [examples](../../examples) directory without the need for any additional configuration steps:

1. The first example you should deploy is the [**device-discovery**](../../examples/device-discovery/) test, which acts as a sanity check to verify that GPUs are being exposed to containers correctly:
    
    ```bash
    kubectl apply -f '../../examples/device-discovery/device-discovery-wddm.yml'
    ```
    
    Once the Job has been created, wait for the Pod to be assigned to a Windows worker node and then run to completion. If the Job finishes with a status of "Succeeded" then you should check the Pod logs to verify that the NVIDIA Tesla T4 GPU is listed in the output. If the Job finishes with a status of "Failed" or if the log output lists zero devices then something has gone wrong. A failure here could indicate an issue with the Kubernetes device plugins for DirectX themselves, or with some aspect of the EKS cluster configuration.

2. Since the EKS worker nodes are using NVIDIA GPUs, the second example you should deploy is the [**nvidia-smi**](../../examples/nvidia-smi/) test, which acts as a sanity check to verify that the NVIDIA GPU drivers are able to communicate with the GPU:
    
    ```bash
    kubectl apply -f '../../examples/nvidia-smi/nvidia-smi-wddm.yml'
    ```
    
    If the Job finishes with a status of "Succeeded" then `nvidia-smi` was able to communicate with the GPU, and you can check the Pod logs to verify that the output is as expected. If the Job finishes with a status of "Failed" then this indicates either an issue with the Kubernetes device plugins for DirectX themselves or with the NVIDIA GPU drivers on the worker node.

3. With these basic sanity checks out of the way, you can then try out any of the other examples that work on NVIDIA GPUs. Since the NVIDIA Tesla T4 GPUs provided by `g4dn.xlarge` EC2 instances support both compute and display, you will need to deploy examples that request a `directx.microsoft.com/display` resource. Note that some examples include YAML files for both compute-only (MCDM) and compute+display (WDDM) requests, so be sure to use the version with a filename suffix of `-wddm.yml`.
    
    Note that if you attempt to run two tests at once, one of them will wait for the other to complete before it can be scheduled. This is because the Windows node group will not automatically scale up in response to requests for DirectX devices, so only one node (and thus one GPU) will be available to allocate to containers at any given time.


## Cleaning up

To delete all previously deployed AWS resources, run the main deployment script with the `-Clean` flag. Under Windows:

```
deploy.bat -Clean
```

Under Linux and macOS:

```bash
pwsh deploy.ps1 -Clean
```

If you specified flags for the AWS region or custom resource names when deploying the resources then be sure to include these flags when deleting them as well. For example:

```bash
# Deletes the AWS resources that were deployed in the example from the earlier section
pwsh deploy.ps1 -Region "ap-southeast-2" -AmiName "my-custom-ami" -ClusterName "my-test-cluster" -Clean
```
