// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsPPOTrainer.h"

#include "LearningAgentsCritic.h"
#include "LearningAgentsInteractor.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsPolicy.h"
#include "LearningCompletion.h"
#include "LearningExperience.h"
#include "LearningLog.h"
#include "LearningNeuralNetwork.h"
#include "LearningTrainer.h"
#include "LearningAgentsCommunicator.h"
#include "LearningAgentsTrainingEnvironment.h"
#include "LearningExternalTrainer.h"

#include "Misc/App.h"
#include "GameFramework/GameUserSettings.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Engine/GameViewportClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Editor/EditorPerformanceSettings.h"
#endif

ULearningAgentsPPOTrainer::ULearningAgentsPPOTrainer() : Super(FObjectInitializer::Get()) {}
ULearningAgentsPPOTrainer::ULearningAgentsPPOTrainer(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsPPOTrainer::~ULearningAgentsPPOTrainer() = default;

void ULearningAgentsPPOTrainer::BeginDestroy()
{
	if (IsTraining())
	{
		EndTraining();
	}

	Super::BeginDestroy();
}

ULearningAgentsPPOTrainer* ULearningAgentsPPOTrainer::MakePPOTrainer(
	ULearningAgentsManager*& InManager,
	ULearningAgentsInteractor*& InInteractor,
	ULearningAgentsTrainingEnvironment*& InTrainingEnvironment,
	ULearningAgentsPolicy*& InPolicy,
	ULearningAgentsCritic*& InCritic,
	const FLearningAgentsCommunicator& Communicator,
	TSubclassOf<ULearningAgentsPPOTrainer> Class,
	const FName Name,
	const FLearningAgentsPPOTrainerSettings& TrainerSettings)
{
	if (!InManager)
	{
		UE_LOG(LogLearning, Error, TEXT("MakePPOTrainer: InManager is nullptr."));
		return nullptr;
	}

	if (!Class)
	{
		UE_LOG(LogLearning, Error, TEXT("MakePPOTrainer: Class is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManager, Class, Name, EUniqueObjectNameOptions::GloballyUnique);

	ULearningAgentsPPOTrainer* Trainer = NewObject<ULearningAgentsPPOTrainer>(InManager, Class, UniqueName);
	if (!Trainer) { return nullptr; }

	Trainer->SetupPPOTrainer(InManager, InInteractor, InTrainingEnvironment, InPolicy, InCritic, Communicator, TrainerSettings);

	return Trainer->IsSetup() ? Trainer : nullptr;
}

void ULearningAgentsPPOTrainer::SetupPPOTrainer(
	ULearningAgentsManager*& InManager,
	ULearningAgentsInteractor*& InInteractor,
	ULearningAgentsTrainingEnvironment*& InTrainingEnvironment,
	ULearningAgentsPolicy*& InPolicy,
	ULearningAgentsCritic*& InCritic,
	const FLearningAgentsCommunicator& Communicator,
	const FLearningAgentsPPOTrainerSettings& TrainerSettings)
{
	if (IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup already run!"), *GetName());
		return;
	}

	if (!InManager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InManager is nullptr."), *GetName());
		return;
	}

	if (!InInteractor)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InInteractor is nullptr."), *GetName());
		return;
	}

	if (!InInteractor->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InInteractor->GetName());
		return;
	}

	if (!InTrainingEnvironment)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InTrainingEnvironment is nullptr."), *GetName());
		return;
	}

	if (!InTrainingEnvironment->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InTrainingEnvironment->GetName());
		return;
	}

	if (!InPolicy)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InPolicy is nullptr."), *GetName());
		return;
	}

	if (!InPolicy->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InPolicy->GetName());
		return;
	}

	if (!InCritic)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InCritic is nullptr."), *GetName());
		return;
	}

	if (!InCritic->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InCritic->GetName());
		return;
	}

	if (!Communicator.Trainer)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Communicator's Trainer is nullptr."), *GetName());
		return;
	}

	Manager = InManager;
	Interactor = InInteractor;
	Policy = InPolicy;
	Critic = InCritic;
	TrainingEnvironment = InTrainingEnvironment;
	Trainer = Communicator.Trainer;

	// Create Episode Buffer
	EpisodeBuffer = MakeUnique<UE::Learning::FEpisodeBuffer>();
	EpisodeBuffer->Resize(
		Manager->GetMaxAgentNum(),
		TrainerSettings.MaxEpisodeStepNum,
		Interactor->GetObservationVectorSize(),
		Interactor->GetActionVectorSize(),
		Policy->GetMemoryStateSize());

	// Create Replay Buffer
	ReplayBuffer = MakeUnique<UE::Learning::FReplayBuffer>();
	ReplayBuffer->Resize(
		Interactor->GetObservationVectorSize(),
		Interactor->GetActionVectorSize(),
		Policy->GetMemoryStateSize(),
		TrainerSettings.MaximumRecordedEpisodesPerIteration,
		TrainerSettings.MaximumRecordedStepsPerIteration);

	bIsSetup = true;

	Manager->AddListener(this);
}

