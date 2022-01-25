[CmdletBinding()]
Param()

function Install-Package($pathToNuget, $packageName, $installLocation, $subPaths, $packageVersion, $alias)
{
	# Package names get long, which can cause path length problems both during install
	# and when referencing contents later.  Install to the temp folder, and then just copy
	# out the bits we actually need.
	$tempFolder = [System.IO.Path]::GetTempPath()

	# This script used to unpack to folders with a version suffix.  Delete any of these that still
	# exist otherwise NuGet might decide the package is already installed and files won't be where
	# we expect
	Remove-Item -Path ([System.IO.Path]::Combine($tempFolder, $packageName + ".*")) -Recurse -ErrorAction Continue 2>&1 | Write-Verbose

	# Version name format is a little inconsistent (01 vs 001, etc.).  ExcludeVersion allows for
	# the output path to be predictable despite this.
	if ($packageVersion -ne $null)
	{
		&$pathToNuget install $packageName -outputdirectory $tempFolder -ExcludeVersion -version $packageVersion 2>&1 | Write-Verbose
	}
	else
	{
		&$pathToNuget install $packageName -outputdirectory $tempFolder -ExcludeVersion 2>&1 | Write-Verbose
	}

	# The install action should have essentially unzipped the nupkg.  Now we're going to
	# copy the important bits out of it into the requested UE location
	$unpackedToPath = [System.IO.Path]::Combine($tempFolder, $packageName)

	if ($alias -ne $null)
	{
		$aliasPath = [System.IO.Path]::Combine($installLocation, $alias + "." + $packageVersion)
	}
	else
	{
		$aliasPath = $installLocation
	}

	# Create the target folder if it does not already exist - this is important because
	# the behavior of Copy-Item will change depending on whether Destination exists or not.
	New-Item -ItemType Directory $aliasPath -ErrorAction Ignore

	# Iterate over the sub-directories provided and copy them into our UE tree
	$subPaths | %{[System.IO.Path]::Combine($unpackedToPath, $_)} | Copy-Item -Destination $aliasPath -Recurse -Container -ErrorAction Continue 2>&1 | Write-Verbose
}

# Package versions.  Should match OnlineSubsystemLive.build.cs
$xsapiVersionUwp = "2017.11.20171204.001"
$xsapiVersionXdk = "2017.11.20171204.001"
$ximVersionUwp = "1706.8.0"

$webClient = New-Object System.Net.WebClient
$startupPath = Split-Path $MyInvocation.MyCommand.Path
$ossLivePath = [System.IO.Path]::Combine($startupPath, "Engine", "Plugins", "Online", "OnlineSubsystemLive")

# Locate nuget.exe - check locally first
$nuget = (Get-ChildItem | Where-Object {$_.Name -eq "nuget.exe"})
if ($nuget -eq $null)
{
	# Failing that, check PATH
	$nuget = (Get-Command -commandtype application | Where-Object {$_.Name -eq "nuget.exe"})

	# Download if still not found
	if ($nuget -eq $null)
	{
		$nuget = [System.IO.Path]::Combine($startupPath, "nuget.exe")
		$webClient.DownloadFile("https://dist.nuget.org/win-x86-commandline/latest/nuget.exe", $nuget)
	}
}
else
{
	$nuget = $startupPath + "\" + $nuget
}

# Install Xbox Live packages
Write-Output "Installing Xbox Live SDK from Nuget..."
$xsapiInstallPath = [System.IO.Path]::Combine($ossLivePath, "ThirdParty", "XSAPI")
$ximInstallPath = [System.IO.Path]::Combine($ossLivePath, "ThirdParty", "XIM")
Install-Package $nuget microsoft.xbox.live.sdk.winrt.uwp.native.release $xsapiInstallPath @("build\native\lib") $xsapiVersionUwp UWP
Install-Package $nuget microsoft.xbox.live.sdk.winrt.XboxOneXDK $xsapiInstallPath @("build\native\bin", "build\native\references") $xsapiVersionXdk XboxOne
Install-Package $nuget microsoft.xbox.XboxIntegratedMultiplayer.cpp.uwp $ximInstallPath @("build\native\lib", "build\native\include") $ximVersionUwp UWP

# Install Windows Device Portal Wrapper (used by UWP.Automation)
#Write-Output "Installing Windows Device Portal Wrapper from Nuget..."
Write-Output "Skipping installing Windows Device Portal Wrapper from Nuget... because its outdated, and we already have a source version integrated"
#$wdpwrapperInstallPath = [System.IO.Path]::Combine($startupPath, "Engine", "Binaries", "ThirdParty", "WindowsDevicePortalWrapper")
#Install-Package $nuget windowsdeviceportalwrapper $wdpwrapperInstallPath @("lib\net452\*")

# Check for Live Extensions SDK
Write-Output "Checking for Xbox Live Extensions SDK..."
$windowsSdkLocationValue = (Get-ItemProperty "HKLM:\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v10.0" -Name InstallationFolder).InstallationFolder
$existingLiveExtSdk = get-childitem $windowsSdkLocationValue -recurse | where-object {$_.Name -like "XboxLive"}
if ($existingLiveExtSdk -eq $null)
{
	Write-Output "Xbox Live Extensions SDK not found.  Installing..."

	# Downloading Xbox Live Extensions SDK
	$xblextzip = $ossLivePath + "\XboxLiveExtensionSDK.zip"
	$xblextfolder = $ossLivePath + "\XboxLiveExtensionSDK"
	$webClient.DownloadFile("https://aka.ms/xblextsdk", $xblextzip )

	# Unpack and install
	Expand-Archive $xblextzip -DestinationPath $xblextfolder -Force

	# Attempt to match the latest installed Windows SDK
	$highestSdkVersion = Get-ChildItem ([System.IO.Path]::Combine($windowsSdkLocationValue, "Include")) -Name | Convert-String -Example '"10.0.10240.0"=10240' | measure-object -maximum

	if ($highestSdkVersion.Maximum -ge 15063)
	{
		$installExe = [System.IO.Path]::Combine($xblextfolder, "XboxLivePlatformExt_10.0.15063", "XboxLivePlatformExt.exe")
	}
	elseif ($highestSdkVersion.Maximum -eq 14393)
	{
		$installExe = [System.IO.Path]::Combine($xblextfolder, "XboxLivePlatformExt_10.0.14393", "XboxLivePlatformExt.exe")
	}
	else
	{
		$installExe = [System.IO.Path]::Combine($xblextfolder, "XboxLivePlatformExt_10.0.10240", "XboxLivePlatformExt.exe")
	}

	$installProc = Start-Process $installExe -wait

	# Cleanup
	Remove-Item $xblextzip
	Remove-Item $xblextfolder -Recurse -Force
}

# Init git submodules if possible (external projects we consume in source format)
if ((Get-ChildItem -Hidden | Where-Object {$_.Name -eq ".git"}) -ne $null)
{
	Write-Output "Ensuring git submodules are up-to-date..." 
	&git submodule update --init --recursive 2>&1 | Write-Verbose
}