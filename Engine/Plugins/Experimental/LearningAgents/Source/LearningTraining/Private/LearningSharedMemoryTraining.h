// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningTrainer.h"

class ULearningNeuralNetworkData;

namespace UE::Learning
{
	enum class ECompletionMode : uint8;
	struct FReplayBuffer;

	namespace SharedMemoryTraining
	{
		enum class EControls : uint8
		{
			ExperienceEpisodeNum	= 0,
			ExperienceStepNum		= 1,
			ExperienceSignal		= 2,
			ConfigSignal			= 3,
			NetworkSignal			= 4,
			CompleteSignal			= 5,
			StopSignal				= 6,
			PingSignal				= 7,

			ControlNum				= 8,
		};

		LEARNINGTRAINING_API uint8 GetControlNum();

		LEARNINGTRAINING_API ETrainerResponse SendConfigSignal(
			TLearningArrayView<1, volatile int32> Controls,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse RecvNetwork(
			TLearningArrayView<1, volatile int32> Controls,
			ULearningNeuralNetworkData& OutNetwork,
			FSubprocess& Process,
			const TLearningArrayView<1, const uint8> NetworkData,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendStop(
			TLearningArrayView<1, volatile int32> Controls);

		LEARNINGTRAINING_API bool HasPolicyOrCompleted(TLearningArrayView<1, volatile int32> Controls);

		LEARNINGTRAINING_API ETrainerResponse SendNetwork(
			TLearningArrayView<1, volatile int32> Controls,
			TLearningArrayView<1, uint8> NetworkData,
			FSubprocess& Process,
			const ULearningNeuralNetworkData& Network,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendExperience(
			TLearningArrayView<1, int32> EpisodeStarts,
			TLearningArrayView<1, int32> EpisodeLengths,
			TLearningArrayView<1, ECompletionMode> EpisodeCompletionModes,
			TLearningArrayView<2, float> EpisodeFinalObservations,
			TLearningArrayView<2, float> EpisodeFinalMemoryStates,
			TLearningArrayView<2, float> Observations,
			TLearningArrayView<2, float> Actions,
			TLearningArrayView<2, float> MemoryStates,
			TLearningArrayView<1, float> Rewards,
			TLearningArrayView<1, volatile int32> Controls,
			FSubprocess& Process,
			const FReplayBuffer& ReplayBuffer,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendExperience(
			TLearningArrayView<1, int32> EpisodeStarts,
			TLearningArrayView<1, int32> EpisodeLengths,
			TLearningArrayView<2, float> Observations,
			TLearningArrayView<2, float> Actions,
			TLearningArrayView<1, volatile int32> Controls,
			FSubprocess& Process,
			const TLearningArrayView<1, const int32> EpisodeStartsExperience,
			const TLearningArrayView<1, const int32> EpisodeLengthsExperience,
			const TLearningArrayView<2, const float> ObservationExperience,
			const TLearningArrayView<2, const float> ActionExperience,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

	}
}