void ULearningAgentsPPOTrainer::OnAgentsAdded_Implementation(const TArray<int32>& AgentIds)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	EpisodeBuffer->Reset(AgentIds);
}

void ULearningAgentsPPOTrainer::OnAgentsRemoved_Implementation(const TArray<int32>& AgentIds)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	EpisodeBuffer->Reset(AgentIds);
}

void ULearningAgentsPPOTrainer::OnAgentsReset_Implementation(const TArray<int32>& AgentIds)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	EpisodeBuffer->Reset(AgentIds);
}

const bool ULearningAgentsPPOTrainer::IsTraining() const
{
	return bIsTraining;
}

void ULearningAgentsPPOTrainer::BeginTraining(
	const FLearningAgentsPPOTrainingSettings& TrainingSettings,
	const FLearningAgentsTrainingGameSettings& TrainingGameSettings,
	const bool bResetAgentsOnBegin)
{
	if (!PLATFORM_WINDOWS)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training currently only supported on Windows."), *GetName());
		return;
	}

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (IsTraining())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Already Training!"), *GetName());
		return;
	}

	ApplyGameSettings(TrainingGameSettings);

	// We need to setup the trainer prior to sending the config
	Trainer->AddNetwork(Policy->GetPolicyNetworkAsset()->GetFName(), *Policy->GetPolicyNetworkAsset()->NeuralNetworkData);
	Trainer->AddNetwork(Critic->GetCriticNetworkAsset()->GetFName(), *Critic->GetCriticNetworkAsset()->NeuralNetworkData);
	Trainer->AddNetwork(Policy->GetEncoderNetworkAsset()->GetFName(), *Policy->GetEncoderNetworkAsset()->NeuralNetworkData);
	Trainer->AddNetwork(Policy->GetDecoderNetworkAsset()->GetFName(), *Policy->GetDecoderNetworkAsset()->NeuralNetworkData);
	Trainer->AddReplayBuffer(TEXT("ReplayBuffer"), *ReplayBuffer);

	UE_LOG(LogLearning, Display, TEXT("%s: Sending config..."), *GetName());
	SendConfig(TrainingSettings);

	UE_LOG(LogLearning, Display, TEXT("%s: Sending initial policy..."), *GetName());

	UE::Learning::ETrainerResponse Response = UE::Learning::ETrainerResponse::Success;

	Response = Trainer->SendNetwork(Policy->GetPolicyNetworkAsset()->GetFName(), *Policy->GetPolicyNetworkAsset()->NeuralNetworkData);
	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending policy to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		Trainer->Terminate();
		return;
	}

	Response = Trainer->SendNetwork(Critic->GetCriticNetworkAsset()->GetFName(), *Critic->GetCriticNetworkAsset()->NeuralNetworkData);
	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending critic to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		Trainer->Terminate();
		return;
	}

	Response = Trainer->SendNetwork(Policy->GetEncoderNetworkAsset()->GetFName(), *Policy->GetEncoderNetworkAsset()->NeuralNetworkData);
	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending encoder to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		Trainer->Terminate();
		return;
	}

	Response = Trainer->SendNetwork(Policy->GetDecoderNetworkAsset()->GetFName(), *Policy->GetDecoderNetworkAsset()->NeuralNetworkData);
	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending decoder to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		Trainer->Terminate();
		return;
	}

	if (bResetAgentsOnBegin)
	{
		Manager->ResetAllAgents();
	}

	ReplayBuffer->Reset();

	bIsTraining = true;
}

