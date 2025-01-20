Write-Output "Checking for Xbox Live Extensions SDK..."
$windowsSdkLocationValue = (Get-ItemProperty "HKLM:\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v10.0" -Name InstallationFolder).InstallationFolder
$existingLiveExtSdk = get-childitem $windowsSdkLocationValue -recurse | where-object {$_.Name -like "XboxLive"}
if ($existingLiveExtSdk -eq $null)
{
    Write-Output "Xbox Live Extensions SDK not found.  Installing..."

    # Download Xbox Live Extensions SDK
    $xblextzip = $ossLivePath + "\XboxLiveExtensionSDK.zip"
    $xblextfolder = $ossLivePath + "\XboxLiveExtensionSDK"
    $webClient.DownloadFile("https://aka.ms/xblextsdk", $xblextzip)

    # Unpack and install
    Expand-Archive $xblextzip -DestinationPath $xblextfolder -Force
}
else
{
    Write-Output "Xbox Live Extensions SDK is already installed."
}