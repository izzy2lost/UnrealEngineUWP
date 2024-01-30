// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMediaPlaybackServerProcess.h"

#include "AvalancheMediaSettings.h"
#include "IAvaMediaModule.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Playback/AvaMediaPlaybackClient.h"

namespace UE::AvaMediaPlaybackServerProcess::Private
{
	FString GetLocalServerName()
	{
		return UAvalancheMediaSettings::Get().LocalPlaybackServerSettings.ServerName;
	}

	const TCHAR* GetPlaybackServerLogReplicationVerbosityString()
	{
		const UAvalancheMediaSettings& Settings = UAvalancheMediaSettings::Get(); 
		return ToString(UAvalancheMediaSettings::ToLogVerbosity(Settings.PlaybackServerLogReplicationVerbosity));
	}
}

FAvaMediaPlaybackServerProcess::FAvaMediaPlaybackServerProcess(FProcHandle&& InProcessHandle)
	: ProcessHandle(MoveTemp(InProcessHandle))
{}

bool FAvaMediaPlaybackServerProcess::IsLaunched()
{
	return ProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(ProcessHandle);
}

bool FAvaMediaPlaybackServerProcess::Launch()
{
	if (IsLaunched())
	{
		UE_LOG(LogAvaMedia, Warning, TEXT("Local playback server is already running."));
		return true;
	}

	using namespace UE::AvaMediaPlaybackServerProcess::Private;

	FString GameNameOrProjectFile;
	if (FPaths::IsProjectFilePathSet())
	{
		GameNameOrProjectFile = FString::Printf(TEXT("\"%s\""), *FPaths::GetProjectFilePath());
	}
	else
	{
		GameNameOrProjectFile = FApp::GetProjectName();
	}

	FString CommandLine;
	CommandLine += GameNameOrProjectFile;

	// Todo: It will likely need a more complete set of parameters. Reference: UDisplayClusterLaunchEditorProjectSettings.
	const FLocalPlaybackServerSettings& Settings = UAvalancheMediaSettings::Get().LocalPlaybackServerSettings;

	// Standard arguments
	const FIntPoint Resolution = Settings.Resolution;
	CommandLine += FString::Printf(TEXT(" -game -windowed -ResX=%d -ResY=%d -messaging"), Resolution.X, Resolution.Y);
	if (Settings.bEnableLogConsole)
	{
		CommandLine += TEXT(" -log");
	}
	// Avalanche arguments
	CommandLine += FString::Printf(TEXT(" -AvaMediaPlaybackServerStart=%s"), *GetLocalServerName());
	CommandLine += TEXT(" -AvaMediaPlaybackClientSuppress");
	CommandLine += FString::Printf(TEXT(" -AvaMediaPlaybackServerLogReplication=%s"), GetPlaybackServerLogReplicationVerbosityString());
	// Storm Sync arguments
	CommandLine += TEXT(" -NoStormSyncServerAutoStart");

	if (!Settings.ExtraCommandLineArguments.IsEmpty())
	{
		CommandLine += TEXT(" ");
		CommandLine += Settings.ExtraCommandLineArguments;
	}

	FString LogCommands;
	
	for (const FAvaPlaybackServerLoggingEntry& LoggingEntry : Settings.Logging)
	{
		if (!LoggingEntry.Category.IsNone())
		{
			LogCommands += FString::Printf(TEXT("%s%s %s"),
					LogCommands.IsEmpty() ? TEXT("") : TEXT(", "), *LoggingEntry.Category.ToString(),
					ToString(UAvalancheMediaSettings::ToLogVerbosity(LoggingEntry.VerbosityLevel)));
		}
	}

	if (!LogCommands.IsEmpty())
	{
		CommandLine += FString::Printf(TEXT(" -LogCmds=\"%s\""), *LogCommands);		
	}

	const FString ExecutablePath = FPlatformProcess::ExecutablePath();

	uint32 ProcessID = 0;
	constexpr bool bLaunchDetached = true;
	constexpr bool bLaunchMinimized = false;
	constexpr bool bLaunchWindowHidden = false;
	constexpr uint32 PriorityModifier = 0;

	UE_LOG(LogAvaMedia, Log, TEXT("Launching a playback server in game mode in a new process with the following command line:"));
	UE_LOG(LogAvaMedia, Log, TEXT("%s %s"), *ExecutablePath, *CommandLine);

	ProcessHandle = FPlatformProcess::CreateProc(
		*ExecutablePath, *CommandLine, bLaunchDetached,
		bLaunchMinimized, bLaunchWindowHidden, &ProcessID,
		PriorityModifier, nullptr, nullptr, nullptr);
	
	return ProcessHandle.IsValid();
}