void ULearningAgentsPPOTrainer::ApplyGameSettings(const FLearningAgentsTrainingGameSettings& Settings)
{
	// Record GameState Settings

	bFixedTimestepUsed = FApp::UseFixedTimeStep();
	FixedTimeStepDeltaTime = FApp::GetFixedDeltaTime();

	UGameUserSettings* GameSettings = UGameUserSettings::GetGameUserSettings();
	if (GameSettings)
	{
		bVSyncEnabled = GameSettings->IsVSyncEnabled();
	}

	UPhysicsSettings* PhysicsSettings = UPhysicsSettings::Get();
	if (PhysicsSettings)
	{
		MaxPhysicsStep = PhysicsSettings->MaxPhysicsDeltaTime;
	}

	IConsoleVariable* MaxFPSCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
	if (MaxFPSCVar)
	{
		MaxFPS = MaxFPSCVar->GetInt();
	}

	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (ViewportClient)
	{
		ViewModeIndex = ViewportClient->ViewModeIndex;
	}

#if WITH_EDITOR
	UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();
	if (EditorPerformanceSettings)
	{
		bUseLessCPUInTheBackground = EditorPerformanceSettings->bThrottleCPUWhenNotForeground;
		bEditorVSyncEnabled = EditorPerformanceSettings->bEnableVSync;
	}
#endif

	// Apply Training GameState Settings

	FApp::SetUseFixedTimeStep(Settings.bUseFixedTimeStep);

	if (Settings.FixedTimeStepFrequency > UE_SMALL_NUMBER)
	{
		FApp::SetFixedDeltaTime(1.0f / Settings.FixedTimeStepFrequency);
		if (Settings.bSetMaxPhysicsStepToFixedTimeStep && PhysicsSettings)
		{
			PhysicsSettings->MaxPhysicsDeltaTime = 1.0f / Settings.FixedTimeStepFrequency;
		}
	}
	else
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Provided invalid FixedTimeStepFrequency: %0.5f"), *GetName(), Settings.FixedTimeStepFrequency);
	}

	if (Settings.bDisableMaxFPS && MaxFPSCVar)
	{
		MaxFPSCVar->Set(0);
	}

	if (Settings.bDisableVSync && GameSettings)
	{
		GameSettings->SetVSyncEnabled(false);
		GameSettings->ApplySettings(false);
	}

	if (Settings.bUseUnlitViewportRendering && ViewportClient)
	{
		ViewportClient->ViewModeIndex = EViewModeIndex::VMI_Unlit;
	}

#if WITH_EDITOR
	if (Settings.bDisableUseLessCPUInTheBackground && EditorPerformanceSettings)
	{
		EditorPerformanceSettings->bThrottleCPUWhenNotForeground = false;
		EditorPerformanceSettings->PostEditChange();
	}

	if (Settings.bDisableEditorVSync && EditorPerformanceSettings)
	{
		EditorPerformanceSettings->bEnableVSync = false;
		EditorPerformanceSettings->PostEditChange();
	}
#endif
}

