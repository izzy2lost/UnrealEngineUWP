// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningPPOTrainer.h"

#include "LearningArray.h"
#include "LearningExternalTrainer.h"
#include "LearningLog.h"
#include "LearningNeuralNetwork.h"
#include "LearningExperience.h"
#include "LearningProgress.h"
#include "LearningSharedMemory.h"
#include "LearningSharedMemoryTraining.h"
#include "LearningSocketTraining.h"
#include "LearningObservation.h"
#include "LearningAction.h"

#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include "Dom/JsonObject.h"

#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "SocketSubsystem.h"

ULearningSocketPPOTrainerServerCommandlet::ULearningSocketPPOTrainerServerCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

int32 ULearningSocketPPOTrainerServerCommandlet::Main(const FString& Commandline)
{
	UE_LOG(LogLearning, Display, TEXT("Running PPO Training Server Commandlet..."));

#if WITH_EDITOR
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;

	UCommandlet::ParseCommandLine(*Commandline, Tokens, Switches, Params);

	const FString* PythonExecutiblePathParam = Params.Find(TEXT("PythonExecutiblePath"));
	const FString* PythonContentPathParam = Params.Find(TEXT("PythonContentPath"));
	const FString* IntermediatePathParam = Params.Find(TEXT("IntermediatePath"));
	const FString* IpAddressParam = Params.Find(TEXT("IpAddress"));
	const FString* PortParam = Params.Find(TEXT("Port"));
	const FString* LogSettingsParam = Params.Find(TEXT("LogSettings"));

	const FString PythonExecutiblePath = PythonExecutiblePathParam ? *PythonExecutiblePathParam : UE::Learning::Trainer::GetPythonExecutablePath(FPaths::ProjectIntermediateDir());
	const FString PythonContentPath = PythonContentPathParam ? *PythonContentPathParam : UE::Learning::Trainer::GetPythonContentPath(FPaths::EngineDir());
	const FString IntermediatePath = IntermediatePathParam ? *IntermediatePathParam : UE::Learning::Trainer::GetIntermediatePath(FPaths::ProjectIntermediateDir());

	const TCHAR* IpAddress = IpAddressParam ? *(*IpAddressParam) : UE::Learning::Trainer::DefaultIp;
	const uint32 Port = PortParam ? FCString::Atoi(*(*PortParam)) : UE::Learning::Trainer::DefaultPort;
	
	UE::Learning::ELogSetting LogSettings = UE::Learning::ELogSetting::Normal;
	if (LogSettingsParam)
	{
		if (*LogSettingsParam == TEXT("Normal"))
		{
			LogSettings = UE::Learning::ELogSetting::Normal;
		}
		else if (*LogSettingsParam == TEXT("Silent"))
		{
			LogSettings = UE::Learning::ELogSetting::Silent;
		}
		else
		{
			UE_LEARNING_NOT_IMPLEMENTED();
			return 1;
		}
	}
	
	UE_LOG(LogLearning, Display, TEXT("---  PPO Training Server Arguments ---"));
	UE_LOG(LogLearning, Display, TEXT("PythonExecutiblePath: %s"), *PythonExecutiblePath);
	UE_LOG(LogLearning, Display, TEXT("PythonContentPath: %s"), *PythonContentPath);
	UE_LOG(LogLearning, Display, TEXT("IntermediatePath: %s"), *IntermediatePath);
	UE_LOG(LogLearning, Display, TEXT("IpAddress: %s"), IpAddress);
	UE_LOG(LogLearning, Display, TEXT("Port: %i"), Port);
	UE_LOG(LogLearning, Display, TEXT("LogSettings: %s"), LogSettings == UE::Learning::ELogSetting::Normal ? TEXT("Normal") : TEXT("Silent"));

	UE::Learning::FSocketTrainerServerProcess ServerProcess(
		PythonExecutiblePath,
		PythonContentPath,
		IntermediatePath,
		IpAddress,
		Port,
		UE::Learning::Trainer::DefaultTimeout,
		UE::Learning::ESubprocessFlags::None,
		LogSettings);

	while (ServerProcess.IsRunning())
	{
		FPlatformProcess::Sleep(0.01f);
	}

#else
	UE_LEARNING_NOT_IMPLEMENTED();
#endif

	return 0;
}

