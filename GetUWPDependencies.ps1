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
$xsapiVersionUwp = "2017.05.20170517.001"
$xsapiVersionXdk = "2017.05.20170517.001"

$webClient = New-Object System.Net.WebClient
$startupPath = Split-Path $MyInvocation.MyCommand.Path
$ossLivePath = [System.IO.Path]::Combine($startupPath, "Engine", "Plugins", "Online", "XboxOne", "OnlineSubsystemLive")

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
Install-Package $nuget microsoft.xbox.live.sdk.winrt.uwp.native.release $xsapiInstallPath @("build\native\lib") $xsapiVersionUwp UWP
Install-Package $nuget microsoft.xbox.live.sdk.winrt.XboxOneXDK $xsapiInstallPath @("build\native\bin", "build\native\references") $xsapiVersionXdk XboxOne

# Install Windows Device Portal Wrapper (used by UWP.Automation)
Write-Output "Installing Windows Device Portal Wrapper from Nuget..."
$wdpwrapperInstallPath = [System.IO.Path]::Combine($startupPath, "Engine", "Binaries", "ThirdParty", "WindowsDevicePortalWrapper")
Install-Package $nuget windowsdeviceportalwrapper $wdpwrapperInstallPath @("lib\net452\*")

# Check for Live Extensions SDK
Write-Output "Checking for Xbox Live Extensions SDK..."
$existingLiveExtSdk = Get-ItemProperty HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\* | Where-Object {$_.DisplayName -like "*Xbox Live Platform Extensions*"}
if ($existingLiveExtSdk -eq $null)
{
	Write-Output "Xbox Live Extensions SDK not found.  Installing..."

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

# Init git submodules if possible (external projects we consume in source format)
if ((Get-ChildItem -Hidden | Where-Object {$_.Name -eq ".git"}) -ne $null)
{
	Write-Output "Ensuring git submodules are up-to-date..." 
	&git submodule update --init --recursive 2>&1 | Write-Verbose
}