void ULearningAgentsPPOTrainer::SendConfig(const FLearningAgentsPPOTrainingSettings& Settings)
{
	TSharedRef<FJsonObject> ConfigObject = MakeShared<FJsonObject>();
	ConfigObject->SetStringField(TEXT("TaskName"), TEXT("Training"));
	ConfigObject->SetStringField(TEXT("TrainerMethod"), TEXT("PPO"));
	ConfigObject->SetStringField(TEXT("TrainerType"), TEXT("Network"));
	ConfigObject->SetStringField(TEXT("TimeStamp"), *FDateTime::Now().ToFormattedString(TEXT("%Y-%m-%d_%H-%M-%S")));

	ConfigObject->SetObjectField(TEXT("ObservationSchema"),
		UE::Learning::Trainer::ConvertObservationSchemaToJSON(Interactor->GetObservationSchema()->ObservationSchema,
		Interactor->GetObservationSchemaElement().SchemaElement));
	ConfigObject->SetObjectField(TEXT("ActionSchema"),
		UE::Learning::Trainer::ConvertActionSchemaToJSON(Interactor->GetActionSchema()->ActionSchema,
		Interactor->GetActionSchemaElement().SchemaElement));
	ConfigObject->SetNumberField(TEXT("ObservationVectorDimensionNum"), ReplayBuffer->GetObservations().Num<1>());
	ConfigObject->SetNumberField(TEXT("ActionVectorDimensionNum"), ReplayBuffer->GetActions().Num<1>());
	ConfigObject->SetNumberField(TEXT("MemoryStateVectorDimensionNum"), ReplayBuffer->GetMemoryStates().Num<1>());
	ConfigObject->SetNumberField(TEXT("MaxEpisodeNum"), ReplayBuffer->GetMaxEpisodeNum());
	ConfigObject->SetNumberField(TEXT("MaxStepNum"), ReplayBuffer->GetMaxStepNum());
	
	ConfigObject->SetNumberField(TEXT("PolicyNetworkByteNum"), Policy->GetPolicyNetworkAsset()->NeuralNetworkData->GetSnapshotByteNum());
	ConfigObject->SetNumberField(TEXT("CriticNetworkByteNum"), Critic->GetCriticNetworkAsset()->NeuralNetworkData->GetSnapshotByteNum());
	ConfigObject->SetNumberField(TEXT("EncoderNetworkByteNum"), Policy->GetEncoderNetworkAsset()->NeuralNetworkData->GetSnapshotByteNum());
	ConfigObject->SetNumberField(TEXT("DecoderNetworkByteNum"), Policy->GetDecoderNetworkAsset()->NeuralNetworkData->GetSnapshotByteNum());

	ConfigObject->SetNumberField(TEXT("IterationNum"), Settings.NumberOfIterations);
	ConfigObject->SetNumberField(TEXT("LearningRatePolicy"), Settings.LearningRatePolicy);
	ConfigObject->SetNumberField(TEXT("LearningRateCritic"), Settings.LearningRateCritic);
	ConfigObject->SetNumberField(TEXT("LearningRateDecay"), Settings.LearningRateDecay);
	ConfigObject->SetNumberField(TEXT("WeightDecay"), Settings.WeightDecay);
	ConfigObject->SetNumberField(TEXT("PolicyBatchSize"), Settings.PolicyBatchSize);
	ConfigObject->SetNumberField(TEXT("CriticBatchSize"), Settings.CriticBatchSize);
	ConfigObject->SetNumberField(TEXT("PolicyWindow"), Settings.PolicyWindowSize);
	ConfigObject->SetNumberField(TEXT("IterationsPerGather"), Settings.IterationsPerGather);
	ConfigObject->SetNumberField(TEXT("CriticWarmupIterations"), Settings.CriticWarmupIterations);
	ConfigObject->SetNumberField(TEXT("EpsilonClip"), Settings.EpsilonClip);
	ConfigObject->SetNumberField(TEXT("ActionSurrogateWeight"), Settings.ActionSurrogateWeight);
	ConfigObject->SetNumberField(TEXT("ActionRegularizationWeight"), Settings.ActionRegularizationWeight);
	ConfigObject->SetNumberField(TEXT("ActionEntropyWeight"), Settings.ActionEntropyWeight);
	ConfigObject->SetNumberField(TEXT("ReturnRegularizationWeight"), Settings.ReturnRegularizationWeight);
	ConfigObject->SetNumberField(TEXT("GaeLambda"), Settings.GaeLambda);
	ConfigObject->SetBoolField(TEXT("AdvantageNormalization"), Settings.bAdvantageNormalization);
	ConfigObject->SetNumberField(TEXT("AdvantageMin"), Settings.MinimumAdvantage);
	ConfigObject->SetNumberField(TEXT("AdvantageMax"), Settings.MaximumAdvantage);
	ConfigObject->SetBoolField(TEXT("UseGradNormMaxClipping"), Settings.bUseGradNormMaxClipping);
	ConfigObject->SetNumberField(TEXT("GradNormMax"), Settings.GradNormMax);
	ConfigObject->SetNumberField(TEXT("TrimEpisodeStartStepNum"), Settings.NumberOfStepsToTrimAtStartOfEpisode);
	ConfigObject->SetNumberField(TEXT("TrimEpisodeEndStepNum"), Settings.NumberOfStepsToTrimAtEndOfEpisode);
	ConfigObject->SetNumberField(TEXT("Seed"), Settings.RandomSeed);
	ConfigObject->SetNumberField(TEXT("DiscountFactor"), Settings.DiscountFactor);
	ConfigObject->SetStringField(TEXT("Device"), UE::Learning::Trainer::GetDeviceString(UE::Learning::Agents::GetTrainingDevice(Settings.Device)));
	ConfigObject->SetBoolField(TEXT("UseTensorBoard"), Settings.bUseTensorboard);
	ConfigObject->SetBoolField(TEXT("SaveSnapshots"), Settings.bSaveSnapshots);

	UE::Learning::ETrainerResponse Response = UE::Learning::ETrainerResponse::Success;
	Response = Trainer->SendConfig(ConfigObject);
	
	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending config to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		Trainer->Terminate();
		return;
	}
}

