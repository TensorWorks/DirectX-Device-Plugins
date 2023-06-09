#!/usr/bin/env python3
# 
# This script automates the generation of `setup.ps1` in the `scripts` subdirectory
# 
import json, re, subprocess, yaml, sys
from pathlib import Path


class Utility:
	
	@staticmethod
	def log(message):
		"""
		Logs a message to stderr
		"""
		print('[generate-setup-script.py]: {}'.format(message), flush=True, file=sys.stderr)
	
	@staticmethod
	def capture(command, **kwargs):
		"""
		Executes the specified command and captures its output
		"""
		
		# Log the command being executed
		Utility.log(command)
		
		# Attempt to execute the specified command
		result = subprocess.run(
			command,
			check = True,
			capture_output = True,
			universal_newlines = True,
			**kwargs
		)
		
		# Return the contents of stdout
		return result.stdout.strip()
	
	@staticmethod
	def writeFile(filename, data):
		"""
		Writes data to the specified file
		"""
		return Path(filename).write_bytes(data.encode('utf-8'))
	
	@staticmethod
	def commentForStep(name):
		"""
		Returns a descriptive comment for the build step with the specified name
		"""
		return {
			
			'ConfigureDirectories':               '# Create each of our directories',
			'DownloadKubernetes':                 '# Download the Kubernetes components',
			'DownloadEKSArtifacts':               '# Download the EKS artifacts archive',
			'ExtractEKSArtifacts':                '# Extract the EKS artifacts archive',
			'MoveEKSArtifacts':                   '# Move the EKS files into place',
			'ExecuteBuildScripts':                '# Perform EKS worker node setup',
			'RemoveEKSArtifactDownloadDirectory': '# Perform cleanup',
			
			'InstallContainers': '\n'.join([
				'# Install the Windows Containers feature',
				'# (Note: this is actually a no-op here, since we install the feature beforehand in startup.ps1)'
			])
			
		}.get(name, None)
	
	@staticmethod
	def parseConstants(constants):
		"""
		Parses an EC2 ImageBuilder component's constants list
		"""
		parsed = {}
		for entry in constants:
			for key, values in entry.items():
				parsed[key] = values['value']
		return parsed
	
	@staticmethod
	def replaceConstants(string, constants):
		"""
		Converts EC2 ImageBuilder constant references to PowerShell variable references
		"""
		
		# If the value of a constant is used as a magic value rather than a reference,
		# replace it with a reference to the variable representing the constant instead
		transformed = string
		for key, value in constants.items():
			transformed = transformed.replace(value, '${}'.format(key))
		
		# Convert `{{ variable }}` syntax to PowerShell `$variable` syntax
		# (Note that we don't bother to wrap the variable names in curly braces, since we know that none
		#  of the variable names contain special characters, and they're only ever interpolated as either
		#  part of a filesystem path surrounded by separators, or as a parameter surrounded by whitespace)
		return re.sub('{{ (.+?) }}', '$\\1', transformed)
	
	@staticmethod
	def replaceSystemPaths(path):
		"""
		Replaces hard-coded system paths with the equivalent environment variables
		"""
		replaced = path
		replaced = replaced.replace('C:\\Program Files', '$env:ProgramFiles')
		replaced = replaced.replace('C:\\ProgramData', '$env:ProgramData')
		return replaced
	
	@staticmethod
	def s3UriToHttpsUrl(s3Uri):
		"""
		Converts an `s3://` URI to an HTTPS URL
		"""
		url = s3Uri.replace('s3://', '')
		components = url.split('/', 1)
		return 'https://{}.s3.amazonaws.com/{}'.format(components[0], components[1])


# Retrieve the contents of the "Amazon EKS Optimized Windows AMI" EC2 ImageBuilder component
componentData = json.loads(Utility.capture([
	'aws',
	'imagebuilder',
	'get-component',
	'--region=us-east-1',
	'--component-build-version-arn',
	'arn:aws:imagebuilder:us-east-1:aws:component/eks-optimized-ami-windows/1.24.0'
]))

# Parse the pipeline YAML data and extract the list of constants
pipelineData = yaml.load(componentData['component']['data'], Loader=yaml.Loader)
constants = Utility.parseConstants(pipelineData['constants'])

# Extract the steps for the "build" phase
buildSteps = [p['steps'] for p in pipelineData['phases'] if p['name'] == 'build'][0]

print('CONSTANTS:')
print(json.dumps(constants, indent=4))

print()
print('BUILD STEPS:')
print(json.dumps(buildSteps, indent=4))