namespace UE::Learning::PPOTrainer
{
	ETrainerResponse Train(
		IExternalTrainer* ExternalTrainer,
		FReplayBuffer& ReplayBuffer,
		FEpisodeBuffer& EpisodeBuffer,
		FResetInstanceBuffer& ResetBuffer,
		ULearningNeuralNetworkData& PolicyNetwork,
		ULearningNeuralNetworkData& CriticNetwork,
		ULearningNeuralNetworkData& EncoderNetwork,
		ULearningNeuralNetworkData& DecoderNetwork,
		TLearningArrayView<2, float> ObservationVectorBuffer,
		TLearningArrayView<2, float> ActionVectorBuffer,
		TLearningArrayView<2, float> PreEvaluationMemoryStateVectorBuffer,
		TLearningArrayView<2, float> MemoryStateVectorBuffer,
		TLearningArrayView<1, float> RewardBuffer,
		TLearningArrayView<1, ECompletionMode> CompletionBuffer,
		TLearningArrayView<1, ECompletionMode> EpisodeCompletionBuffer,
		TLearningArrayView<1, ECompletionMode> AllCompletionBuffer,
		const TFunctionRef<void(const FIndexSet Instances)> ResetFunction,
		const TFunctionRef<void(const FIndexSet Instances)> ObservationFunction,
		const TFunctionRef<void(const FIndexSet Instances)> PolicyFunction,
		const TFunctionRef<void(const FIndexSet Instances)> ActionFunction,
		const TFunctionRef<void(const FIndexSet Instances)> UpdateFunction,
		const TFunctionRef<void(const FIndexSet Instances)> RewardFunction,
		const TFunctionRef<void(const FIndexSet Instances)> CompletionFunction,
		const FIndexSet Instances,
		const Observation::FSchema& ObservationSchema,
		const Observation::FSchemaElement& ObservationSchemaElement,
		const Action::FSchema& ActionSchema,
		const Action::FSchemaElement& ActionSchemaElement,
		const FPPOTrainerTrainingSettings& TrainerSettings,
		TAtomic<bool>* bRequestTrainingStopSignal,
		FRWLock* PolicyNetworkLock,
		FRWLock* CriticNetworkLock,
		FRWLock* EncoderNetworkLock,
		FRWLock* DecoderNetworkLock,
		TAtomic<bool>* bPolicyNetworkUpdatedSignal,
		TAtomic<bool>* bCriticNetworkUpdatedSignal,
		TAtomic<bool>* bEncoderNetworkUpdatedSignal,
		TAtomic<bool>* bDecoderNetworkUpdatedSignal,
		const ELogSetting LogSettings)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Learning::PPOTrainer::Train);

		ETrainerResponse Response = ETrainerResponse::Success;

		// Send initial Policy

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Sending initial Policy..."));
		}

		ExternalTrainer->AddNetwork(TEXT("Policy"), PolicyNetwork);
		ExternalTrainer->AddNetwork(TEXT("Critic"), CriticNetwork);
		ExternalTrainer->AddNetwork(TEXT("Encoder"), EncoderNetwork);
		ExternalTrainer->AddNetwork(TEXT("Decoder"), DecoderNetwork);
		ExternalTrainer->AddReplayBuffer(TEXT("ReplayBuffer"), ReplayBuffer);

		const int32 ObservationVectorDimensionNum = ReplayBuffer.GetObservations().Num<1>();
		const int32 ActionVectorDimensionNum = ReplayBuffer.GetActions().Num<1>();
		const int32 MemoryStateVectorDimensionNum = ReplayBuffer.GetMemoryStates().Num<1>();

		// Write PPO Config
		TSharedRef<FJsonObject> ConfigObject = MakeShared<FJsonObject>();
		ConfigObject->SetStringField(TEXT("TaskName"), TEXT("Training"));
		ConfigObject->SetStringField(TEXT("TrainerMethod"), TEXT("PPO"));
		ConfigObject->SetStringField(TEXT("TrainerType"), TEXT("Network"));
		ConfigObject->SetStringField(TEXT("TimeStamp"), *FDateTime::Now().ToFormattedString(TEXT("%Y-%m-%d_%H-%M-%S")));

		ConfigObject->SetObjectField(TEXT("ObservationSchema"), Trainer::ConvertObservationSchemaToJSON(ObservationSchema, ObservationSchemaElement));
		ConfigObject->SetObjectField(TEXT("ActionSchema"), Trainer::ConvertActionSchemaToJSON(ActionSchema, ActionSchemaElement));
		ConfigObject->SetNumberField(TEXT("ObservationVectorDimensionNum"), ObservationVectorDimensionNum);
		ConfigObject->SetNumberField(TEXT("ActionVectorDimensionNum"), ActionVectorDimensionNum);
		ConfigObject->SetNumberField(TEXT("MemoryStateVectorDimensionNum"), MemoryStateVectorDimensionNum);
		ConfigObject->SetNumberField(TEXT("MaxEpisodeNum"), ReplayBuffer.GetMaxEpisodeNum());
		ConfigObject->SetNumberField(TEXT("MaxStepNum"), ReplayBuffer.GetMaxStepNum());

		ConfigObject->SetNumberField(TEXT("PolicyNetworkByteNum"), PolicyNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("CriticNetworkByteNum"), CriticNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("EncoderNetworkByteNum"), EncoderNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("DecoderNetworkByteNum"), DecoderNetwork.GetSnapshotByteNum());

		ConfigObject->SetNumberField(TEXT("IterationNum"), TrainerSettings.IterationNum);
		ConfigObject->SetNumberField(TEXT("LearningRatePolicy"), TrainerSettings.LearningRatePolicy);
		ConfigObject->SetNumberField(TEXT("LearningRateCritic"), TrainerSettings.LearningRateCritic);
		ConfigObject->SetNumberField(TEXT("LearningRateDecay"), TrainerSettings.LearningRateDecay);
		ConfigObject->SetNumberField(TEXT("WeightDecay"), TrainerSettings.WeightDecay);
		ConfigObject->SetNumberField(TEXT("PolicyBatchSize"), TrainerSettings.PolicyBatchSize);
		ConfigObject->SetNumberField(TEXT("CriticBatchSize"), TrainerSettings.CriticBatchSize);
		ConfigObject->SetNumberField(TEXT("PolicyWindow"), TrainerSettings.PolicyWindow);
		ConfigObject->SetNumberField(TEXT("IterationsPerGather"), TrainerSettings.IterationsPerGather);
		ConfigObject->SetNumberField(TEXT("CriticWarmupIterations"), TrainerSettings.CriticWarmupIterations);
		ConfigObject->SetNumberField(TEXT("EpsilonClip"), TrainerSettings.EpsilonClip);
		ConfigObject->SetNumberField(TEXT("ActionSurrogateWeight"), TrainerSettings.ActionSurrogateWeight);
		ConfigObject->SetNumberField(TEXT("ActionRegularizationWeight"), TrainerSettings.ActionRegularizationWeight);
		ConfigObject->SetNumberField(TEXT("ActionEntropyWeight"), TrainerSettings.ActionEntropyWeight);
		ConfigObject->SetNumberField(TEXT("ReturnRegularizationWeight"), TrainerSettings.ReturnRegularizationWeight);
		ConfigObject->SetNumberField(TEXT("GaeLambda"), TrainerSettings.GaeLambda);
		ConfigObject->SetBoolField(TEXT("AdvantageNormalization"), TrainerSettings.bAdvantageNormalization);
		ConfigObject->SetNumberField(TEXT("AdvantageMin"), TrainerSettings.AdvantageMin);
		ConfigObject->SetNumberField(TEXT("AdvantageMax"), TrainerSettings.AdvantageMax);
		ConfigObject->SetBoolField(TEXT("UseGradNormMaxClipping"), TrainerSettings.bUseGradNormMaxClipping);
		ConfigObject->SetNumberField(TEXT("GradNormMax"), TrainerSettings.GradNormMax);
		ConfigObject->SetNumberField(TEXT("TrimEpisodeStartStepNum"), TrainerSettings.TrimEpisodeStartStepNum);
		ConfigObject->SetNumberField(TEXT("TrimEpisodeEndStepNum"), TrainerSettings.TrimEpisodeEndStepNum);
		ConfigObject->SetNumberField(TEXT("Seed"), TrainerSettings.Seed);
		ConfigObject->SetNumberField(TEXT("DiscountFactor"), TrainerSettings.DiscountFactor);
		ConfigObject->SetStringField(TEXT("Device"), Trainer::GetDeviceString(TrainerSettings.Device));
		ConfigObject->SetBoolField(TEXT("UseTensorBoard"), TrainerSettings.bUseTensorboard);
		ConfigObject->SetBoolField(TEXT("SaveSnapshots"), TrainerSettings.bSaveSnapshots);

		ExternalTrainer->SendConfig(ConfigObject, LogSettings);

		Response = ExternalTrainer->SendNetwork(TEXT("Policy"), PolicyNetwork, PolicyNetworkLock);

		if (Response != ETrainerResponse::Success)
		{
			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Error, TEXT("Error sending initial policy to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
			}

			ExternalTrainer->Terminate();
			return Response;
		}

		// Send initial Critic

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Sending initial Critic..."));
		}

		Response = ExternalTrainer->SendNetwork(TEXT("Critic"), CriticNetwork, CriticNetworkLock);

		if (Response != ETrainerResponse::Success)
		{
			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Error, TEXT("Error sending initial critic to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
			}

			ExternalTrainer->Terminate();
			return Response;
		}

		// Send initial Encoder

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Sending initial Encoder..."));
		}

		Response = ExternalTrainer->SendNetwork(TEXT("Encoder"), EncoderNetwork, EncoderNetworkLock);

		if (Response != ETrainerResponse::Success)
		{
			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Error, TEXT("Error sending initial encoder to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
			}

			ExternalTrainer->Terminate();
			return Response;
		}

		// Send initial Decoder

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Sending initial Decoder..."));
		}

		Response = ExternalTrainer->SendNetwork(TEXT("Decoder"), DecoderNetwork, DecoderNetworkLock);

		if (Response != ETrainerResponse::Success)
		{
			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Error, TEXT("Error sending initial decoder to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
			}

			ExternalTrainer->Terminate();
			return Response;
		}

		// Start Training Loop
		while (true)
		{
			if (bRequestTrainingStopSignal && (*bRequestTrainingStopSignal))
			{
				*bRequestTrainingStopSignal = false;

				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Display, TEXT("Stopping Training..."));
				}

				Response = ExternalTrainer->SendStop();

				if (Response != ETrainerResponse::Success)
				{
					if (LogSettings != ELogSetting::Silent)
					{
						UE_LOG(LogLearning, Error, TEXT("Error sending stop signal to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
					}

					ExternalTrainer->Terminate();
					return Response;
				}

				break;
			}
			else
			{
				Experience::GatherExperienceUntilReplayBufferFull(
					ReplayBuffer,
					EpisodeBuffer,
					ResetBuffer,
					ObservationVectorBuffer,
					ActionVectorBuffer,
					PreEvaluationMemoryStateVectorBuffer,
					MemoryStateVectorBuffer,
					RewardBuffer,
					CompletionBuffer,
					EpisodeCompletionBuffer,
					AllCompletionBuffer,
					ResetFunction,
					ObservationFunction,
					PolicyFunction,
					ActionFunction,
					UpdateFunction,
					RewardFunction,
					CompletionFunction,
					Instances);

				Response = ExternalTrainer->SendReplayBuffer(TEXT("ReplayBuffer"), ReplayBuffer);

				if (Response != ETrainerResponse::Success)
				{
					if (LogSettings != ELogSetting::Silent)
					{
						UE_LOG(LogLearning, Error, TEXT("Error sending replay buffer to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
					}

					ExternalTrainer->Terminate();
					return Response;
				}
			}

			// Update Policy

			Response = ExternalTrainer->ReceiveNetwork(TEXT("Policy"), PolicyNetwork, PolicyNetworkLock);

			if (Response == ETrainerResponse::Completed)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Display, TEXT("Trainer completed training."));
				}
				break;
			}
			else if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error receiving policy from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}
				break;
			}

			if (bPolicyNetworkUpdatedSignal)
			{
				*bPolicyNetworkUpdatedSignal = true;
			}

			// Update Critic

			Response = ExternalTrainer->ReceiveNetwork(TEXT("Critic"), CriticNetwork, CriticNetworkLock);

			if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error receiving critic from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}
				break;
			}

			if (bCriticNetworkUpdatedSignal)
			{
				*bCriticNetworkUpdatedSignal = true;
			}

			// Update Encoder

			Response = ExternalTrainer->ReceiveNetwork(TEXT("Encoder"), EncoderNetwork, EncoderNetworkLock);

			if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error receiving encoder from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}
				break;
			}

			if (bEncoderNetworkUpdatedSignal)
			{
				*bEncoderNetworkUpdatedSignal = true;
			}

			// Update Decoder

			Response = ExternalTrainer->ReceiveNetwork(TEXT("Decoder"), DecoderNetwork, DecoderNetworkLock);

			if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error receiving decoder from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}
				break;
			}

			if (bDecoderNetworkUpdatedSignal)
			{
				*bDecoderNetworkUpdatedSignal = true;
			}
		}

		// Allow some time for trainer to shut down gracefully before we kill it...
			
		Response = ExternalTrainer->Wait();

		if (Response != ETrainerResponse::Success)
		{
			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Error, TEXT("Error waiting for trainer to exit: %s. Check log for errors."), Trainer::GetResponseString(Response));
			}
		}

		ExternalTrainer->Terminate();

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Training Task Done!"));
		}

		return ETrainerResponse::Success;
	}
}