void ULearningAgentsPPOTrainer::DoneTraining()
{
	if (IsTraining())
	{
		// Wait for Trainer to finish
		Trainer->Wait();

		// If not finished in time, terminate
		Trainer->Terminate();

		// Apply back previous game settings
		FApp::SetUseFixedTimeStep(bFixedTimestepUsed);
		FApp::SetFixedDeltaTime(FixedTimeStepDeltaTime);
		UGameUserSettings* GameSettings = UGameUserSettings::GetGameUserSettings();
		if (GameSettings)
		{
			GameSettings->SetVSyncEnabled(bVSyncEnabled);
			GameSettings->ApplySettings(true);
		}

		UPhysicsSettings* PhysicsSettings = UPhysicsSettings::Get();
		if (PhysicsSettings)
		{
			PhysicsSettings->MaxPhysicsDeltaTime = MaxPhysicsStep;
		}

		IConsoleVariable* MaxFPSCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
		if (MaxFPSCVar)
		{
			MaxFPSCVar->Set(MaxFPS);
		}

		UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
		if (ViewportClient)
		{
			ViewportClient->ViewModeIndex = ViewModeIndex;
		}

#if WITH_EDITOR
		UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();
		if (EditorPerformanceSettings)
		{
			EditorPerformanceSettings->bThrottleCPUWhenNotForeground = bUseLessCPUInTheBackground;
			EditorPerformanceSettings->bEnableVSync = bEditorVSyncEnabled;
			EditorPerformanceSettings->PostEditChange();
		}
#endif

		bIsTraining = false;
	}
}

void ULearningAgentsPPOTrainer::EndTraining()
{
	if (IsTraining())
	{
		UE_LOG(LogLearning, Display, TEXT("%s: Stopping training..."), *GetName());
		Trainer->SendStop();
		DoneTraining();
	}
}

