// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsCommunicator.h"

#include "LearningAgentsInteractor.h"
#include "LearningExternalTrainer.h"

#include "Misc/Paths.h"

FLearningAgentsTrainerProcess ULearningAgentsCommunicatorLibrary::SpawnSharedMemoryTrainingProcess(
	const FLearningAgentsTrainerProcessSettings& TrainerProcessSettings,
	const FLearningAgentsSharedMemoryCommunicatorSettings& SharedMemorySettings)
{
	FLearningAgentsTrainerProcess TrainerProcess;

	const FString PythonExecutablePath = UE::Learning::Trainer::GetPythonExecutablePath(TrainerProcessSettings.GetIntermediatePath());
	if (!FPaths::FileExists(PythonExecutablePath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSharedMemoryTrainingProcess: Can't find Python executable \"%s\"."), *PythonExecutablePath);
		return TrainerProcess;
	}

	const FString PythonContentPath = UE::Learning::Trainer::GetPythonContentPath(TrainerProcessSettings.GetEditorEnginePath());
	if (!FPaths::DirectoryExists(PythonContentPath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSharedMemoryTrainingProcess: Can't find LearningAgents plugin Content \"%s\"."), *PythonContentPath);
		return TrainerProcess;
	}

	const FString IntermediatePath = UE::Learning::Trainer::GetIntermediatePath(TrainerProcessSettings.GetIntermediatePath());

	TrainerProcess.TrainerProcess = MakeShared<UE::Learning::FSharedMemoryTrainerServerProcess>(
		SharedMemorySettings.TaskName,
		PythonExecutablePath,
		PythonContentPath,
		IntermediatePath,
		SharedMemorySettings.ProcessNum,
		SharedMemorySettings.Timeout);

	return TrainerProcess;
}

FLearningAgentsCommunicator ULearningAgentsCommunicatorLibrary::MakeSharedMemoryCommunicator(
	const FLearningAgentsTrainerProcess& TrainerProcess,
	const FLearningAgentsTrainerProcessSettings& TrainerProcessSettings,
	const FLearningAgentsSharedMemoryCommunicatorSettings& SharedMemorySettings)
{
	FLearningAgentsCommunicator Communicator;

	if (!TrainerProcess.TrainerProcess)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeSharedMemoryCommunicator: TrainerProcess is nullptr"));
		return Communicator;
	}

	Communicator.Trainer = MakeShared<UE::Learning::FSharedMemoryTrainer>(
		SharedMemorySettings.TaskName,
		SharedMemorySettings.ProcessNum,
		TrainerProcess.TrainerProcess,
		SharedMemorySettings.Timeout);

	return Communicator;
}

FLearningAgentsTrainerProcess ULearningAgentsCommunicatorLibrary::SpawnSocketTrainingProcess(
	const FLearningAgentsTrainerProcessSettings& TrainerProcessSettings,
	const FLearningAgentsSocketCommunicatorSettings& SocketSettings)
{
	FLearningAgentsTrainerProcess TrainerProcess;

	const FString PythonExecutablePath = UE::Learning::Trainer::GetPythonExecutablePath(TrainerProcessSettings.GetIntermediatePath());
	if (!FPaths::FileExists(PythonExecutablePath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSocketTrainingProcess: Can't find Python executable \"%s\"."), *PythonExecutablePath);
		return TrainerProcess;
	}

	const FString PythonContentPath = UE::Learning::Trainer::GetPythonContentPath(TrainerProcessSettings.GetEditorEnginePath());
	if (!FPaths::DirectoryExists(PythonContentPath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSocketTrainingProcess: Can't find LearningAgents plugin Content \"%s\"."), *PythonContentPath);
		return TrainerProcess;
	}

	const FString IntermediatePath = UE::Learning::Trainer::GetIntermediatePath(TrainerProcessSettings.GetIntermediatePath());

	TrainerProcess.TrainerProcess = MakeShared<UE::Learning::FSocketTrainerServerProcess>(
		PythonExecutablePath,
		PythonContentPath,
		IntermediatePath,
		*SocketSettings.IpAddress,
		SocketSettings.Port,
		SocketSettings.Timeout);

	return TrainerProcess;
}

FLearningAgentsCommunicator ULearningAgentsCommunicatorLibrary::MakeSocketCommunicator(
	FLearningAgentsTrainerProcess TrainerProcess,
	const FLearningAgentsSocketCommunicatorSettings& SocketSettings)
{
	UE::Learning::ETrainerResponse Response = UE::Learning::ETrainerResponse::Success;

	FLearningAgentsCommunicator Communicator;
	Communicator.Trainer = MakeShared<UE::Learning::FSocketTrainer>(
		Response,
		TrainerProcess.TrainerProcess,
		*SocketSettings.IpAddress,
		SocketSettings.Port,
		SocketSettings.Timeout);

	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeSocketCommunicator: Failed to connect to training process: %s. Check log for additional errors."), UE::Learning::Trainer::GetResponseString(Response));
		Communicator.Trainer->Terminate();
	}

	return Communicator;
}
