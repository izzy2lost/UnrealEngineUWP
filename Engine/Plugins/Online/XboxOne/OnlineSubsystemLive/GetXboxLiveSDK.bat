@echo off

rem   Check we are elevated and re-launch if not.
rem   Pass in the nuget path if found, since the spawned environment might not match the current one exactly.

net file 1>nul 2>nul
if errorlevel 1 (
	for /F "usebackq" %%i in (`where nuget.exe`) do set nuget=%%i
	powershell "Start-Process -filepath %0 '%nuget%' -verb runas" >nul 2>&1
	exit /b 2
)

rem   Fetch nuget path from parent or path...

if "%1" == "" for /F "usebackq" %%i in (`where nuget.exe`) do set nuget=%%i
if NOT "%1" == "" set nuget=%1

if "%nuget%" == "" (
	echo Nuget.exe was not found.  Download from https://nuget.org/downloads and either place in this folder or add to your PATH environment variable.
	pause
	exit /b 1
)

set UwpVer=2016.12.20170107.01
set XboxOneVer=2016.12.20170126.001

%nuget% install microsoft.xbox.live.sdk.winrt.uwp -version %UwpVer% -outputdirectory "%~dp0\ThirdParty\XSAPI"
%nuget% install microsoft.xbox.live.sdk.winrt.XboxOneXDK -version %XboxOneVer% -outputdirectory "%~dp0\ThirdParty\XSAPI"

echo Setting up shortened path links, to avoid the 260 character limit...
mklink /d "%~dp0\ThirdParty\XSAPI\UWP" "%~dp0\ThirdParty\XSAPI\Microsoft.Xbox.Live.SDK.WinRT.UWP.%UwpVer%"
mklink /d "%~dp0\ThirdParty\XSAPI\XboxOne" "%~dp0\ThirdParty\XSAPI\Microsoft.Xbox.Live.SDK.WinRT.XboxOneXDK.%XboxOneVer%"