# Prepend our header to the generated PowerShell code
generated = '''<#
	THIS FILE IS AUTOMATICALLY GENERATED, DO NOT EDIT!
	
	This script is based on the logic from the "Amazon EKS Optimized Windows AMI"
	EC2 ImageBuilder component, with modifications to use containerd 1.7.0.
	
	The original ImageBuilder component logic is Copyright Amazon.com, Inc. or
	its affiliates, and is licensed under the MIT License.
#>

# Halt execution if we encounter an error
$ErrorActionPreference = 'Stop'


# Applies in-place patches to a file
function PatchFile
{
	Param (
		$File,
		$Patches
	)
	
	$patched = Get-Content -Path $File -Raw
	$Patches.GetEnumerator() | ForEach-Object {
		$patched = $patched.Replace($_.Key, $_.Value)
	}
	Set-Content -Path $File -Value $patched -NoNewline
}


'''

# Inject an additional constant for the parent of the temp directory, immediately before the child directory
tempPath = {k:v for k,v in constants.items() if k == 'TempPath'}
otherConstants = {k:v for k,v in constants.items() if k != 'TempPath'}
constants = {**otherConstants, 'TempRoot': 'C:\\TempEKSArtifactDir', **tempPath}

# Define variables for each of our constants
generated += '# Constants\n'
existingConstants = {}
for key, value in constants.items():
	transformed = Utility.replaceConstants(value, existingConstants)
	transformed = Utility.replaceSystemPaths(transformed)
	generated += '${} = "{}"\n'.format(key, transformed)
	existingConstants[key] = value

