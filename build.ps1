Param (
	[parameter(HelpMessage = "Clean existing build output")]
	[switch] $Clean,
	
	[parameter(HelpMessage = "Build the device discovery library only, do not build the Kubernetes device plugins or container images")]
	[switch] $LibraryOnly,
	
	[parameter(HelpMessage = "Build the Kubernetes device plugins only, do not build the device discovery library or container images")]
	[switch] $PluginsOnly,
	
	[parameter(HelpMessage = "Build container images for the Kubernetes device plugins")]
	[switch] $Images,
	
	[parameter(HelpMessage = "Build container images for example workloads")]
	[switch] $Examples,
	
	[parameter(HelpMessage = "The prefix used for image tags when building container images")]
	$TagPrefix = 'index.docker.io/tensorworks',
	
	[parameter(HelpMessage = "Override the repository version string for computing container image tag suffixes")]
	$Version = ''
)


# Halt execution if we encounter an error
$ErrorActionPreference = 'Stop'


# Executes a command and throws an error if it returns a non-zero exit code
function Run-Command($command) {
	
	# Print the command and execute it
	$formatted = If ($args.Count -gt 0) { "$command $args" } Else { "$command"}
	Write-Host "[$formatted]" -ForegroundColor DarkYellow
	& "$command" @args
	
	# If the command terminated with a non-zero exit code then throw an error
	if ($LastExitCode -ne 0) {
		throw "Command '$formatted' terminated with exit code $LastExitCode"
	}
}

# Invokes the Crane image management tool
function Invoke-Crane {
	Run-Command go run 'github.com/google/go-containerregistry/cmd/crane@latest' @args
}

# Builds a container image using Crane
function Build-ContainerImage {
	Param (
		$BaseImage,
		$Directory,
		$Entrypoint,
		$ImageTag,
		$Files
	)
	
	# Create a tarball with the specified files
	$tarball = "$env:Temp\layer-$([guid]::NewGuid()).tar"
	Run-Command tar.exe --create --verbose --file="$tarball" --directory="$Directory" @Files
	
	# Append the filesystem layer to the base image and set the entrypoint
	Invoke-Crane append --platform=windows/amd64 -b "$BaseImage" -f "$tarball" -t "$ImageTag"
	Invoke-Crane mutate "$ImageTag" --entrypoint="$Entrypoint"
	
	# Perform cleanup
	Remove-Item -Path "$tarball" -Force
	
	# Print the container image tag
	Write-Host "Built image: $ImageTag" -ForegroundColor Cyan
}


# Resolve the path to the directories used during the build process
$buildDir = "$PSScriptRoot\build"
$binDir = "$PSScriptRoot\bin"
$examplesDir = "$PSScriptRoot\examples"
$externalDir = "$PSScriptRoot\external"
$vcpkgDir = "$externalDir\vcpkg"

# Determine whether we are cleaning the build output
if ($Clean)
{
	# Remove the build directory if it exists
	if ((Test-Path -Path $buildDir) -eq $true)
	{
		Write-Host 'Removing the build directory...' -ForegroundColor Green
		Remove-Item -Path $buildDir -Recurse -Force
	}
	
	# Remove all .dll and .exe files in the bin directory
	Write-Host 'Removing binaries from the bin directory...' -ForegroundColor Green
	Remove-Item -Path "$binDir\*.dll" -Force
	Remove-Item -Path "$binDir\*.exe" -Force
	Exit
}

# Install vcpkg if we don't already have it
if ((Test-Path -Path $vcpkgDir) -eq $false)
{
	Write-Host "`nInstalling a local copy of vcpkg..." -ForegroundColor Green
	Run-Command git clone 'https://github.com/Microsoft/vcpkg.git' "$vcpkgDir"
	Run-Command "$vcpkgDir\bootstrap-vcpkg.bat"
}

# Install vswhere if we don't already have it
$vsWhere = "$externalDir\vswhere.exe"
if ((Test-Path -Path $vsWhere) -eq $false)
{
	Write-Host "`nInstalling a local copy of vswhere..." -ForegroundColor Green
	(New-Object System.Net.WebClient).DownloadFile(
		'https://github.com/microsoft/vswhere/releases/download/3.0.3/vswhere.exe',
		"$vsWhere"
	)
}

