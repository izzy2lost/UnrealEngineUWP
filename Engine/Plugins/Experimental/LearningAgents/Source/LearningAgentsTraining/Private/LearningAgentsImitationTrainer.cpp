// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsImitationTrainer.h"

#include "LearningAgentsInteractor.h"
#include "LearningArrayMap.h"
#include "LearningExperience.h"
#include "LearningFeatureObject.h"
#include "LearningLog.h"
#include "LearningImitationTrainer.h"
#include "LearningAgentsRecording.h"
#include "LearningAgentsPolicy.h"
#include "LearningNeuralNetworkObject.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

ULearningAgentsImitationTrainer::ULearningAgentsImitationTrainer() : Super(FObjectInitializer::Get()) {}
ULearningAgentsImitationTrainer::ULearningAgentsImitationTrainer(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsImitationTrainer::~ULearningAgentsImitationTrainer() = default;

void ULearningAgentsImitationTrainer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsTraining())
	{
		EndTraining();
	}

	Super::EndPlay(EndPlayReason);
}

void ULearningAgentsImitationTrainer::BeginTraining(
	ULearningAgentsPolicy* InPolicy, 
	const ULearningAgentsRecording* Recording,
	const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings,
	const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings,
	const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings,
	const bool bReinitializePolicyNetwork)
{
	if (IsTraining())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Cannot begin training as we are already training!"), *GetName());
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

	if (!Recording)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Recording is nullptr."), *GetName());
		return;
	}

	if (Recording->Records.IsEmpty())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Recording is empty!"), *GetName());
		return;
	}

	Policy = InPolicy;

	// Record Timeout Setting

	TrainerTimeout = ImitationTrainerSettings.TrainerCommunicationTimeout;

	// Check Paths

	const FString PythonExecutablePath = UE::Learning::Trainer::GetPythonExecutablePath(ImitationTrainerPathSettings.GetEditorEnginePath());

	if (!FPaths::FileExists(PythonExecutablePath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Can't find Python executable \"%s\"."), *GetName(), *PythonExecutablePath);
		return;
	}
	const FString PythonContentPath = UE::Learning::Trainer::GetPythonContentPath(ImitationTrainerPathSettings.GetEditorEnginePath());

	if (!FPaths::DirectoryExists(PythonContentPath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Can't find LearningAgents plugin Content \"%s\"."), *GetName(), *PythonContentPath);
		return;
	}

	const FString SitePackagesPath = UE::Learning::Trainer::GetSitePackagesPath(ImitationTrainerPathSettings.GetEditorEnginePath());

	if (!FPaths::DirectoryExists(SitePackagesPath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Can't find Python site-packages \"%s\"."), *GetName(), *SitePackagesPath);
		return;
	}

	const FString IntermediatePath = UE::Learning::Trainer::GetIntermediatePath(ImitationTrainerPathSettings.GetIntermediatePath());

	// Sizes

	const int32 ObservationNum = Policy->GetPolicyObject().ObservationNum;
	const int32 ActionNum = Policy->GetPolicyObject().ActionNum;
	const int32 MemoryStateNum = Policy->GetPolicyObject().MemoryStateNum;

	// Get Number of Steps

	int32 TotalEpisodeNum = 0;
	int32 TotalStepNum = 0;
	for (const FLearningAgentsRecord& Record : Recording->Records)
	{
		if (Record.ObservationDimNum != ObservationNum)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Record has wrong dimensionality for observations, got %i, policy expected %i."), *GetName(), Record.ObservationDimNum, ObservationNum);
			continue;
		}

		if (Record.ActionDimNum != ActionNum)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Record has wrong dimensionality for actions, got %i, policy expected %i."), *GetName(), Record.ActionDimNum, ActionNum);
			continue;
		}

		TotalEpisodeNum++;
		TotalStepNum += Record.StepNum;
	}

	if (TotalStepNum == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Recording contains no valid training data."), *GetName());
		return;
	}

	// Copy into Flat Arrays

	RecordedEpisodeStarts.SetNumUninitialized({ TotalEpisodeNum });
	RecordedEpisodeLengths.SetNumUninitialized({ TotalEpisodeNum });
	RecordedObservations.SetNumUninitialized({ TotalStepNum, ObservationNum });
	RecordedActions.SetNumUninitialized({ TotalStepNum, ActionNum });

	int32 EpisodeIdx = 0;
	int32 StepIdx = 0;
	for (const FLearningAgentsRecord& Record : Recording->Records)
	{
		if (Record.ObservationDimNum != ObservationNum) { continue; }
		if (Record.ActionDimNum != ActionNum) { continue; }

		RecordedEpisodeStarts[EpisodeIdx] = StepIdx;
		RecordedEpisodeLengths[EpisodeIdx] = Record.StepNum;
		UE::Learning::Array::Copy(RecordedObservations.Slice(StepIdx, Record.StepNum), Record.Observations);
		UE::Learning::Array::Copy(RecordedActions.Slice(StepIdx, Record.StepNum), Record.Actions);
		EpisodeIdx++;
		StepIdx += Record.StepNum;
	}

	UE_LEARNING_CHECK(EpisodeIdx == TotalEpisodeNum);
	UE_LEARNING_CHECK(StepIdx == TotalStepNum);

	// Begin Training Properly

	UE_LOG(LogLearning, Display, TEXT("%s: Imitation Training Started"), *GetName());


	UE::Learning::FImitationTrainerTrainingSettings ImitationTrainingSettings;
	ImitationTrainingSettings.IterationNum = ImitationTrainerTrainingSettings.NumberOfIterations;
	ImitationTrainingSettings.LearningRatePolicy = ImitationTrainerTrainingSettings.LearningRate;
	ImitationTrainingSettings.LearningRateDecay = ImitationTrainerTrainingSettings.LearningRateDecay;
	ImitationTrainingSettings.WeightDecay = ImitationTrainerTrainingSettings.WeightDecay;
	ImitationTrainingSettings.PolicyBatchSize = ImitationTrainerTrainingSettings.BatchSize;
	ImitationTrainingSettings.Seed = ImitationTrainerTrainingSettings.RandomSeed;
	ImitationTrainingSettings.Device = UE::Learning::Agents::GetTrainerDevice(ImitationTrainerTrainingSettings.Device);
	ImitationTrainingSettings.bUseTensorboard = ImitationTrainerTrainingSettings.bUseTensorboard;

	const UE::Learning::EImitationTrainerFlags TrainerFlags = 
		bReinitializePolicyNetwork ? 
		UE::Learning::EImitationTrainerFlags::None : 
		UE::Learning::EImitationTrainerFlags::UseInitialPolicyNetwork;

	ImitationTrainer = MakeUnique<UE::Learning::FSharedMemoryImitationTrainer>(
		GetName(),
		PythonExecutablePath,
		SitePackagesPath,
		PythonContentPath,
		IntermediatePath,
		TotalEpisodeNum,
		TotalStepNum,
		ObservationNum,
		ActionNum,
		MemoryStateNum,
		Policy->GetPolicyNetwork(),
		ImitationTrainingSettings);

	UE_LOG(LogLearning, Display, TEXT("%s: Sending / Receiving initial policy..."), *GetName());

	UE::Learning::ETrainerResponse Response = UE::Learning::ETrainerResponse::Success;

	Response = ImitationTrainer->SendPolicy(Policy->GetPolicyNetwork(), TrainerTimeout);

	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending policy to trainer: %s. Check log for errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		ImitationTrainer->Terminate();
		bHasTrainingFailed = true;
		return;
	}

	if (!bReinitializePolicyNetwork)
	{
		Response = ImitationTrainer->RecvPolicy(Policy->GetPolicyNetwork(), TrainerTimeout);

		if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error receiving policy from trainer: %s. Check log for errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			ImitationTrainer->Terminate();
			return;
		}

		Policy->GetNetworkAsset()->ForceMarkDirty();
	}

	UE_LOG(LogLearning, Display, TEXT("%s: Sending Experience..."), *GetName());

	// Send Experience

	Response = ImitationTrainer->SendExperience(
		RecordedEpisodeStarts,
		RecordedEpisodeLengths,
		RecordedObservations, 
		RecordedActions, 
		TrainerTimeout);

	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending experience to trainer: %s. Check log for errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		ImitationTrainer->Terminate();
		return;
	}

	bIsTraining = true;
}

