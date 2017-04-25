@echo off
where /q nuget
if ERRORLEVEL 1 (
	echo Nuget.exe was not found.  Download from https://nuget.org/downloads and either place in this folder or add to your PATH environment variable.
	pause
	exit /b 1
)

nuget install microsoft.xbox.live.sdk.winrt.uwp -version 2016.12.20170107.1 -outputdirectory ThirdParty\XSAPI
nuget install microsoft.xbox.live.sdk.winrt.XboxOneXDK -version 2016.12.20170126.1 -outputdirectory ThirdParty\XSAPI