# Install the NuGet CLI tool if we don't already have it
$nuget = "$externalDir\nuget.exe"
$nugetConfig = "$externalDir\NuGet.Config"
if ((Test-Path -Path $nuget) -eq $false)
{
	# Install the CLI tool
	Write-Host "`nInstalling a local copy of the NuGet CLI tool..." -ForegroundColor Green
	(New-Object System.Net.WebClient).DownloadFile(
		'https://dist.nuget.org/win-x86-commandline/v6.2.1/nuget.exe',
		"$nuget"
	)
	
	# Configure the CLI tool with the nuget.org source
	Set-Content -Path "$nugetConfig" -Value '<configuration></configuration>' -Force
	Run-Command "$nuget" sources add -Name 'nuget.org' -Source 'https://api.nuget.org/v3/index.json' -ConfigFile "$nugetConfig"
	Run-Command "$nuget" sources enable -Name 'nuget.org' -ConfigFile "$nugetConfig"
}

# Use vswhere to determine the version of Visual Studio that is installed (2019, 2022, etc.)
$vsVersion = & "$vswhere" -latest -products * -requires Microsoft.Component.MSBuild -property 'catalog_productLineVersion'
if (!$vsVersion) {
	throw "Failed to determine the version of Visual Studio"
}

# Determine the platform toolset version that corresponds to the Visual Studio version
$toolsetVersions = @{ "2019" = "v142"; "2022" = "v143" }
$platformToolset = $toolsetVersions[$vsVersion]
if (!$platformToolset) {
	throw "Failed to determine the platform toolset version that corresponds to the version of Visual Studio"
}

# Use vswhere to locate MSBuild
$msBuild = & "$vswhere" -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'
if (!$msBuild) {
	throw "Failed to locate MSBuild"
}

# Use vswhere to locate the MSBuild `BuildCustomizations` directory for the latest toolchain
$vsCustomisations = & "$vswhere" -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\Microsoft\VC\*\BuildCustomizations'
if (!$vsCustomisations) {
	throw "Failed to locate the MSBuild BuildCustomizations directory"
}

# If a version override was not specified then calculate it from the Git commit details
if (!$Version)
{
	# Set the version string to the commit hash
	$Version = & git rev-parse HEAD
	
	# Determine whether the current commit is a tagged release
	$gitTag = & git name-rev --name-only --tags HEAD
	if ($gitTag -ne 'undefined') {
		$Version = $gitTag
	}
}

# Report the detected values
Write-Host "Repository version: $Version" -ForegroundColor Cyan
Write-Host "Detected Visual Studio $vsVersion (platform toolset $platformToolset)" -ForegroundColor Cyan
Write-Host "Found MSBuild: $msBuild" -ForegroundColor Cyan

if (!$PluginsOnly)
{
	# Create the build directory if it doesn't already exist
	if ((Test-Path -Path $buildDir) -eq $false)
	{
		Write-Host "`nCreating the build directory..." -ForegroundColor Green
		New-Item -Path $buildDir -ItemType Directory -Force | Out-Null
	}
	
	# Build the device discovery library (vcpkg will automatically install all required dependencies)
	Write-Host "`nBuilding the device discovery library..." -ForegroundColor Green
	Push-Location "$buildDir"
	Run-Command cmake "$PSScriptRoot/library" `
		-A x64 -DVCPKG_TARGET_TRIPLET=x64-windows `
		"-DCMAKE_INSTALL_PREFIX=$PSScriptRoot" `
		"-DCMAKE_TOOLCHAIN_FILE=$vcpkgDir\scripts\buildsystems\vcpkg.cmake" 
	Run-Command cmake --build . --target install --config Release
	Pop-Location
}

if (!$LibraryOnly)
{
	# Build the Kubernetes device plugins and strip the debug symbols from the executables
	Write-Host "`nBuilding the Kubernetes device plugins..." -ForegroundColor Green
	Push-Location "$PSScriptRoot/plugins"
	$env:CGO_ENABLED=0
	Run-Command go build -o "$PSScriptRoot\bin" -ldflags '-s -w' ./...
	Pop-Location
}