void ULearningAgentsImitationTrainer::DoneTraining()
{
	if (IsTraining())
	{
		// Wait for Trainer to finish
		ImitationTrainer->Wait(1.0f);

		// If not finished in time, terminate
		ImitationTrainer->Terminate();

		bIsTraining = false;
	}
}

void ULearningAgentsImitationTrainer::EndTraining()
{
	if (IsTraining())
	{
		UE_LOG(LogLearning, Display, TEXT("%s: Stopping training..."), *GetName());
		ImitationTrainer->SendStop();
		DoneTraining();
	}
}

void ULearningAgentsImitationTrainer::IterateTraining()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsImitationTrainer::IterateTraining);

	if (!IsTraining())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training not running."), *GetName());
		return;
	}

	if (ImitationTrainer->HasPolicyOrCompleted())
	{
		UE::Learning::ETrainerResponse Response = ImitationTrainer->RecvPolicy(Policy->GetPolicyNetwork(), TrainerTimeout);
		Policy->GetNetworkAsset()->ForceMarkDirty();

		if (Response == UE::Learning::ETrainerResponse::Completed)
		{
			UE_LOG(LogLearning, Display, TEXT("%s: Trainer completed training."), *GetName());
			DoneTraining();
			return;
		}
		else if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error receiving policy from trainer. Check log for errors."), *GetName());
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}
	}
}

void ULearningAgentsImitationTrainer::RunTraining(
	ULearningAgentsPolicy* InPolicy,
	const ULearningAgentsRecording* Recording,
	const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings,
	const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings,
	const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings,
	const bool bReinitializePolicyNetwork)
{
	if (bHasTrainingFailed)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training has failed. Check log for errors."), *GetName());
		return;
	}

	// If we aren't training yet, then start training and do the first inference step.
	if (!IsTraining())
	{
		BeginTraining(
			InPolicy,
			Recording,
			ImitationTrainerSettings,
			ImitationTrainerTrainingSettings,
			ImitationTrainerPathSettings,
			bReinitializePolicyNetwork);

		if (!IsTraining())
		{
			// If IsTraining is false, then BeginTraining must have failed and we can't continue.
			return;
		}
	}

	// Otherwise, do the regular training process.
	IterateTraining();
}

bool ULearningAgentsImitationTrainer::IsTraining() const
{
	return bIsTraining;
}

bool ULearningAgentsImitationTrainer::HasTrainingFailed() const
{
	return bHasTrainingFailed;
}
