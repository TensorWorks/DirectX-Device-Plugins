# Perform cleanup
Set-Service -Name sshd -StartupType 'Manual'
Remove-Item -Path 'C:\ProgramData\ssh\administrators_authorized_keys' -Force

# Remove the file for this script, since Packer won't have a chance to perform its own cleanup
Remove-Item -Path $PSCommandPath -Force

# Perform sysprep and shut down the VM
& "$Env:ProgramFiles\Amazon\EC2Launch\EC2Launch.exe" sysprep --shutdown=true
