<powershell>

# Install the OpenSSH server and set the sshd service to start automatically at system startup
Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
Set-Service -Name sshd -StartupType 'Automatic'

# Create the OpenSSH configuration directory if it doesn't already exist
$sshDir = 'C:\ProgramData\ssh'
if ((Test-Path -Path $sshDir) -eq $false) {
	New-Item -Path $sshDir -ItemType Directory -Force | Out-Null
}

# Retrieve the SHH public key from the EC2 metadata service
$authorisedKeys = "$sshDir\administrators_authorized_keys"
curl.exe 'http://169.254.169.254/latest/meta-data/public-keys/0/openssh-key' -o "$authorisedKeys"

# Set the required ACLs for the authorised keys file
icacls.exe "$authorisedKeys" /inheritance:r /grant "Administrators:F" /grant "SYSTEM:F"

# Install the Windows feature for containers, which will require a reboot
Install-WindowsFeature -Name Containers -IncludeAllSubFeature

# Restart the VM
Restart-Computer

</powershell>