void ULearningAgentsPPOTrainer::ProcessExperience(const bool bResetAgentsOnUpdate)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPPOTrainer::ProcessExperience);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!IsTraining())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training not running."), *GetName());
		return;
	}

	if (Manager->GetAgentNum() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: No agents added to Manager."), *GetName());
	}

	// Check Observations, Actions, Rewards, and Completions have been completed and have matching iteration number

	TArray<int32> ValidAgentIds;
	ValidAgentIds.Empty(Manager->GetMaxAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (Interactor->GetObservationIteration(AgentId) == 0 ||
			Interactor->GetActionIteration(AgentId) == 0 ||
			TrainingEnvironment->GetRewardIteration(AgentId) == 0 ||
			TrainingEnvironment->GetCompletionIteration(AgentId) == 0)
		{
			UE_LOG(LogLearning, Display, TEXT("%s: Agent with id %i has not completed a full step of observations, actions, rewards, completions and so experience will not be processed for it."), *GetName(), AgentId);
			continue;
		}

		if (Interactor->GetObservationIteration(AgentId) != Interactor->GetActionIteration(AgentId) ||
			Interactor->GetObservationIteration(AgentId) != TrainingEnvironment->GetRewardIteration(AgentId) ||
			Interactor->GetObservationIteration(AgentId) != TrainingEnvironment->GetCompletionIteration(AgentId))
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has non-matching iteration numbers (observation: %i, action: %i, reward: %i, completion: %i). Experience will not be processed for it."), *GetName(), AgentId,
				Interactor->GetObservationIteration(AgentId),
				Interactor->GetActionIteration(AgentId),
				TrainingEnvironment->GetRewardIteration(AgentId),
				TrainingEnvironment->GetCompletionIteration(AgentId));
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	UE::Learning::FIndexSet ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Check for episodes that have been immediately completed

	for (const int32 AgentId : ValidAgentSet)
	{
		if (TrainingEnvironment->GetAgentCompletion(AgentId) != UE::Learning::ECompletionMode::Running && EpisodeBuffer->GetEpisodeStepNums()[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has completed episode and will be reset but has not generated any experience."), *GetName(), AgentId);
		}
	}

	// Add Experience to Episode Buffer
	EpisodeBuffer->Push(
		Interactor->GetObservationVectorArrayView(),
		Interactor->GetActionVectorArrayView(),
		Policy->GetPreEvaluationMemoryState(),
		TrainingEnvironment->GetRewardArrayView(),
		ValidAgentSet);

	// Find the set of agents which have reached the maximum episode length and mark them as truncated
	UE::Learning::Completion::EvaluateEndOfEpisodeCompletions(
		TrainingEnvironment->GetEpisodeCompletions(),
		EpisodeBuffer->GetEpisodeStepNums(),
		EpisodeBuffer->GetMaxStepNum(),
		ValidAgentSet);

	TrainingEnvironment->SetAllCompletions(ValidAgentSet);

	TrainingEnvironment->GetResetBuffer().SetResetInstancesFromCompletions(TrainingEnvironment->GetAllCompletions(), ValidAgentSet);

	// If there are no agents completed we are done
	if (TrainingEnvironment->GetResetBuffer().GetResetInstanceNum() == 0)
	{
		return;
	}

	// Otherwise Gather Observations for completed Instances without incrementing iteration number
	Interactor->GatherObservations(TrainingEnvironment->GetResetBuffer().GetResetInstances(), false);

	// And push those episodes to the Replay Buffer
	const bool bReplayBufferFull = ReplayBuffer->AddEpisodes(
		TrainingEnvironment->GetAllCompletions(),
		Interactor->GetObservationVectorArrayView(),
		Policy->GetMemoryState(),
		*EpisodeBuffer,
		TrainingEnvironment->GetResetBuffer().GetResetInstances());

	if (bReplayBufferFull)
	{
		UE::Learning::ETrainerResponse Response = Trainer->SendReplayBuffer(TEXT("ReplayBuffer"), *ReplayBuffer);

		if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error waiting to push experience to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}

		ReplayBuffer->Reset();

		// Get Updated Policy
		Response = Trainer->ReceiveNetwork(Policy->GetPolicyNetworkAsset()->GetFName(), *Policy->GetPolicyNetworkAsset()->NeuralNetworkData);
		Policy->GetPolicyNetworkAsset()->ForceMarkDirty();

		if (Response == UE::Learning::ETrainerResponse::Completed)
		{
			UE_LOG(LogLearning, Display, TEXT("%s: Trainer completed training."), *GetName());
			DoneTraining();
			return;
		}
		else if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error waiting for policy from trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}

		// Get Updated Critic
		Response = Trainer->ReceiveNetwork(Critic->GetCriticNetworkAsset()->GetFName(), *Critic->GetCriticNetworkAsset()->NeuralNetworkData);
		Critic->GetCriticNetworkAsset()->ForceMarkDirty();

		if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error waiting for critic from trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}

		// Get Updated Encoder
		Response = Trainer->ReceiveNetwork(Policy->GetEncoderNetworkAsset()->GetFName(), *Policy->GetEncoderNetworkAsset()->NeuralNetworkData);
		Policy->GetEncoderNetworkAsset()->ForceMarkDirty();

		if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error waiting for encoder from trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}

		// Get Updated Decoder
		Response = Trainer->ReceiveNetwork(Policy->GetDecoderNetworkAsset()->GetFName(), *Policy->GetDecoderNetworkAsset()->NeuralNetworkData);
		Policy->GetDecoderNetworkAsset()->ForceMarkDirty();

		if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error waiting for decoder from trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}

		if (bResetAgentsOnUpdate)
		{
			// Reset all agents since we have a new policy
			TrainingEnvironment->GetResetBuffer().SetResetInstances(Manager->GetAllAgentSet());
			Manager->ResetAgents(TrainingEnvironment->GetResetBuffer().GetResetInstancesArray());
			return;
		}
	}

	// Manually reset Episode Buffer for agents who have reached the maximum episode length as 
	// they wont get it reset via the agent manager's call to ResetAgents
	TrainingEnvironment->GetResetBuffer().SetResetInstancesFromCompletions(TrainingEnvironment->GetEpisodeCompletions(), ValidAgentSet);
	EpisodeBuffer->Reset(TrainingEnvironment->GetResetBuffer().GetResetInstances());

	// Call ResetAgents for agents which have manually signaled a completion
	TrainingEnvironment->GetResetBuffer().SetResetInstancesFromCompletions(TrainingEnvironment->GetAgentCompletions(), ValidAgentSet);
	if (TrainingEnvironment->GetResetBuffer().GetResetInstanceNum() > 0)
	{
		Manager->ResetAgents(TrainingEnvironment->GetResetBuffer().GetResetInstancesArray());
	}
}

