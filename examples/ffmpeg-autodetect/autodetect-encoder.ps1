# Attempt to detect the availability of a hardware video encoder
$encoder = ''
if ((Get-ChildItem "C:\Windows\System32\amfrt64.dll" -ErrorAction SilentlyContinue))
{
	Write-Host 'Detected an AMD GPU, using the AMF video encoder'
	$encoder = 'h264_amf'
}
elseif ((Get-ChildItem "C:\Windows\System32\intel_gfx_api-x64.dll" -ErrorAction SilentlyContinue))
{
	Write-Host 'Detected an Intel GPU, using the Quick Sync video encoder'
	$encoder = 'h264_qsv'
}
elseif ((Get-ChildItem "C:\Windows\System32\nvEncodeAPI64.dll" -ErrorAction SilentlyContinue))
{
	Write-Host 'Detected an NVIDIA GPU, using the NVENC video encoder'
	$encoder = 'h264_nvenc'
}
else {
	throw "Failed to detect the availability of a supported hardware video encoder"
}

# Invoke FFmpeg with the detected hardware video encoder
& C:\ffmpeg.exe -i C:\sample-video.mp4 -c:v "$encoder" -preset default C:\output.mp4
if ($LastExitCode -ne 0) {
	throw "FFmpeg terminated with exit code $LastExitCode"
}
