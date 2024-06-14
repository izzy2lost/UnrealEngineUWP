// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsTrainer.h"

#include "Misc/Paths.h"

namespace UE::Learning::Agents
{
	ELearningAgentsTrainingDevice GetLearningAgentsTrainingDevice(const ETrainerDevice Device)
	{
		switch (Device)
		{
		case ETrainerDevice::CPU: return ELearningAgentsTrainingDevice::CPU;
		case ETrainerDevice::GPU: return ELearningAgentsTrainingDevice::GPU;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Trainer Device.")); return ELearningAgentsTrainingDevice::CPU;
		}
	}

	ETrainerDevice GetTrainingDevice(const ELearningAgentsTrainingDevice Device)
	{
		switch (Device)
		{
		case ELearningAgentsTrainingDevice::CPU: return ETrainerDevice::CPU;
		case ELearningAgentsTrainingDevice::GPU: return ETrainerDevice::GPU;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Trainer Device.")); return ETrainerDevice::CPU;
		}
	}
}

FLearningAgentsTrainerProcessSettings::FLearningAgentsTrainerProcessSettings()
{
	EditorEngineRelativePath.Path = FPaths::EngineDir();
	EditorIntermediateRelativePath.Path = FPaths::ProjectIntermediateDir();
}

FString FLearningAgentsTrainerProcessSettings::GetEditorEnginePath() const
{
#if WITH_EDITOR
	return EditorEngineRelativePath.Path;
#else
	if (NonEditorEngineRelativePath.IsEmpty())
	{
		UE_LOG(LogLearning, Warning, TEXT("GetEditorEnginePath: NonEditorEngineRelativePath not set"));
	}

	return NonEditorEngineRelativePath;
#endif
}

FString FLearningAgentsTrainerProcessSettings::GetIntermediatePath() const
{
#if WITH_EDITOR
	return EditorIntermediateRelativePath.Path;
#else
	if (NonEditorIntermediateRelativePath.IsEmpty())
	{
		UE_LOG(LogLearning, Warning, TEXT("GetIntermediatePath: NonEditorIntermediateRelativePath not set"));
	}

	return NonEditorIntermediateRelativePath;
#endif
}
