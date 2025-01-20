Write-Output "Checking for Xbox Live Extensions SDK..."

try {
    # Get Windows SDK location
    $windowsSdkLocationValue = (Get-ItemProperty "HKLM:\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v10.0" -Name InstallationFolder).InstallationFolder
    if (-not $windowsSdkLocationValue) {
        throw "Unable to locate the Windows SDK installation folder."
    }

    # Check for existing Xbox Live Extensions SDK
    $existingLiveExtSdk = Get-ChildItem $windowsSdkLocationValue -Recurse | Where-Object { $_.Name -like "XboxLive" }
    if ($existingLiveExtSdk -eq $null) {
        Write-Output "Xbox Live Extensions SDK not found. Installing..."

        # Define paths
        $ossLivePath = $env:TEMP # Default to TEMP directory if not defined elsewhere
        $xblextzip = Join-Path $ossLivePath "XboxLiveExtensionSDK.zip"
        $xblextfolder = Join-Path $ossLivePath "XboxLiveExtensionSDK"

        # Initialize WebClient and download the SDK
        $webClient = New-Object System.Net.WebClient
        $webClient.DownloadFile("https://aka.ms/xblextsdk", $xblextzip)

        # Unpack and install the SDK
        Expand-Archive -Path $xblextzip -DestinationPath $xblextfolder -Force
        Write-Output "Xbox Live Extensions SDK has been downloaded and installed at $xblextfolder."
    } else {
        Write-Output "Xbox Live Extensions SDK is already installed."
    }
} catch {
    Write-Error "An error occurred: $_"
    exit 1
}
