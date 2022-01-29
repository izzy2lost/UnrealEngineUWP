@echo off
setlocal
pushd "%~dp0"

rem Figure out if we should append the -prompt argument
set PROMPT_ARGUMENT=
for %%P in (%*) do if /I "%%P" == "--prompt" goto no_prompt_argument
for %%P in (%*) do if /I "%%P" == "--force" goto no_prompt_argument
set PROMPT_ARGUMENT=--prompt
:no_prompt_argument

rem Sync the dependencies...
.\Engine\Binaries\DotNET\GitDependencies.exe %PROMPT_ARGUMENT% %*
if ERRORLEVEL 1 goto error

rem Setup the git hooks...
if not exist .git\hooks goto no_git_hooks_directory
echo Registering git hooks...
echo #!/bin/sh >.git\hooks\post-checkout
echo Engine/Binaries/DotNET/GitDependencies.exe %* >>.git\hooks\post-checkout
rem @ATG_CHANGE - BEGIN Sync dependencies custom to the UWP fork
echo powershell -NoProfile -ExecutionPolicy Bypass -File GetUWPDependencies.ps1 >>.git\hooks\post-checkout 
rem @ARG_CHANGE - END
echo #!/bin/sh >.git\hooks\post-merge
echo Engine/Binaries/DotNET/GitDependencies.exe %* >>.git\hooks\post-merge
rem @ATG_CHANGE - BEGIN Sync dependencies custom to the UWP fork
echo powershell -NoProfile -ExecutionPolicy Bypass -File GetUWPDependencies.ps1 >>.git\hooks\post-merge 
rem @ARG_CHANGE - END
:no_git_hooks_directory

rem Install prerequisites...
echo Installing prerequisites...
start /wait Engine\Extras\Redist\en-us\UE4PrereqSetup_x64.exe /quiet

rem @ATG_CHANGE - BEGIN Sync dependencies custom to the UWP fork
echo Installing dependencies custom to the UWP fork...
powershell -NoProfile -ExecutionPolicy Bypass -File %~dp0\GetUWPDependencies.ps1
rem @ARG_CHANGE - END

rem Register the engine installation...
if not exist .\Engine\Binaries\Win64\UnrealVersionSelector-Win64-Shipping.exe goto :no_unreal_version_selector
.\Engine\Binaries\Win64\UnrealVersionSelector-Win64-Shipping.exe /register
:no_unreal_version_selector

rem Done!
goto :end

rem Error happened. Wait for a keypress before quitting.
:error
pause

:end
popd
