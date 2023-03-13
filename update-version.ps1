Param (
	[parameter(Mandatory=$true, Position=0, HelpMessage = "The new version string")]
	$version
)


# Patches a file using the specified regular expression search string and replacement
function Patch-File {
	Param (
		$Path,
		$Search,
		$Replace
	)
	
	Write-Host "Updating $Path" -ForegroundColor Cyan
	$content = Get-Content -Path $Path -Raw
	$content = $content -replace $Search, $Replace
	Set-Content -Path $Path -NoNewline -Value $content
}

# Patches image tags in a YAML file
function Patch-ImageTags {
	Param ($Path)
	Patch-File -Path $Path `
		-Search '"index.docker.io/tensorworks/(.+):(.+)"' `
		-Replace "`"index.docker.io/tensorworks/`$1:$version`""
}


# Update the version strings in all of our files, ready for a new release
Write-Host "Updating version strings to $version..." -ForegroundColor Green

# Update the version string for the device discovery library
Patch-File -Path "$PSScriptRoot\library\src\DeviceDiscovery.cpp" `
	-Search '#define LIBRARY_VERSION L"(.+)"' `
	-Replace "#define LIBRARY_VERSION L`"$version`""

# Update the version string for the device plugins
Patch-File -Path "$PSScriptRoot\plugins\internal\plugin\common_main.go" `
	-Search 'const version = "(.+)"' `
	-Replace "const version = `"$version`""

# Update the deployment YAML files
foreach ($file in Get-ChildItem -Path "$PSScriptRoot\deployments\*.yml") {
	Patch-ImageTags -Path $file.FullName
}

# Update the example YAML files
foreach ($file in Get-ChildItem -Path "$PSScriptRoot\examples\*\*.yml") {
	Patch-ImageTags -Path $file.FullName
}
