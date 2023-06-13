Param (
	[parameter(HelpMessage = "Remove existing resources created by a previous run")]
	[switch] $Clean,
	
	[parameter(HelpMessage = "The AWS region in which to deploy resources")]
	$Region = 'us-east-1',
	
	[parameter(HelpMessage = "The name to use for the custom worker node AMI")]
	$AmiName = 'eks-worker-node',
	
	[parameter(HelpMessage = "The name to use for the EKS cluster")]
	$ClusterName = 'demo-cluster'
)


# Halt execution if we encounter an error
$ErrorActionPreference = 'Stop'


# Replaces the placeholders in a template file with values and writes the output to a new file
function FillTemplate
{
	Param (
		$Template,
		$Rendered,
		$Values
	)
	
	$filled = Get-Content -Path $Template -Raw
	$Values.GetEnumerator() | ForEach-Object {
		$filled = $filled.Replace($_.Key, $_.Value)
	}
	Set-Content -Path $Rendered -Value $filled -NoNewline
}

# Represents the output of a native process
class ProcessOutput
{
	ProcessOutput([string] $stdout, [string] $stderr)
	{
		$this.StandardOutput = $stdout
		$this.StandardError = $stderr
	}
	
	[string] $StandardOutput
	[string] $StandardError
}

# Helper functions for executing native commands
class ExecutionHelpers
{
	# Escapes command-line arguments for passing to a native command
	static [string] EscapeArguments([string[]] $arguments)
	{
		$escaped = @()
		
		foreach ($arg in $arguments)
		{
			if ($arg.Contains(' ')) {
				$escaped += @("`"$arg`"")
			}
			else {
				$escaped += @($arg)
			}
		}
		
		return $escaped -join ' '
	}
	
	# Executes a command and throws an error if it returns a non-zero exit code
	static [ProcessOutput] RunCommand([string] $command, [string[]] $arguments, [bool] $captureStdOut, [bool] $captureStdErr)
	{
		# Log the command
		$escapedArgs = [ExecutionHelpers]::EscapeArguments($arguments)
		$formatted = "[$command $escapedArgs]"
		Write-Host "$formatted" -ForegroundColor DarkYellow
		
		# Execute the command and wait for it to complete, retrieving the exit code, stdout and stderr
		$info = New-Object System.Diagnostics.ProcessStartInfo
		$info.FileName = $command
		$info.Arguments = $escapedArgs
		$info.RedirectStandardError = $captureStdErr
		$info.RedirectStandardOutput = $captureStdOut
		$info.UseShellExecute = $false
		$info.WorkingDirectory = (Get-Location).ToString()
		$process = New-Object System.Diagnostics.Process
		$process.StartInfo = $info
		$process.Start()
		# Start reading the output before waiting for the process to exit
		# https://stackoverflow.com/a/139604
		$stdout = if ($captureStdOut) { $process.StandardOutput.ReadToEnd() } else { '' }
		$stderr = if ($captureStdErr) { $process.StandardError.ReadToEnd() } else { '' }
		$process.WaitForExit()
		$exitCode = $process.ExitCode
		
		# If the command terminated with a non-zero exit code then throw an error
		if ($exitCode -ne 0) {
			throw "Command $formatted terminated with exit code $exitCode, stdout $stdout and stderr $stderr"
		}
		
		# Return the output
		return [ProcessOutput]::new($stdout, $stderr)
	}
	
	# Do not capture stdout and stderr of child processes unless the caller explicitly requests it
	static [void] RunCommand([string] $command, [string[]] $arguments) {
		[ExecutionHelpers]::RunCommand($command, $arguments, $false, $false)
	}
	
	# Tests whether the specified command exists, by attempting to execute it with the supplied arguments
	static [bool] CommandExists([string] $command, [string[]] $testArguments)
	{
		try
		{
			[ExecutionHelpers]::RunCommand($command, $testArguments, $true, $true)
			return $true
		}
		catch {
			return $false
		}
	}
}

# Represents the Packer manifest data for our EKS worker node AMI
class PackerManifest
{
	PackerManifest([string] $path) {
		$this.ManifestPath = $path
	}
	
	[bool] Exists() {
		return (Test-Path -Path $this.ManifestPath)
	}
	
	[void] Parse()
	{
		# Parse the Packer manifest JSON and validate the AMI details
		$manifestDetails = Get-Content -Path $this.ManifestPath -Raw | ConvertFrom-Json
		$amiDetails = ($manifestDetails.builds[0].artifact_id -split ':')
		if ($amiDetails.Length -lt 2) {
			throw "Malformed 'artifact_id' field in Packer build manifest: '$amiDetails'"
		}
		
		# Extract the region and AMI ID
		$this.AmiRegion = $amiDetails[0]
		$this.AmiID = $amiDetails[1]
		
		# If the manifest data doesn't contain the snapshot ID for the AMI then populate it
		$this.SnapshotID = $manifestDetails.builds[0].custom_data.snapshot_id
		if ($this.SnapshotID.Length -lt 1)
		{
			# Attempt to retrieve the snapshot ID from the AWS API
			Write-Host 'Retrieving the snapshot ID for the AMI...' -ForegroundColor Green
			$queryOutput = [ExecutionHelpers]::RunCommand('aws', @('ec2', 'describe-images', "--region=$($this.AmiRegion)", "--image-ids=$($this.AmiID)"), $true, $true)
			$snapshotDetails = $queryOutput.StandardOutput | ConvertFrom-Json
			$this.SnapshotID = $snapshotDetails.Images[0].BlockDeviceMappings[0].Ebs.SnapshotId
			if ($amiDetails.Length -lt 1) {
				throw "Failed to retrieve snapshot ID for AMI: '$this.AmiID'"
			}
			
			# Inject the snapshot ID into the manifest data
			$manifestDetails.builds[0].custom_data.snapshot_id = $this.SnapshotID
			
			# Write the updated manifest data back to the JSON file
			$manifestJson = ConvertTo-Json $manifestDetails -Depth 32
			Set-Content -Path $this.ManifestPath -Value $manifestJson -NoNewline
		}
	}
	
	[void] Delete()
	{
		# De-register the AMI
		[ExecutionHelpers]::RunCommand('aws', @('ec2', 'deregister-image', "--region=$($this.AmiRegion)", "--image-id=$($this.AmiID)"))
		
		# Remove the snapshot
		[ExecutionHelpers]::RunCommand('aws', @('ec2', 'delete-snapshot', "--region=$($this.AmiRegion)", "--snapshot-id=$($this.SnapshotID)"))
		
		# Delete the manifest JSON file
		Remove-Item -Force $this.ManifestPath
	}
	
	[string] $ManifestPath
	[string] $AmiID
	[string] $AmiRegion
	[string] $SnapshotID
}

# Represents an EKS cluster managed by eksctl
class EksCluster
{
	EksCluster([string] $name) {
		$this.Name = $name
	}
	
	[bool] Exists()
	{
		try
		{
			[ExecutionHelpers]::RunCommand('eksctl', @('get', 'cluster', "--name=$($this.Name)", "--region=$($global:Region)"), $true, $true)
			return $true
		}
		catch {
			return $false
		}
	}
	
	[void] Create([string] $yamlFile) {
		[ExecutionHelpers]::RunCommand('eksctl', @('create', 'cluster', '-f', $yamlFile.Replace('\', '/')))
	}
	
	[void] Delete() {
		[ExecutionHelpers]::RunCommand('eksctl', @('delete', 'cluster', "--name=$($this.Name)", "--region=$($global:Region)"))
	}
	
	[string] $Name
}


# Verify that all of the native commands we require are available
$requiredCommands = @{
	'the AWS CLI' = [ExecutionHelpers]::CommandExists('aws', @('--version'));
	'eksctl' = [ExecutionHelpers]::CommandExists('eksctl', @('version'));
	'kubectl' = [ExecutionHelpers]::CommandExists('kubectl', @('help'));
	'HashiCorp Packer' = [ExecutionHelpers]::CommandExists('packer', @('version'))
}
foreach ($command in $requiredCommands.GetEnumerator())
{
	if ($command.Value -eq $false) {
		throw "Error: $($command.Name) must be installed to run this script!"
	}
}

# Resolve the path to the Packer manifest file and create a helper object to represent the manifest data
$packerDir = "$PSScriptRoot\node"
$packerManifest = [PackerManifest]::new("$packerDir\manifest.json")

# Create a helper object to represent our test EKS cluster
$eksCluster = [EksCluster]::new($global:ClusterName)

# Determine whether we are removing existing resources created by a previous run
if ($Clean)
{
	# Remove the EKS cluster if it exists
	if ($eksCluster.Exists())
	{
		Write-Host 'Removing existing EKS cluster...' -ForegroundColor Green
		$eksCluster.Delete()
	}
	
	# Delete the AMI and its accompanying snapshot if they exist
	if ($packerManifest.Exists())
	{
		Write-Host 'Removing AMI and its accompanying snapshot...' -ForegroundColor Green
		$packerManifest.Parse()
		$packerManifest.Delete()
	}
	
	Exit
}

# Build the custom worker node AMI if it doesn't already exist
if ($packerManifest.Exists() -eq $false)
{
	# Populate the Packer template
	$packerfile = "$packerDir\eks-worker-node.pkr.hcl"
	FillTemplate `
		-Template "$packerDir\eks-worker-node.pkr.hcl.template" `
		-Rendered $packerfile `
		-Values @{
			'__AWS_REGION__' = $global:Region;
			'__AMI_NAME__' = $global:AmiName
		}
	
	# Build the AMI
	Write-Host 'Building the EKS custom worker node AMI...' -ForegroundColor Green
	Push-Location "$packerDir"
	[ExecutionHelpers]::RunCommand('packer', @('init', 'eks-worker-node.pkr.hcl'))
	[ExecutionHelpers]::RunCommand('packer', @('build', 'eks-worker-node.pkr.hcl'))
	Pop-Location
}

# Parse the Packer manifest JSON and validate the AMI details
$packerManifest.Parse()

# Populate the cluster template YAML with the values for the AMI
$clusterDir = "$PSScriptRoot\cluster"
$configFile = "$clusterDir\test-cluster.yml"
FillTemplate `
	-Template "$clusterDir\test-cluster.template" `
	-Rendered $configFile `
	-Values @{
		'__CLUSTER_NAME__' = $global:ClusterName;
		'__AWS_REGION__' = $packerManifest.AmiRegion;
		'__AMI_ID__' = $packerManifest.AmiID
	}

# Deploy the test EKS cluster if it doesn't already exist
if ($eksCluster.Exists() -eq $false)
{
	Write-Host 'Deploying a test EKS cluster with a Windows worker node group using the custom AMI...' -ForegroundColor Green
	$eksCluster.Create($configFile)
}

# Deploy the device plugin DaemonSets to the test cluster
Write-Host 'Deploying the DirectX device plugin DaemonSets to the test EKS cluster...' -ForegroundColor Green
$deploymentsYaml = "$PSScriptRoot\..\..\deployments\default-daemonsets.yml"
[ExecutionHelpers]::RunCommand('kubectl', @('apply', '-f', $deploymentsYaml.Replace('\', '/')))
