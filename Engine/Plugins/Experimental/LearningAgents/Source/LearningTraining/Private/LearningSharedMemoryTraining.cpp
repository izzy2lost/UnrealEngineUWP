// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningSharedMemoryTraining.h"

#include "LearningProgress.h"
#include "LearningNeuralNetwork.h"
#include "LearningExperience.h"
#include "LearningCompletion.h"

#include "HAL/PlatformProcess.h"

namespace UE::Learning::SharedMemoryTraining
{
	uint8 GetControlNum()
	{
		return (uint8)EControls::ControlNum;
	}

	ETrainerResponse SendStop(TLearningArrayView<1, volatile int32> Controls)
	{
		Controls[(uint8)EControls::StopSignal] = true;
		return ETrainerResponse::Success;
	}

	bool HasPolicyOrCompleted(TLearningArrayView<1, volatile int32> Controls)
	{
		return Controls[(uint8)EControls::NetworkSignal] || Controls[(uint8)EControls::CompleteSignal];
	}

	ETrainerResponse SendConfigSignal(
		TLearningArrayView<1, volatile int32> Controls,
		const ELogSetting LogSettings)
	{
		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Sending config signal..."));
		}

		Controls[(uint8)EControls::ConfigSignal] = true;

		return ETrainerResponse::Success;
	}

	ETrainerResponse RecvNetwork(
		TLearningArrayView<1, volatile int32> Controls,
		ULearningNeuralNetworkData& OutNetwork,
		FSubprocess& Process,
		const TLearningArrayView<1, const uint8> NetworkData,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the network is done being written by the training process
		while (!Controls[(uint8)EControls::NetworkSignal])
		{
			// Check if Completed Signal has been raised
			if (Controls[(uint8)EControls::CompleteSignal])
			{
				// If so set low to confirm we have read it
				Controls[(uint8)EControls::CompleteSignal] = false;
				return ETrainerResponse::Completed;
			}

			// If we're monitoring a process, then has it has exited?
			if (!Process.Update())
			{
				return ETrainerResponse::Unexpected;
			}

			// Check if we've timed out
			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}

			// Check if ping has been sent
			if (Controls[(uint8)EControls::PingSignal])
			{
				Controls[(uint8)EControls::PingSignal] = false;
				WaitTime = 0.0f;
			}

			// Sleep for some time
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pulling network..."));
		}

		// Read the network
		bool bSuccess = false;
		{
			FScopeNullableWriteLock ScopeLock(NetworkLock);

			if (NetworkData.Num() != OutNetwork.GetSnapshotByteNum())
			{
				UE_LOG(LogLearning, Error, TEXT("Error receiving network. Incorrect buffer size. Buffer is %i bytes, expected %i."), NetworkData.Num(), OutNetwork.GetSnapshotByteNum());
				bSuccess = false;
			}
			else
			{
				if (!OutNetwork.LoadFromSnapshot(MakeArrayView(NetworkData.GetData(), NetworkData.Num())))
				{
					UE_LOG(LogLearning, Error, TEXT("Error receiving network. Invalid Format."));
					bSuccess = false;
				}
				else
				{
					bSuccess = true;
				}
			}
		}

		// Confirm we have read the network
		Controls[(uint8)EControls::NetworkSignal] = false;

		return bSuccess ? ETrainerResponse::Success : ETrainerResponse::Unexpected;
	}

	ETrainerResponse SendNetwork(
		TLearningArrayView<1, volatile int32> Controls,
		TLearningArrayView<1, uint8> NetworkData,
		FSubprocess& Process,
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the policy is requested by the training process
		while (!Controls[(uint8)EControls::NetworkSignal])
		{
			// If we're monitoring a process, then has it has exited?
			if (!Process.Update())
			{
				return ETrainerResponse::Unexpected;
			}

			// Check if we've timed out
			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}

			// Check if ping has been sent
			if (Controls[(uint8)EControls::PingSignal])
			{
				Controls[(uint8)EControls::PingSignal] = false;
				WaitTime = 0.0f;
			}

			// Sleep for some time
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing network..."));
		}

		// Write the network
		bool bSuccess = false;
		{
			FScopeNullableReadLock ScopeLock(NetworkLock);
			if (NetworkData.Num() != Network.GetSnapshotByteNum())
			{
				UE_LOG(LogLearning, Error, TEXT("Error sending network. Incorrect buffer size. Buffer is %i bytes, expected %i."), NetworkData.Num(), Network.GetSnapshotByteNum());
				bSuccess = false;
			}
			else
			{
				Network.SaveToSnapshot(MakeArrayView(NetworkData.GetData(), NetworkData.Num()));
				bSuccess = true;
			}
		}

		// Confirm we have written the network
		Controls[(uint8)EControls::NetworkSignal] = false;

		return bSuccess ? ETrainerResponse::Success : ETrainerResponse::Unexpected;
	}

	ETrainerResponse SendExperience(
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
		const float Timeout,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the training process is done reading any experience
		while (Controls[(uint8)EControls::ExperienceSignal])
		{
			// If we're monitoring a process, then has it has exited?
			if (!Process.Update())
			{
				return ETrainerResponse::Unexpected;
			}

			// Check if we've timed out
			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}

			// Check if ping has been sent
			if (Controls[(uint8)EControls::PingSignal])
			{
				Controls[(uint8)EControls::PingSignal] = false;
				WaitTime = 0.0f;
			}

			// Sleep for some time
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Experience..."));
		}

		const int32 EpisodeNum = ReplayBuffer.GetEpisodeNum();
		const int32 StepNum = ReplayBuffer.GetStepNum();

		// Write experience to the shared memory
		Array::Copy(EpisodeStarts.Slice(0, EpisodeNum), ReplayBuffer.GetEpisodeStarts());
		Array::Copy(EpisodeLengths.Slice(0, EpisodeNum), ReplayBuffer.GetEpisodeLengths());
		Array::Copy(EpisodeCompletionModes.Slice(0, EpisodeNum), ReplayBuffer.GetEpisodeCompletionModes());
		Array::Copy(EpisodeFinalObservations.Slice(0, EpisodeNum), ReplayBuffer.GetEpisodeFinalObservations());
		Array::Copy(EpisodeFinalMemoryStates.Slice(0, EpisodeNum), ReplayBuffer.GetEpisodeFinalMemoryStates());
		Array::Copy(Observations.Slice(0, StepNum), ReplayBuffer.GetObservations());
		Array::Copy(Actions.Slice(0, StepNum), ReplayBuffer.GetActions());
		Array::Copy(MemoryStates.Slice(0, StepNum), ReplayBuffer.GetMemoryStates());
		Array::Copy(Rewards.Slice(0, StepNum), ReplayBuffer.GetRewards());

		// Indicate that experience is written
		Controls[(uint8)EControls::ExperienceEpisodeNum] = EpisodeNum;
		Controls[(uint8)EControls::ExperienceStepNum] = StepNum;
		Controls[(uint8)EControls::ExperienceSignal] = true;

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendExperience(
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
		const float Timeout,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the training process is done reading any experience
		while (Controls[(uint8)EControls::ExperienceSignal])
		{
			// If we're monitoring a process, then has it has exited?
			if (!Process.Update())
			{
				return ETrainerResponse::Unexpected;
			}

			// Check if we've timed out
			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}

			// Check if ping has been sent
			if (Controls[(uint8)EControls::PingSignal])
			{
				Controls[(uint8)EControls::PingSignal] = false;
				WaitTime = 0.0f;
			}

			// Sleep for some time
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Experience..."));
		}

		const int32 EpisodeNum = EpisodeStartsExperience.Num<0>();
		const int32 StepNum = ObservationExperience.Num<0>();

		// Write experience to the shared memory
		Array::Copy(EpisodeStarts.Slice(0, EpisodeNum), EpisodeStartsExperience);
		Array::Copy(EpisodeLengths.Slice(0, EpisodeNum), EpisodeLengthsExperience);
		Array::Copy(Observations.Slice(0, StepNum), ObservationExperience);
		Array::Copy(Actions.Slice(0, StepNum), ActionExperience);

		// Confirm that experience is written
		Controls[(uint8)EControls::ExperienceEpisodeNum] = EpisodeNum;
		Controls[(uint8)EControls::ExperienceStepNum] = StepNum;
		Controls[(uint8)EControls::ExperienceSignal] = true;

		return ETrainerResponse::Success;
	}

}