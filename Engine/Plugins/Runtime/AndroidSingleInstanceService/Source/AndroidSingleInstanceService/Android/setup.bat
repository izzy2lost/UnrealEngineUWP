@echo off
@echo Creating local.properties
setlocal ENABLEDELAYEDEXPANSION
set NDKDIR=!NDKROOT:\=\\!
set SDKDIR=!ANDROID_HOME:\=\\!
echo ndk.dir=%NDKDIR%> local.properties
echo sdk.dir=%SDKDIR%>> local.properties