if (!$PluginsOnly -and !$LibraryOnly -and $Images)
{
	# Use Crane to build the container image for the MCDM device plugin
	Write-Host "`nBuilding and pushing the container image for the MCDM device plugin..." -ForegroundColor Green
	Build-ContainerImage `
		-ImageTag "$TagPrefix/mcdm-device-plugin:$Version" `
		-BaseImage 'mcr.microsoft.com/windows/nanoserver:ltsc2022' `
		-Files @('directx-device-discovery.dll', 'device-plugin-mcdm.exe') `
		-Entrypoint 'device-plugin-mcdm.exe' `
		-Directory $binDir
	
	# Use Crane to build the container image for the WDDM device plugin
	Write-Host "`nBuilding and pushing the container image for the WDDM device plugin..." -ForegroundColor Green
	Build-ContainerImage `
		-ImageTag "$TagPrefix/wddm-device-plugin:$Version" `
		-BaseImage 'mcr.microsoft.com/windows/nanoserver:ltsc2022' `
		-Files @('directx-device-discovery.dll', 'device-plugin-wddm.exe') `
		-Entrypoint 'device-plugin-wddm.exe' `
		-Directory $binDir
}

if (!$PluginsOnly -and !$LibraryOnly -and $Examples)
{
	# Use Crane to build the container image for the device discovery example
	Write-Host "`nBuilding and pushing the container image for the device discovery example..." -ForegroundColor Green
	Build-ContainerImage `
		-ImageTag "$TagPrefix/example-device-discovery:$Version" `
		-BaseImage 'mcr.microsoft.com/windows/servercore:ltsc2022' `
		-Files @('directx-device-discovery.dll', 'test-device-discovery-cpp.exe') `
		-Entrypoint 'C:\test-device-discovery-cpp.exe,--verbose' `
		-Directory $binDir
	
	# Clone the DirectML repository if we don't already have a copy
	$directMLDir = "$externalDir\DirectML"
	$directMLCommit = '3c5a947e0e4f8115dd5dd2fea00ac545120052ac'
	if ((Test-Path -Path $directMLDir) -eq $false)
	{
		Write-Host "`nDownloading the DirectML repository..." -ForegroundColor Green
		New-Item -Path $directMLDir -ItemType Directory -Force | Out-Null
		Push-Location "$directMLDir"
		Run-Command git init
		Run-Command git remote add origin 'https://github.com/microsoft/DirectML.git'
		Run-Command git fetch --depth=1 origin "$directMLCommit"
		Run-Command git checkout "$directMLCommit"
		Pop-Location
	}
	
	# Build the HelloDirectML sample if it hasn't already been built
	$helloDirectMLDir = "$directMLDir\Samples\HelloDirectML"
	$helloDirectMLBin = "$helloDirectMLDir\HelloDirectML\x64\Release"
	$helloDirectMLExe = "$helloDirectMLBin\HelloDirectML.exe"
	if ((Test-Path -Path $helloDirectMLExe) -eq $false)
	{
		# Build the sample
		Write-Host "`nBuilding the HelloDirectML sample..." -ForegroundColor Green
		$env:_CL_='/MT'
		Push-Location "$helloDirectMLDir"
		Run-Command "$nuget" restore -ConfigFile "$nugetConfig"
		Run-Command "$msBuild" HelloDirectML.vcxproj `
			/property:Configuration=Release `
			/property:Platform=x64 `
			"/property:PlatformToolset=$platformToolset" `
			/property:WindowsTargetPlatformVersion=10.0
		Pop-Location
		
		# Copy DirectML.dll from the NuGet package to the sample bin directory
		Copy-Item -Path "$helloDirectMLDir\packages\Microsoft.AI.DirectML.1.7.0\bin\x64-win\DirectML.dll" -Destination "$helloDirectMLBin" -Force
	}
	
	# Use Crane to build the container image for the DirectML example
	Write-Host "`nBuilding and pushing the container image for the DirectML example..." -ForegroundColor Green
	Build-ContainerImage `
		-ImageTag "$TagPrefix/example-directml:$Version" `
		-BaseImage 'mcr.microsoft.com/windows/servercore:ltsc2022' `
		-Files @('DirectML.dll', 'HelloDirectML.exe') `
		-Entrypoint 'C:\HelloDirectML.exe' `
		-Directory $helloDirectMLBin
	
	# Clone the OpenCL SDK repository if we don't already have a copy
	$openCLSDK = "$externalDir\OpenCL-SDK"
	if ((Test-Path -Path $openCLSDK) -eq $false)
	{
		Write-Host "`nDownloading the OpenCL SDK repository..." -ForegroundColor Green
		Run-Command git clone -b 'v2022.05.18' --depth=1 --recursive 'https://github.com/KhronosGroup/OpenCL-SDK.git' "$openCLSDK"
	}
	
	# Build the enumopencl sample if it hasn't already been built
	$openCLBuild = "$openCLSDK\build"
	$openCLBin = "$openCLBuild\bin\Release"
	$enumOpenCL = "$openCLBin\enumopencl.exe"
	if ((Test-Path -Path $enumOpenCL) -eq $false)
	{
		# Create a vcpkg manifest file to install the dependencies for the OpenCL samples
		Set-Content -Force -Path "$openCLSDK\vcpkg.json" -Value '{"name":"opencl","version":"0.0.0","dependencies":["glm","sfml","tclap"]}'
		
		# Build the samples
		Write-Host "`nBuilding the enumopencl sample..." -ForegroundColor Green
		New-Item -Path $openCLBuild -ItemType Directory -Force | Out-Null
		Push-Location "$openCLBuild"
		Run-Command cmake "$openCLSDK" `
			-A x64 -DVCPKG_TARGET_TRIPLET=x64-windows `
			'-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG' `
			'-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG' `
			"-DCMAKE_INSTALL_PREFIX=$openCLSDK" `
			"-DCMAKE_TOOLCHAIN_FILE=$vcpkgDir\scripts\buildsystems\vcpkg.cmake" `
			-DBUILD_TESTING=OFF `
			-DBUILD_DOCS=OFF `
			-DBUILD_EXAMPLES=OFF `
			-DBUILD_TESTS=OFF `
			-DOPENCL_SDK_BUILD_SAMPLES=ON `
			-DOPENCL_SDK_TEST_SAMPLES=OFF
		Run-Command cmake --build . --target enumopencl --config Release
		Pop-Location
	}
	
	# Use Crane to build the container image for the enumopencl example
	Write-Host "`nBuilding and pushing the container image for the enumopencl example..." -ForegroundColor Green
	Build-ContainerImage `
		-ImageTag "$TagPrefix/example-opencl-enum:$Version" `
		-BaseImage 'mcr.microsoft.com/windows/servercore:ltsc2022' `
		-Files @('enumopencl.exe') `
		-Entrypoint 'C:\enumopencl.exe' `
		-Directory $openCLBin
	
	# Determine whether the CUDA Toolkit v11.6 is installed
	if ($env:CUDA_PATH_V11_6)
	{
		# Copy the Visual Studio integration files from the CUDA Toolkit to the MSBuild `BuildCustomizations` directory
		Copy-Item -Path "$env:CUDA_PATH\extras\visual_studio_integration\MSBuildExtensions\*" -Destination "$vsCustomisations" -Force
		
		# Clone the CUDA samples repository if we don't already have a copy
		$cudaSamples = "$externalDir\cuda-samples"
		$cudaSamplesBin = "$cudaSamples\bin\win64\Release"
		if ((Test-Path -Path $cudaSamples) -eq $false)
		{
			Write-Host "`nDownloading the CUDA samples repository..." -ForegroundColor Green
			Run-Command git clone -b 'v11.6' --depth=1 'https://github.com/NVIDIA/cuda-samples.git' "$cudaSamples"
		}
		
		# Build the CUDA deviceQuery sample if it hasn't already been built
		$deviceQuery = "$cudaSamplesBin\deviceQuery.exe"
		if ((Test-Path -Path $deviceQuery) -eq $false)
		{
			Write-Host "`nBuilding the CUDA deviceQuery sample..." -ForegroundColor Green
			Run-Command "$msBuild" "$cudaSamples\Samples\1_Utilities\deviceQuery\deviceQuery_vs${vsVersion}.sln" /property:Configuration=Release
		}
		
		# Build the CUDA MC_EstimatePiP sample if it hasn't already been built
		$monteCarlo = "$cudaSamplesBin\MC_EstimatePiP.exe"
		if ((Test-Path -Path $monteCarlo) -eq $false)
		{
			# Build the sample
			Write-Host "`nBuilding the CUDA MC_EstimatePiP sample..." -ForegroundColor Green
			Run-Command "$msBuild" "$cudaSamples\Samples\2_Concepts_and_Techniques\MC_EstimatePiP\MC_EstimatePiP_vs${vsVersion}.sln" /property:Configuration=Release
			
			# Copy curand64_10.dll from the CUDA Toolkit bin directory to the sample bin directory
			Copy-Item -Path "$env:CUDA_PATH_V11_6\bin\curand64_10.dll" -Destination "$cudaSamplesBin" -Force
		}
		
		# Use Crane to build the container image for the CUDA deviceQuery example
		Write-Host "`nBuilding and pushing the container image for the CUDA deviceQuery example..." -ForegroundColor Green
		Build-ContainerImage `
			-ImageTag "$TagPrefix/example-cuda-devicequery:$Version" `
			-BaseImage 'mcr.microsoft.com/windows/servercore:ltsc2022' `
			-Files @('deviceQuery.exe') `
			-Entrypoint 'C:\deviceQuery.exe' `
			-Directory $cudaSamplesBin
		
		# Use Crane to build the container image for the CUDA MC_EstimatePiP example
		Write-Host "`nBuilding and pushing the container image for the CUDA MC_EstimatePiP example..." -ForegroundColor Green
		Build-ContainerImage `
			-ImageTag "$TagPrefix/example-cuda-montecarlo:$Version" `
			-BaseImage 'mcr.microsoft.com/windows/servercore:ltsc2022' `
			-Files @('curand64_10.dll', 'MC_EstimatePiP.exe') `
			-Entrypoint 'C:\MC_EstimatePiP.exe' `
			-Directory $cudaSamplesBin
	}
	else {
		Write-Host "`nCUDA Toolkit v11.6 was not detected, not building CUDA examples" -ForegroundColor Cyan
	}
	
	# Install FFmpeg if we don't already have it
	$ffmpegDir = "$externalDir\ffmpeg"
	if ((Test-Path -Path $ffmpegDir) -eq $false)
	{
		# Download the FFmpeg release archive
		Write-Host "`nInstalling a local copy of FFmpeg..." -ForegroundColor Green
		$ffmpegZip = "$externalDir\ffmpeg.zip"
		(New-Object System.Net.WebClient).DownloadFile(
			'https://www.gyan.dev/ffmpeg/builds/packages/ffmpeg-5.1.2-essentials_build.zip',
			"$ffmpegZip"
		)
		
		# Extract the archive
		Expand-Archive -Path "$ffmpegZip" -DestinationPath "$ffmpegDir" -Force
	}
	
	# Download a sample video for use with FFmpeg if we don't already have it
	$ffmpegBin = "$ffmpegDir\ffmpeg-5.1.2-essentials_build\bin"
	$sampleVideo = "$ffmpegBin\sample-video.mp4"
	if ((Test-Path -Path $sampleVideo) -eq $false)
	{
		Write-Host "`nDownloading a sample video for use with FFmpeg..." -ForegroundColor Green
		(New-Object System.Net.WebClient).DownloadFile(
			'https://raw.githubusercontent.com/SPBTV/video_av1_samples/master/spbtv_sample_bipbop_av1_960x540_25fps.mp4',
			"$sampleVideo"
		)
	}
	
	# Use Crane to build the container image for the FFmpeg examples
	Write-Host "`nBuilding and pushing the container image for the FFmpeg examples..." -ForegroundColor Green
	Copy-Item -Path "$examplesDir\ffmpeg-autodetect\autodetect-encoder.ps1" -Destination "$ffmpegBin" -Force
	Build-ContainerImage `
		-ImageTag "$TagPrefix/example-ffmpeg:$Version" `
		-BaseImage 'mcr.microsoft.com/windows/server:ltsc2022' `
		-Files @('ffmpeg.exe', 'sample-video.mp4', 'autodetect-encoder.ps1') `
		-Entrypoint 'C:\ffmpeg.exe' `
		-Directory "$ffmpegBin"
}
