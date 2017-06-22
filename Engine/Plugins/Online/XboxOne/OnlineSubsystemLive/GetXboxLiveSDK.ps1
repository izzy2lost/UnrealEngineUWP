function Install-LivePackage($pathToNuget, $packageName, $packageVersion, $installLocation, $alias)
{
	# Package names get long, which can cause path length problems both during install
	# and when referencing contents later.  Install to the temp folder, and then just copy
	# out the bits we actually need.
	$tempFolder = [System.IO.Path]::GetTempPath()
	&$pathToNuget install $packageName -version $packageVersion -outputdirectory $tempFolder

	$aliasPath = [System.IO.Path]::Combine($installLocation, $alias + "." + $packageVersion)

	$actualPath = [System.IO.Path]::Combine($tempFolder, $packageName + "." + $packageVersion, "build", "native")
	Copy-Item ([System.IO.Path]::Combine($actualPath, "lib")) -Destination ([System.IO.Path]::Combine($aliasPath, "lib")) -Recurse -ErrorAction Ignore
	Copy-Item ([System.IO.Path]::Combine($actualPath, "bin")) -Destination ([System.IO.Path]::Combine($aliasPath, "bin")) -Recurse -ErrorAction Ignore
	Copy-Item ([System.IO.Path]::Combine($actualPath, "references")) -Destination ([System.IO.Path]::Combine($aliasPath, "references")) -Recurse -ErrorAction Ignore
	Copy-Item ([System.IO.Path]::Combine($actualPath, "include")) -Destination ([System.IO.Path]::Combine($aliasPath, "include")) -Recurse -ErrorAction Ignore
}

# Package versions.  Should match OnlineSubsystemLive.build.cs
$xsapiVersionUwp = "2017.05.20170517.001"
$xsapiVersionXdk = "2017.05.20170517.001"

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
Install-LivePackage $nuget microsoft.xbox.live.sdk.winrt.uwp.native.release $xsapiVersionUwp $xsapiInstallPath UWP
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