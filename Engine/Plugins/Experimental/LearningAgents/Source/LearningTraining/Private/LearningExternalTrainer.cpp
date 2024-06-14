// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningExternalTrainer.h"

#include "LearningExperience.h"
#include "LearningNeuralNetwork.h"
#include "LearningSharedMemoryTraining.h"
#include "LearningSocketTraining.h"

#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "SocketSubsystem.h"

namespace UE::Learning
{
	FSharedMemoryTrainerServerProcess::FSharedMemoryTrainerServerProcess(
		const FString& TaskName,
		const FString& PythonExecutablePath,
		const FString& PythonContentPath,
		const FString& InIntermediatePath,
		const int32 ProcessNum,
		const float InTimeout,
		const ESubprocessFlags TrainingProcessFlags)
	{
		check(ProcessNum > 0);

		int32 ProcessIdx = 0;
		FParse::Value(FCommandLine::Get(), TEXT("LearningProcessIdx"), ProcessIdx);
		
		Timeout = InTimeout;
		IntermediatePath = InIntermediatePath;

		if (ProcessIdx == 0)
		{
			// Allocate the control memory if we are the parent UE process
			Controls = SharedMemory::Allocate<2, volatile int32>({ ProcessNum, SharedMemoryTraining::GetControlNum() });
		}
		else
		{
			FGuid ControlsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningControlsGuid"), ControlsGuid));
			Controls = SharedMemory::Map<2, volatile int32>(ControlsGuid, { ProcessNum, SharedMemoryTraining::GetControlNum() });

			// We do not want to launch another training process if we are a child process
			return;
		}

		UE_LEARNING_CHECK(FPaths::FileExists(PythonExecutablePath));
		UE_LEARNING_CHECK(FPaths::DirectoryExists(PythonContentPath));

		// We need to zero the control memory before we start the training sub-process since it may contain
		// uninitialized values or those left over from previous runs.
		Array::Zero(Controls.View);

		const FString TimeStamp = FDateTime::Now().ToFormattedString(TEXT("%Y-%m-%d_%H-%M-%S"));
		const FString TrainerMethod = TEXT("PPO");
		const FString TrainerType = TEXT("SharedMemory");
		ConfigPath = InIntermediatePath / TEXT("Configs") / FString::Printf(TEXT("%s_%s_%s_%s.json"), *TaskName, *TrainerMethod, *TrainerType, *TimeStamp);

		IFileManager& FileManager = IFileManager::Get();
		const FString CommandLineArguments = FString::Printf(TEXT("\"%s\" SharedMemory \"%s\" %i \"%s\""),
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*(PythonContentPath / TEXT("train_ppo.py"))),
			*Controls.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces),
			ProcessNum,
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*ConfigPath));

		TrainingProcess.Launch(
			FileManager.ConvertToAbsolutePathForExternalAppForRead(*PythonExecutablePath),
			CommandLineArguments, 
			TrainingProcessFlags);
	}

	FSharedMemoryTrainerServerProcess::~FSharedMemoryTrainerServerProcess()
	{
		Terminate();
	}

	bool FSharedMemoryTrainerServerProcess::IsRunning() const
	{
		return TrainingProcess.IsRunning();
	}

	bool FSharedMemoryTrainerServerProcess::Wait()
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		while (TrainingProcess.Update())
		{
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return false;
			}
		}

		return true;
	}

	void FSharedMemoryTrainerServerProcess::Terminate()
	{
		TrainingProcess.Terminate();
	}

	TSharedMemoryArrayView<2, volatile int32> FSharedMemoryTrainerServerProcess::GetControlsSharedMemoryArrayView() const
	{
		return Controls;
	}

	const FString& FSharedMemoryTrainerServerProcess::GetIntermediatePath() const
	{
		return IntermediatePath;
	}

	const FString& FSharedMemoryTrainerServerProcess::GetConfigPath() const
	{
		return ConfigPath;
	}

	FSubprocess* FSharedMemoryTrainerServerProcess::GetTrainingSubprocess()
	{
		return &TrainingProcess;
	}

	void FSharedMemoryTrainerServerProcess::Deallocate()
	{
		if (Controls.Region != nullptr)
		{
			SharedMemory::Deallocate(Controls);
		}
	}

	void FSharedMemoryTrainer::FSharedMemoryExperienceContainer::Deallocate()
	{
		if (EpisodeStarts.Region != nullptr)
		{
			SharedMemory::Deallocate(EpisodeStarts);
			SharedMemory::Deallocate(EpisodeLengths);
			SharedMemory::Deallocate(EpisodeCompletionModes);
			SharedMemory::Deallocate(EpisodeFinalObservations);
			SharedMemory::Deallocate(EpisodeFinalMemoryStates);
			SharedMemory::Deallocate(Observations);
			SharedMemory::Deallocate(Actions);
			SharedMemory::Deallocate(MemoryStates);
			SharedMemory::Deallocate(Rewards);
		}
	}

	FSharedMemoryTrainer::FSharedMemoryTrainer(
		const FString& InTaskName,
		const int32 InProcessNum,
		const TSharedPtr<UE::Learning::ITrainerProcess>& ExternalTrainerProcess,
		const float InTimeout)
	{
		FSharedMemoryTrainerServerProcess* SharedMemoryTrainerProcess = (FSharedMemoryTrainerServerProcess*)ExternalTrainerProcess.Get();
		if (!SharedMemoryTrainerProcess)
		{
			UE_LOG(LogLearning, Error, TEXT("FSharedMemoryTrainer ctor: Trainer process is nullptr. Is it not a shared memory process?"));
			return;
		}

		check(InProcessNum > 0);

		TaskName = InTaskName;
		ConfigPath = SharedMemoryTrainerProcess->GetConfigPath();
		IntermediatePath = SharedMemoryTrainerProcess->GetIntermediatePath();
		TrainingProcess = SharedMemoryTrainerProcess->GetTrainingSubprocess();
		ProcessNum = InProcessNum;
		Controls = SharedMemoryTrainerProcess->GetControlsSharedMemoryArrayView();
		Timeout = InTimeout;

		ProcessIdx = 0;
		FParse::Value(FCommandLine::Get(), TEXT("LearningProcessIdx"), ProcessIdx);
	}

	FSharedMemoryTrainer::~FSharedMemoryTrainer()
	{
		Terminate();
	}

	ETrainerResponse FSharedMemoryTrainer::Wait()
	{
		return ETrainerResponse::Success;
	}

	void FSharedMemoryTrainer::Terminate()
	{
		Deallocate();
	}

	ETrainerResponse FSharedMemoryTrainer::SendStop()
	{
		check(ProcessIdx != INDEX_NONE);
		checkf(Controls.Region, TEXT("SendStop: Controls Shared Memory Region is nullptr"));

		return SharedMemoryTraining::SendStop(Controls.View[ProcessIdx]);
	}

	ETrainerResponse FSharedMemoryTrainer::SendConfig(const TSharedRef<FJsonObject>& ConfigObject, const ELogSetting LogSettings)
	{
		check(ProcessNum > 0);

		if (ProcessIdx != 0)
		{
			// Only the parent process will send the config
			return ETrainerResponse::Success;
		}

		IFileManager& FileManager = IFileManager::Get();
		ConfigObject->SetStringField(TEXT("IntermediatePath"), *FileManager.ConvertToAbsolutePathForExternalAppForRead(*IntermediatePath));
		ConfigObject->SetBoolField(TEXT("LoggingEnabled"), LogSettings == ELogSetting::Silent ? false : true);

		ConfigObject->SetNumberField(TEXT("ProcessNum"), ProcessNum);

		TArray<TSharedPtr<FJsonValue>> NetworkGuidsArray;
		for (const TPair<FName, TSharedMemoryArrayView<1, uint8>>& MapEntry : NeuralNetworkSharedMemoryArrayViews)
		{
			NetworkGuidsArray.Add(MakeShared<FJsonValueString>(*MapEntry.Value.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces)));
		}
		ConfigObject->SetArrayField(TEXT("NetworkGuids"), NetworkGuidsArray);

		TArray<TSharedPtr<FJsonValue>> ExperienceContainerObjectsArray;
		for (const TPair<FName, FSharedMemoryExperienceContainer>& MapEntry : SharedMemoryExperienceContainers)
		{
			TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
			JsonObject->SetStringField(TEXT("EpisodeStartsGuid"), *MapEntry.Value.EpisodeStarts.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			JsonObject->SetStringField(TEXT("EpisodeLengthsGuid"), *MapEntry.Value.EpisodeLengths.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			JsonObject->SetStringField(TEXT("EpisodeCompletionModesGuid"), *MapEntry.Value.EpisodeCompletionModes.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			JsonObject->SetStringField(TEXT("EpisodeFinalObservationsGuid"), *MapEntry.Value.EpisodeFinalObservations.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			JsonObject->SetStringField(TEXT("EpisodeFinalMemoryStatesGuid"), *MapEntry.Value.EpisodeFinalMemoryStates.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			JsonObject->SetStringField(TEXT("ObservationsGuid"), *MapEntry.Value.Observations.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			JsonObject->SetStringField(TEXT("ActionsGuid"), *MapEntry.Value.Actions.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			JsonObject->SetStringField(TEXT("MemoryStatesGuid"), *MapEntry.Value.MemoryStates.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			JsonObject->SetStringField(TEXT("RewardsGuid"), *MapEntry.Value.Rewards.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			
			TSharedRef<FJsonValueObject> JsonValue = MakeShared<FJsonValueObject>(JsonObject);
			ExperienceContainerObjectsArray.Add(JsonValue);
		}
		ConfigObject->SetArrayField(TEXT("ExperienceBuffers"), ExperienceContainerObjectsArray);
		
		FString ConfigString;
		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&ConfigString, 0);
		FJsonSerializer::Serialize(ConfigObject, JsonWriter, true);
		FFileHelper::SaveStringToFile(ConfigString, *ConfigPath);

		return SharedMemoryTraining::SendConfigSignal(Controls.View[ProcessIdx], LogSettings);
	}

	void FSharedMemoryTrainer::AddNetwork(
		const FName& Name,
		const ULearningNeuralNetworkData& Network)
	{
		NeuralNetworkSharedMemoryArrayViews.Add(Name, SharedMemory::Allocate<1, uint8>({ Network.GetSnapshotByteNum() }));
	}

	bool FSharedMemoryTrainer::ContainsNetwork(const FName& Name) const
	{
		return NeuralNetworkSharedMemoryArrayViews.Contains(Name);
	}

	ETrainerResponse FSharedMemoryTrainer::ReceiveNetwork(
		const FName& Name,
		ULearningNeuralNetworkData& OutNetwork,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		check(ProcessIdx != INDEX_NONE);
		checkf(Controls.Region, TEXT("ReceiveNetwork: Controls Shared Memory Region is nullptr"));
		if (!ensureMsgf(NeuralNetworkSharedMemoryArrayViews.Contains(Name), TEXT("Network %s has not been added. Call AddNetwork prior to ReceiveNetwork."), *Name.ToString()))
		{
			return ETrainerResponse::Unexpected;
		}

		return SharedMemoryTraining::RecvNetwork(
			Controls.View[ProcessIdx],
			OutNetwork,
			*TrainingProcess,
			NeuralNetworkSharedMemoryArrayViews[Name].View,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryTrainer::SendNetwork(
		const FName& Name,
		const ULearningNeuralNetworkData& Network,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		check(ProcessIdx != INDEX_NONE);
		checkf(Controls.Region, TEXT("SendNetwork: Controls Shared Memory Region is nullptr"));
		if (!ensureMsgf(NeuralNetworkSharedMemoryArrayViews.Contains(Name), TEXT("Network %s has not been added. Call AddNetwork prior to SendNetwork."), *Name.ToString()))
		{
			return ETrainerResponse::Unexpected;
		}

		return SharedMemoryTraining::SendNetwork(
			Controls.View[ProcessIdx],
			NeuralNetworkSharedMemoryArrayViews[Name].View,
			*TrainingProcess,
			Network,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	void FSharedMemoryTrainer::AddReplayBuffer(
		const FName& Name,
		const FReplayBuffer& ReplayBuffer)
	{
		check(ProcessNum > 0);

		const int32 ObservationVectorDimensionNum = ReplayBuffer.GetObservations().Num<1>();
		const int32 ActionVectorDimensionNum = ReplayBuffer.GetActions().Num<1>();
		const int32 MemoryStateVectorDimensionNum = ReplayBuffer.GetMemoryStates().Num<1>();

		FSharedMemoryExperienceContainer ExperienceContainer;
		if (ProcessIdx == 0)
		{
			ExperienceContainer.EpisodeStarts = SharedMemory::Allocate<2, int32>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			ExperienceContainer.EpisodeLengths = SharedMemory::Allocate<2, int32>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			ExperienceContainer.EpisodeCompletionModes = SharedMemory::Allocate<2, ECompletionMode>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			ExperienceContainer.EpisodeFinalObservations = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum(), ObservationVectorDimensionNum });
			ExperienceContainer.EpisodeFinalMemoryStates = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum(), MemoryStateVectorDimensionNum });
			ExperienceContainer.Observations = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxStepNum(), ObservationVectorDimensionNum });
			ExperienceContainer.Actions = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxStepNum(), ActionVectorDimensionNum });
			ExperienceContainer.MemoryStates = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxStepNum(), MemoryStateVectorDimensionNum });
			ExperienceContainer.Rewards = SharedMemory::Allocate<2, float>({ ProcessNum, ReplayBuffer.GetMaxStepNum() });
		}
		else
		{
			FGuid EpisodeStartsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeStartsGuid"), EpisodeStartsGuid));
			FGuid EpisodeLengthsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeLengthsGuid"), EpisodeLengthsGuid));
			FGuid EpisodeCompletionModesGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeCompletionModesGuid"), EpisodeCompletionModesGuid));
			FGuid EpisodeFinalObservationsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeFinalObservationsGuid"), EpisodeFinalObservationsGuid));
			FGuid EpisodeFinalMemoryStatesGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeFinalMemoryStatesGuid"), EpisodeFinalMemoryStatesGuid));
			FGuid ObservationsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningObservationsGuid"), ObservationsGuid));
			FGuid ActionsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningActionsGuid"), ActionsGuid));
			FGuid MemoryStatesGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningMemoryStatesGuid"), MemoryStatesGuid));
			FGuid RewardsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningRewardsGuid"), RewardsGuid));

			ExperienceContainer.EpisodeStarts = SharedMemory::Map<2, int32>(EpisodeStartsGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			ExperienceContainer.EpisodeLengths = SharedMemory::Map<2, int32>(EpisodeLengthsGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			ExperienceContainer.EpisodeCompletionModes = SharedMemory::Map<2, ECompletionMode>(EpisodeCompletionModesGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			ExperienceContainer.EpisodeFinalObservations = SharedMemory::Map<3, float>(EpisodeFinalObservationsGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum(), ObservationVectorDimensionNum });
			ExperienceContainer.EpisodeFinalMemoryStates = SharedMemory::Map<3, float>(EpisodeFinalMemoryStatesGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum(), MemoryStateVectorDimensionNum });
			ExperienceContainer.Observations = SharedMemory::Map<3, float>(ObservationsGuid, { ProcessNum, ReplayBuffer.GetMaxStepNum(), ObservationVectorDimensionNum });
			ExperienceContainer.Actions = SharedMemory::Map<3, float>(ActionsGuid, { ProcessNum, ReplayBuffer.GetMaxStepNum(), ActionVectorDimensionNum });
			ExperienceContainer.MemoryStates = SharedMemory::Map<3, float>(MemoryStatesGuid, { ProcessNum, ReplayBuffer.GetMaxStepNum(), MemoryStateVectorDimensionNum });
			ExperienceContainer.Rewards = SharedMemory::Map<2, float>(RewardsGuid, { ProcessNum, ReplayBuffer.GetMaxStepNum() });
		}

		SharedMemoryExperienceContainers.Add(Name, ExperienceContainer);
	}

	bool FSharedMemoryTrainer::ContainsReplayBuffer(const FName& Name) const
	{
		return SharedMemoryExperienceContainers.Contains(Name);
	}

	ETrainerResponse FSharedMemoryTrainer::SendReplayBuffer(const FName& Name, const FReplayBuffer& ReplayBuffer, const ELogSetting LogSettings)
	{
		check(ProcessIdx != INDEX_NONE);
		checkf(Controls.Region, TEXT("SendReplayBuffer: Controls Shared Memory Region is nullptr"));
		if (!ensureMsgf(SharedMemoryExperienceContainers.Contains(Name), TEXT("ReplayBuffer %s has not been added. Call AddReplayBuffer prior to SendReplayBuffer."), *Name.ToString()))
		{
			return ETrainerResponse::Unexpected;
		}

		return SharedMemoryTraining::SendExperience(
			SharedMemoryExperienceContainers[Name].EpisodeStarts.View[ProcessIdx],
			SharedMemoryExperienceContainers[Name].EpisodeLengths.View[ProcessIdx],
			SharedMemoryExperienceContainers[Name].EpisodeCompletionModes.View[ProcessIdx],
			SharedMemoryExperienceContainers[Name].EpisodeFinalObservations.View[ProcessIdx],
			SharedMemoryExperienceContainers[Name].EpisodeFinalMemoryStates.View[ProcessIdx],
			SharedMemoryExperienceContainers[Name].Observations.View[ProcessIdx],
			SharedMemoryExperienceContainers[Name].Actions.View[ProcessIdx],
			SharedMemoryExperienceContainers[Name].MemoryStates.View[ProcessIdx],
			SharedMemoryExperienceContainers[Name].Rewards.View[ProcessIdx],
			Controls.View[ProcessIdx],
			*TrainingProcess,
			ReplayBuffer,
			Timeout,
			LogSettings);
	}

	void FSharedMemoryTrainer::Deallocate()
	{
		for (TPair<FName, TSharedMemoryArrayView<1, uint8>>& MapEntry : NeuralNetworkSharedMemoryArrayViews)
		{
			if (MapEntry.Value.Region != nullptr)
			{
				SharedMemory::Deallocate(MapEntry.Value);
			}
		}
		NeuralNetworkSharedMemoryArrayViews.Empty();

		for (TPair<FName, FSharedMemoryExperienceContainer>& MapEntry : SharedMemoryExperienceContainers)
		{
			MapEntry.Value.Deallocate();
		}
		SharedMemoryExperienceContainers.Empty();
	}

	FSocketTrainerServerProcess::FSocketTrainerServerProcess(
		const FString& PythonExecutablePath,
		const FString& PythonContentPath,
		const FString& IntermediatePath,
		const TCHAR* IpAddress,
		const uint32 Port,
		const float InTimeout,
		const ESubprocessFlags TrainingProcessFlags,
		const ELogSetting LogSettings)
	{
		Timeout = InTimeout;

		UE_LEARNING_CHECK(FPaths::FileExists(PythonExecutablePath));
		UE_LEARNING_CHECK(FPaths::DirectoryExists(PythonContentPath));

		IFileManager& FileManager = IFileManager::Get();
		const FString CommandLineArguments = FString::Printf(TEXT("\"%s\" Socket \"%s:%i\" \"%s\" %i"),
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*(PythonContentPath / TEXT("train_ppo.py"))),
			IpAddress,
			Port,
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*IntermediatePath),
			LogSettings == ELogSetting::Normal ? 1 : 0);

		TrainingProcess.Launch(
			FileManager.ConvertToAbsolutePathForExternalAppForRead(*PythonExecutablePath),
			CommandLineArguments, 
			TrainingProcessFlags);
	}

	FSocketTrainerServerProcess::~FSocketTrainerServerProcess()
	{
		Terminate();
	}

	bool FSocketTrainerServerProcess::IsRunning() const
	{
		return TrainingProcess.IsRunning();
	}

	bool FSocketTrainerServerProcess::Wait()
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		while (TrainingProcess.Update())
		{
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return false;
			}
		}

		return true;
	}

	void FSocketTrainerServerProcess::Terminate()
	{
		TrainingProcess.Terminate();
	}

	FSubprocess* FSocketTrainerServerProcess::GetTrainingSubprocess()
	{
		return &TrainingProcess;
	}

	FSocketTrainer::FSocketTrainer(
		ETrainerResponse& OutResponse,
		const TSharedPtr<UE::Learning::ITrainerProcess>& ExternalTrainerProcess,
		const TCHAR* IpAddress,
		const uint32 Port,
		const float InTimeout)
	{
		Timeout = InTimeout;

		FSocketTrainerServerProcess* SocketTrainerProcess = (FSocketTrainerServerProcess*)ExternalTrainerProcess.Get();
		if (SocketTrainerProcess)
		{
			TrainingProcess = SocketTrainerProcess->GetTrainingSubprocess();
		}

		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		checkf(SocketSubsystem, TEXT("Could not get socket subsystem"));

		bool bIsValid = false;
		TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
		Address->SetIp(IpAddress, bIsValid);
		Address->SetPort(Port);

		if (!bIsValid)
		{
			UE_LOG(LogLearning, Error, TEXT("Invalid Ip Address \"%s\"..."), IpAddress);
			OutResponse = ETrainerResponse::Unexpected;
			return;
		}

		Socket = FTcpSocketBuilder(TEXT("LearningTrainerSocket")).AsNonBlocking().Build();

		OutResponse = SocketTraining::WaitForConnection(*Socket, TrainingProcess, *Address, Timeout);
	}

	FSocketTrainer::~FSocketTrainer()
	{
		Terminate();
	}

	ETrainerResponse FSocketTrainer::Wait()
	{
		return ETrainerResponse::Success;
	}

	void FSocketTrainer::Terminate()
	{
		if (Socket)
		{
			Socket->Close();
			Socket = nullptr;
		}
	}

	ETrainerResponse FSocketTrainer::SendStop()
	{
		checkf(Socket, TEXT("Training socket is nullptr"));

		return SocketTraining::SendStop(*Socket, TrainingProcess, Timeout);
	}

	ETrainerResponse FSocketTrainer::SendConfig(
		const TSharedRef<FJsonObject>& ConfigObject,
		const ELogSetting LogSettings)
	{
		checkf(Socket, TEXT("Training socket is nullptr"));

		FString ConfigString;
		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&ConfigString, 0);
		FJsonSerializer::Serialize(ConfigObject, JsonWriter, true);

		return SocketTraining::SendConfig(*Socket, ConfigString, TrainingProcess, Timeout, LogSettings);
	}

	void FSocketTrainer::AddNetwork(
		const FName& Name,
		const ULearningNeuralNetworkData& Network)
	{
		NetworkBuffers.Add(Name, TLearningArray<1, uint8>());
		NetworkBuffers[Name].SetNumUninitialized({Network.GetSnapshotByteNum()});
	}

	bool FSocketTrainer::ContainsNetwork(const FName& Name) const
	{
		return NetworkBuffers.Contains(Name);
	}

	ETrainerResponse FSocketTrainer::ReceiveNetwork(
		const FName& Name,
		ULearningNeuralNetworkData& OutNetwork,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		checkf(Socket, TEXT("Training socket is nullptr"));
		if (!ensureMsgf(NetworkBuffers.Contains(Name), TEXT("Network %s has not been added. Call AddNetwork prior to ReceiveNetwork."), *Name.ToString()))
		{
			return ETrainerResponse::Unexpected;
		}

		return SocketTraining::RecvNetwork(*Socket, OutNetwork, TrainingProcess, NetworkBuffers[Name], Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketTrainer::SendNetwork(
		const FName& Name,
		const ULearningNeuralNetworkData& Network,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		checkf(Socket, TEXT("Training socket is nullptr"));
		if (!ensureMsgf(NetworkBuffers.Contains(Name), TEXT("Network %s has not been added. Call AddNetwork prior to SendNetwork."), *Name.ToString()))
		{
			return ETrainerResponse::Unexpected;
		}

		return SocketTraining::SendNetwork(*Socket, NetworkBuffers[Name], TrainingProcess, Network, Timeout, NetworkLock, LogSettings);
	}

	void FSocketTrainer::AddReplayBuffer(const FName& Name, const FReplayBuffer& ReplayBuffer)
	{
		ExperienceBufferNames.Add(Name);
	}

	bool FSocketTrainer::ContainsReplayBuffer(const FName& Name) const
	{
		return ExperienceBufferNames.Contains(Name);
	}

	ETrainerResponse FSocketTrainer::SendReplayBuffer(const FName& Name, const FReplayBuffer& ReplayBuffer, const ELogSetting LogSettings)
	{
		checkf(Socket, TEXT("Training socket is nullptr"));
		if (!ensureMsgf(ExperienceBufferNames.Contains(Name), TEXT("ReplayBuffer %s has not been added. Call AddReplayBuffer prior to SendReplayBuffer."), *Name.ToString()))
		{
			return ETrainerResponse::Unexpected;
		}

		return SocketTraining::SendExperience(*Socket, ReplayBuffer, TrainingProcess, Timeout, LogSettings);
	}
}