# Process each build step in turn
for step in buildSteps:
	
	# Determine whether we have custom preprocessing logic for the step
	name = step['name']
	if name == 'ConfigureDirectories':
		
		# Add the temp directory to the list of directories to be created
		step['loop']['forEach'] += [constants['TempRoot']]
		
	elif name == 'DownloadKubernetes':
		
		# Inject the driver installation step immediately prior to the Kubernetes download step
		generated += '\n'.join([
			'',
			'# Install the NVIDIA GPU drivers',
			"$driverBucket = 'ec2-windows-nvidia-drivers'",
			"$driver = Get-S3Object -BucketName $driverBucket -KeyPrefix 'latest' -Region 'us-east-1' | Where-Object {$_.Key.Contains('server2022')}",
			'Copy-S3Object -BucketName $driverBucket -Key $driver.Key -LocalFile "$TempRoot\driver.exe" -Region \'us-east-1\'',
			"Start-Process -FilePath \"$TempRoot\driver.exe\" -ArgumentList @('-s', '-noreboot') -NoNewWindow -Wait",
			''
		])
	
	elif name == 'ExtractEKSArtifacts':
		
		# Remove the redundant directory creation command
		step['inputs']['commands'] = [
			c for c in step['inputs']['commands']
			if not c.startswith('New-Item')
		]
		
		# Use absolute file and directory paths rather than relative paths
		step['inputs']['commands'] = [
			c.replace('EKS-Artifacts.zip', '"C:\\EKS-Artifacts.zip"').replace('TempEKSArtifactDir', 'C:\\TempEKSArtifactDir')
			for c in step['inputs']['commands']
		]
		
	elif name == 'InstallContainerRuntimes':
		
		# Inject the containerd 1.7.0 download step, along with our configuration patching steps, immediately prior to the containerd installation step
		generated += '\n'.join([
			'',
			'# -------',
			'',
			'# TEMPORARY UNTIL EKS ADDS SUPPORT FOR CONTAINERD v1.7.0:',
			'# Download and extract the containerd 1.7.0 release build',
			'$containerdTarball = "$TempPath\\containerd-1.7.0.tar.gz"',
			'$containerdFiles = "$TempPath\\containerd-1.7.0"',
			'$webClient.DownloadFile(\'https://github.com/containerd/containerd/releases/download/v1.7.0/containerd-1.7.0-windows-amd64.tar.gz\', $containerdTarball)',
			'New-Item -Path "$containerdFiles" -ItemType Directory -Force | Out-Null',
			'tar.exe -xvzf "$containerdTarball" -C "$containerdFiles"',
			'',
			'# Move the containerd files into place',
			'Move-Item -Path "$containerdFiles\\bin\\containerd.exe" -Destination "$ContainerdPath\\containerd.exe" -Force',
			'Move-Item -Path "$containerdFiles\\bin\\containerd-shim-runhcs-v1.exe" -Destination "$ContainerdPath\\containerd-shim-runhcs-v1.exe" -Force',
			'Move-Item -Path "$containerdFiles\\bin\\ctr.exe" -Destination "$ContainerdPath\\ctr.exe" -Force',
			'',
			'# Clean up the containerd intermediate files',
			'Remove-Item -Path "$containerdFiles" -Recurse -Force',
			'Remove-Item -Path "$containerdTarball" -Force',
			'',
			'# -------',
			'',
			'# Patch the containerd setup script to configure a log file (rather than just discarding log output) and to use the upstream pause',
			'# container image rather than the EKS version, since the latter appears to cause errors when attempting to create Windows Pods',
			'PatchFile -File "$TempPath\Add-ContainerdRuntime.ps1" -Patches @{',
			'	"containerd --register-service" = "containerd --register-service --log-file \'C:\\ProgramData\\containerd\\root\\output.log\'";',
			'	"amazonaws.com/eks/pause-windows:latest" = "registry.k8s.io/pause:3.9"',
			'}',
			'',
			'# Add the full Windows Server 2022 base image and the pause image to the list of images to pre-pull',
			'$baseLayersFile = "$TempPath\eks.baselayers.config"',
			'$baseLayers = Get-Content -Path $baseLayersFile -Raw | ConvertFrom-Json',
			'$baseLayers.2022 += "mcr.microsoft.com/windows/server:ltsc2022"',
			'$baseLayers.2022 += "registry.k8s.io/pause:3.9"',
			'$patchedJson = ConvertTo-Json -Depth 100 -InputObject $baseLayers',
			'Set-Content -Path $baseLayersFile -Value $patchedJson -NoNewline',
			'',
		])
		
		# Simplify the containerd installation command
		step['inputs']['commands'] = [
			'',
			'# Register containerd as the EKS container runtime',
			'Push-Location $TempPath',
			'& .\Add-ContainerdRuntime.ps1 -Path "$ContainerdPath"',
			'Pop-Location'
		]
		
	elif name == 'ExecuteBuildScripts':
		
		# Prefix each script invocation with the call operator
		step['loop']['forEach'] = [
			'& {}'.format(command)
			for command in step['loop']['forEach']
		]
		
		# Strip away the boilerplate code surrounding each script invocation
		step['inputs']['commands'] = ['Push-Location $TempPath'] + step['loop']['forEach'] + ['Pop-Location']
	
	# -------
	
	# If we have a descriptive comment for the step then include it above its generated code
	comment = Utility.commentForStep(name)
	if comment != None:
		generated += '\n{}\n'.format(comment)
	
	# -------
	
	# Generate code for the step based on its action type
	action = step['action']
	
	if action == 'CreateFolder':
		directories = [Utility.replaceConstants(d, constants) for d in step['loop']['forEach']]
		generated += '\n'.join([
			'foreach ($dir in @({})) {{'.format(', '.join(directories)),
			'\tNew-Item -Path $dir -ItemType Directory -Force | Out-Null',
			'}'
		])
		
	elif action == 'DeleteFolder':
		generated += '\n'.join([
			'Remove-Item -Path "{}" -Recurse -Force'.format(Utility.replaceConstants(input['path'], constants))
			for input in step['inputs']
		])
		
	elif action == 'MoveFile':
		generated += '\n'.join([
			'Move-Item -Path "{}" -Destination "{}" -Force'.format(
				Utility.replaceConstants(input['source'], constants),
				Utility.replaceConstants(input['destination'], constants)
			)
			for input in step['inputs']
		])
		
	elif action == 'S3Download':
		generated += '\n'.join([
			'$webClient.DownloadFile("{}", "{}")'.format(
				Utility.s3UriToHttpsUrl(input['source']),
				Utility.replaceConstants(input['destination'], constants)
			)
			for input in step['inputs']
		])
		
	elif action == 'ExecutePowerShell':
		generated += '\n'.join([
			Utility.replaceConstants(c, constants).replace("'", '"')
			for c in step['inputs']['commands']
			if not c.startswith('$ErrorActionPreference')
		])
		
	elif action == 'Reboot':
		Utility.log('Ignoring reboot step.')
		continue
		
	else:
		raise RuntimeError('Unknown build step action: {}'.format(action))
	
	# -------
	
	# Add a trailing newline after each non-ignored step
	generated += '\n'

# Write the generated code to the output script file
outfile = Path(__file__).parent / 'scripts' / 'setup.ps1'
Utility.writeFile(outfile, generated)
Utility.log('Wrote generated code to {}'.format(outfile))
