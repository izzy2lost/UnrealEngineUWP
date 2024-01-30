// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularFeature/AvaSyncProviderFeatureTypes.h"

#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "AvaSyncProviderFeatureTypes"

FAvaSyncConnectionInfo::FAvaSyncConnectionInfo()
	: EngineVersion(FNetworkVersion::GetLocalNetworkVersion())
	, InstanceId(FApp::GetInstanceId())
	, SessionId(FApp::GetSessionId())
	, HostName(FPlatformProcess::ComputerName())
	, ProjectName(FApp::GetProjectName())
{
	// We only want the dirname, not absolute path
	ProjectDir = GetBasename(FPaths::ProjectDir());

	// Figure out which type of instance it is
	if (GEngine == nullptr)
	{
		InstanceType = EAvaSyncEngineType::Unknown;
	}
	else if (IsRunningDedicatedServer())
	{
		InstanceType = EAvaSyncEngineType::Server;
	}
	else if (IsRunningCommandlet())
	{
		InstanceType = EAvaSyncEngineType::Commandlet;
	}
	else if (GEngine->IsEditor())
	{
		InstanceType = EAvaSyncEngineType::Editor;
	}
	else if (IsRunningGame())
	{
		InstanceType = EAvaSyncEngineType::Game;
	}
	else
	{
		InstanceType = EAvaSyncEngineType::Other;
	}	
}

FString FAvaSyncConnectionInfo::ToString() const
{
	return FString::Printf(
		TEXT("EngineVersion: %d, HostName: %s, InstanceId: %s, InstanceType: %s, SessionId: %s, ProjectName: %s, ProjectDir: %s"),
		EngineVersion,
		*HostName,
		*InstanceId.ToString(),
		*UEnum::GetValueAsString(InstanceType),
		*SessionId.ToString(),
		*ProjectName,
		*ProjectDir
	);
}

FString FAvaSyncConnectionInfo::GetBasename(const FString& InPath)
{
	FString LocalPath = InPath;
	LocalPath.RemoveFromEnd(TEXT("/"));
	return FPaths::GetBaseFilename(LocalPath);
}

FText FAvaSyncConnectionInfo::GetHumanReadableInstanceType() const
{
	FText Result;

	switch (InstanceType)
	{
	case EAvaSyncEngineType::Server:
		Result = LOCTEXT("Instance_Type_Server", "Server");
		break;
	case EAvaSyncEngineType::Commandlet:
		Result = LOCTEXT("Instance_Type_Commandlet", "Commandlet");
		break;
	case EAvaSyncEngineType::Editor:
		Result = LOCTEXT("Instance_Type_Editor", "Editor");
		break;
	case EAvaSyncEngineType::Game:
		Result = LOCTEXT("Instance_Type_Game", "Game");
		break;
	case EAvaSyncEngineType::Other:
		Result = LOCTEXT("Instance_Type_Other", "Other");
		break;
	case EAvaSyncEngineType::Unknown:
		Result = LOCTEXT("Instance_Type_Unknown", "Unknown");
		break;
	default: break;
	}
	
	return Result;
}

#undef LOCTEXT_NAMESPACE