void FAvaMediaPlaybackServerProcess::Stop()
{
	FPlatformProcess::TerminateProc(ProcessHandle);
}

TSharedPtr<FAvaMediaPlaybackServerProcess> FAvaMediaPlaybackServerProcess::Find(const FAvaMediaPlaybackClient& InPlaybackClient)
{
	using namespace UE::AvaMediaPlaybackServerProcess::Private;

	// We use the playback client to find connected server processes that are considered local.
	// Local means that it is in the same project content path and the same machine. We figure out
	// if it is the same machine if the process Id can be opened.

	const FString ProjectContentPath = InPlaybackClient.GetProjectContentPath();
	const uint32 ClientProcessId = FPlatformProcess::GetCurrentProcessId();
	const FString LocalServerName = GetLocalServerName();
	
	// First try with the configured local server name.
	if (InPlaybackClient.GetServerProjectContentPath(LocalServerName) == ProjectContentPath)
	{
		const uint32 ServerProcessId = InPlaybackClient.GetServerProcessId(LocalServerName);
		if (ServerProcessId != 0 && ClientProcessId != ServerProcessId)
		{
			FProcHandle ProcessHandle = FPlatformProcess::OpenProcess(ServerProcessId);
			if (ProcessHandle.IsValid())
			{
				return MakeShared<FAvaMediaPlaybackServerProcess>(MoveTemp(ProcessHandle));
			}

			// Warn about the server name collision issue.
			UE_LOG(LogAvaMedia, Warning, TEXT("Found Connected Server \"%s\" but it is not a local process."), *LocalServerName);
			UE_LOG(LogAvaMedia, Warning, TEXT("This indicates a name collision between the servers. The local server should be renamed."));
		}
	}

	// Fallback
	// Try to find any connected server that is "local", i.e. on the same machine and
	// in the same project content folder, but on a different process id.
	
	TArray<FString> ServerNames = InPlaybackClient.GetServerNames();
	for (const FString& ServerName : ServerNames)
	{
		if (InPlaybackClient.GetServerProjectContentPath(ServerName) == ProjectContentPath)
		{
			const uint32 ServerProcessId = InPlaybackClient.GetServerProcessId(ServerName);
			if (ClientProcessId != ServerProcessId)
			{
				FProcHandle ProcessHandle = FPlatformProcess::OpenProcess(ServerProcessId);
				if (ProcessHandle.IsValid())
				{
					return MakeShared<FAvaMediaPlaybackServerProcess>(MoveTemp(ProcessHandle));
				}
			}
		}
	}
	
	return nullptr;	
}

TSharedPtr<FAvaMediaPlaybackServerProcess> FAvaMediaPlaybackServerProcess::FindOrCreate(const FAvaMediaPlaybackClient& InPlaybackClient)
{
	TSharedPtr<FAvaMediaPlaybackServerProcess> ServerProcess = Find(InPlaybackClient);
	if (!ServerProcess.IsValid())
	{
		ServerProcess = MakeShared<FAvaMediaPlaybackServerProcess>();
	}
	return ServerProcess;
}