function Install-LivePackage($pathToNuget, $packageName, $packageVersion, $installLocation, $alias)
{
	# Package names get long, which can cause path length problems both during install
	# and when referencing contents later.  Install via a temp symlink, and also set up
	# permanent links for use by the build system.
	$tempFolder = [System.IO.Path]::GetTempPath()
	[string] $tempLinkName = [System.Guid]::NewGuid()
	$tempLinkName = Join-Path $tempFolder $tempLinkName
	New-Item -Path $installLocation -ItemType Directory -ErrorAction Ignore
	New-Item -Path $tempLinkName -ItemType SymbolicLink -Value $installLocation

	&$pathToNuget install $packageName -version $packageVersion -outputdirectory $tempLinkName

	$aliasPath = $installLocation + "\" + $alias
	$actualPath = $installLocation + "\" + $packageName + "." + $packageVersion

	New-Item -Path $aliasPath -ItemType SymbolicLink -Value $actualPath -Force

	# Remove-Item would try to remove the actual files, rather than just the link
	cmd.exe /c "rmdir $tempLinkName"
}

# Package versions
$xsapiVersionUwp = "2016.12.20170107.01"
$xsapiVersionXdk = "2016.12.20170126.001"


# Elevate if necessary (needed for new-item)
if (!([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) 
{ 
	$arguments = "& '" + $myinvocation.mycommand.definition + "'"
	Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments
	exit 
}

$webClient = New-Object System.Net.WebClient
$ossLivePath = Split-Path $MyInvocation.MyCommand.Path

# Locate nuget.exe - check locally first
$nuget = (Get-ChildItem | Where-Object {$_.Name -eq "nuget.exe"})
if ($nuget -eq $null)
{
	# Failing that, check PATH
	$nuget = (Get-Command -commandtype application | Where-Object {$_.Name -eq "nuget.exe"})

	# Download if still not found
	if ($nuget -eq $null)
	{
		$nuget = $ossLivePath + "\nuget.exe"
		$webClient.DownloadFile("https://dist.nuget.org/win-x86-commandline/latest/nuget.exe", $nuget)
	}
}
else
{
	$nuget = $ossLivePath + "\" + $nuget
}

# Use nuget.exe to install Xbox Live packages
$xsapiInstallPath = $ossLivePath + "\ThirdParty\XSAPI"
$ximInstallPath = $ossLivePath + "\ThirdParty\XIM"
Install-LivePackage $nuget microsoft.xbox.live.sdk.winrt.uwp $xsapiVersionUwp $xsapiInstallPath UWP
Install-LivePackage $nuget microsoft.xbox.live.sdk.winrt.XboxOneXDK $xsapiVersionXdk $xsapiInstallPath XboxOne

# Check for Live Extensions SDK
$existingLiveExtSdk = Get-ItemProperty HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\* | Where-Object {$_.DisplayName -like "*Xbox Live Platform Extensions*"}
if ($existingLiveExtSdk -eq $null)
{
	# Downloading Xbox Live Extensions SDK
	$xblextzip = $ossLivePath + "\XboxLiveExtensionSDK.zip"
	$xblextfolder = $ossLivePath + "\XboxLiveExtensionSDK"
	$webClient.DownloadFile("https://aka.ms/xblextsdk", $xblextzip )

	# Unpack and install
	Expand-Archive $xblextzip -DestinationPath $xblextfolder -Force
	$installExe = $xblextfolder + "\XboxLivePlatformExt.exe"
	$installProc = Start-Process $installExe -wait

	# Cleanup
	Remove-Item $xblextzip
	Remove-Item $xblextfolder -Recurse -Force
}