void ULearningAgentsPPOTrainer::RunTraining(
	const FLearningAgentsPPOTrainingSettings& TrainingSettings,
	const FLearningAgentsTrainingGameSettings& TrainingGameSettings,
	const bool bResetAgentsOnBegin,
	const bool bResetAgentsOnUpdate)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (bHasTrainingFailed)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training has failed. Check log for errors."), *GetName());
		return;
	}

	// If we aren't training yet, then start training and do the first inference step.
	if (!IsTraining())
	{
		BeginTraining(TrainingSettings,	TrainingGameSettings, bResetAgentsOnBegin);

		if (!IsTraining())
		{
			// If IsTraining is false, then BeginTraining must have failed and we can't continue.
			return;
		}

		Policy->RunInference();
	}
	// Otherwise, do the regular training process.
	else
	{
		TrainingEnvironment->GatherCompletions();
		TrainingEnvironment->GatherRewards();
		ProcessExperience(bResetAgentsOnUpdate);
		Policy->RunInference();
	}
}

int32 ULearningAgentsPPOTrainer::GetEpisodeStepNum(const int32 AgentId) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0.0f;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return 0.0f;
	}

	return EpisodeBuffer->GetEpisodeStepNums()[AgentId];
}

bool ULearningAgentsPPOTrainer::HasTrainingFailed() const
{
	return bHasTrainingFailed;
}
