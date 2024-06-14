// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningLog.h"
#include "LearningTrainer.h"
#include "LearningSharedMemory.h"

#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FJsonObject;
class FSocket;
class ULearningNeuralNetworkData;

namespace UE::Learning
{
	struct FReplayBuffer;
	enum class ECompletionMode : uint8;

    /** Interface for communicating with an external trainer process. */
	struct IExternalTrainer
	{
		virtual ~IExternalTrainer() {}

		/** Terminate the trainer immediately. */
		virtual void Terminate() = 0;

		/** Signal for the trainer to stop.	*/
		virtual ETrainerResponse SendStop() = 0;

		/**
		* Wait for the trainer to finish.
		*
		* @param Timeout		Timeout to wait in seconds
		* @returns				Trainer response
		*/
		virtual ETrainerResponse Wait() = 0;


		/**
		* Sends the given json config to the trainer process.
		*
		* @param ConfigObject	The config to send
		* @param LogSettings	The log verbosity level
		* @returns				Trainer response
		*/
		virtual ETrainerResponse SendConfig(
			const TSharedRef<FJsonObject>& ConfigObject,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) = 0;

		/**
		 * Adds the network to this external trainer. Allocates buffers, etc.
		 * Must be called for each network prior to calling Send/Receive.
		 * 
		 * @params Name		The unique name of the network, used as a key.
		 * @params Network	The network to be added.
		 */
		virtual void AddNetwork(const FName& Name, const ULearningNeuralNetworkData& Network) = 0;

		/** Returns true if a network with the given Name has already been added. Otherwise, false. */
		virtual bool ContainsNetwork(const FName& Name) const = 0;

		/**
		* Wait for the trainer to push an updated network.
		*
		* @param OutNetwork		Network to update
		* @param Timeout		Timeout to wait in seconds
		* @param NetworkLock	Lock to use when updating network
		* @param LogSettings	The log verbosity level
		* @returns				Trainer response
		*/
		virtual ETrainerResponse ReceiveNetwork(
			const FName& Name,
			ULearningNeuralNetworkData& OutNetwork,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) = 0;

		/**
		* Wait for the trainer to be ready and push the current policy network.
		*
		* @param Network		Network to push
		* @param Timeout		Timeout to wait in seconds
		* @param NetworkLock	Lock to use when pushing network
		* @param LogSettings	The log verbosity level
		* @returns				Trainer response
		*/
		virtual ETrainerResponse SendNetwork(
			const FName& Name,
			const ULearningNeuralNetworkData& Network,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) = 0;

		/**
		 * Adds a named replay buffer to this external trainer.
		 * Must be called for each buffer prior to calling SendReplayBuffer.
		 *
		 * @params Name						The unique name of the buffer, used as a key
		 * @params ReplayBuffer				The buffer to be added
		 */
		virtual void AddReplayBuffer(const FName& Name,	const FReplayBuffer& ReplayBuffer) = 0;

		/** Returns true if a replay buffer with the given Name has already been added. Otherwise, false. */
		virtual bool ContainsReplayBuffer(const FName& Name) const = 0;

		/**
		* Wait for the trainer to be ready and send new experience.
		*
		* @params Name			The unique name of the buffer, used as a key
		* @param ReplayBuffer	Replay buffer to send
		* @param Timeout		Timeout to wait in seconds
		* @param LogSettings	The log verbosity level
		* @returns				Trainer response
		*/
		virtual ETrainerResponse SendReplayBuffer(
			const FName& Name,
			const FReplayBuffer& ReplayBuffer,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) = 0;
	};

	/** Interface for an external trainer process. */
	struct ITrainerProcess
	{
		virtual ~ITrainerProcess() {}

		/**
		* Check if the server process is still running.
		*/
		virtual bool IsRunning() const = 0;

		/**
		* Wait for the server process to end
		*
		* @returns True if successful, otherwise false if it times out.
		*/
		virtual bool Wait() = 0;

		/**
		* Terminate the server process.
		*/
		virtual void Terminate() = 0;

		/** Get the training subprocess. */
		virtual FSubprocess* GetTrainingSubprocess() = 0;
	};

	/**
	* This object allows you to launch the FSharedMemoryTrainer server as a subprocess,
	* which is convenient when you want to train locally.
	*/
	struct LEARNINGTRAINING_API FSharedMemoryTrainerServerProcess : public ITrainerProcess
	{
		/**
		* Creates a training server as a subprocess using shared memory for communication. This will no-op if this UE
		* process has a non-zero "LearningProcessIdx".
		*
		* @param TaskName					The name of this training task (used to disambiguate filenames, etc.)
		* @param PythonExecutablePath		Path to the python executable used for training. In general should be the
		*									python shipped with Unreal Editor.
		* @param PythonContentPath			Path to the Python Content folder provided by the Learning plugin
		* @param IntermediatePath			Path to the intermediate folder to write temporary files, logs, and
		*									snapshots to
		* @param ProcessNum					Number of processes to use for multi-processed experience gathering (used
		*									to allocate enough shared memory)
		* @param TrainingProcessFlags		Training server subprocess flags
		*/
		FSharedMemoryTrainerServerProcess(
			const FString& TaskName,
			const FString& PythonExecutablePath,
			const FString& PythonContentPath,
			const FString& IntermediatePath,
			const int32 ProcessNum,
			const float InTimeout = Trainer::DefaultTimeout,
			const ESubprocessFlags TrainingProcessFlags = ESubprocessFlags::None);

		~FSharedMemoryTrainerServerProcess();

		/** Check if the server process is still running. */
		virtual bool IsRunning() const override final;

		/**
		* Wait for the server process to end
		*
		* @param Timeout		Timeout to wait in seconds
		* @returns				true if successful, otherwise false if it times out
		*/
		virtual bool Wait() override final;

		/** Terminate the server process. */
		virtual void Terminate() override final;

		/** Get the Controls shared memory array view. */
		TSharedMemoryArrayView<2, volatile int32> GetControlsSharedMemoryArrayView() const;

		/** Get the intermediate path. */
		const FString& GetIntermediatePath() const;

		/** Get the config path. */
		const FString& GetConfigPath() const;

		/** Get the training subprocess. */
		virtual FSubprocess* GetTrainingSubprocess() override final;

	private:

		/** Free and deallocate all shared memory. */
		void Deallocate();

		FString IntermediatePath;
		FString ConfigPath;

		TSharedMemoryArrayView<2, volatile int32> Controls; // Mark as volatile to avoid compiler optimizing away reads without writes etc.

		FSubprocess TrainingProcess;
		float Timeout = Trainer::DefaultTimeout;
	};

	/**
	* Trainer that connects to an external training server to perform training
	*
	* This trainer can be used to allow the python training process the run
	* on a different machine to the experience gathering process.
	*/
	struct LEARNINGTRAINING_API FSharedMemoryTrainer : public IExternalTrainer
	{
		struct FSharedMemoryExperienceContainer
		{
			TSharedMemoryArrayView<2, int32> EpisodeStarts;
			TSharedMemoryArrayView<2, int32> EpisodeLengths;
			TSharedMemoryArrayView<2, ECompletionMode> EpisodeCompletionModes;
			TSharedMemoryArrayView<3, float> EpisodeFinalObservations;
			TSharedMemoryArrayView<3, float> EpisodeFinalMemoryStates;
			TSharedMemoryArrayView<3, float> Observations;
			TSharedMemoryArrayView<3, float> Actions;
			TSharedMemoryArrayView<3, float> MemoryStates;
			TSharedMemoryArrayView<2, float> Rewards;

			/** Free and deallocate all shared memory. */
			void Deallocate();
		};

		/**
		* Creates a new SharedMemory trainer
		*
		* @param InTaskName				Unique name for this training task - used to avoid config filename conflicts
		* @param ProcessNum				Number of processes to use for multi-processed experience gathering (used
		*								to allocate enough shared memory)
		* @param ExternalTrainerProcess	Shared memory used for communicating status to the trainer server process
		* @param InTimeout				Timeout to wait in seconds for connection and initial data transfer
		*/
		FSharedMemoryTrainer(
			const FString& InTaskName,
			const int32 InProcessNum,
			const TSharedPtr<UE::Learning::ITrainerProcess>& ExternalTrainerProcess,
			const float InTimeout = Trainer::DefaultTimeout);

		~FSharedMemoryTrainer();

		virtual void Terminate() override final;

		virtual ETrainerResponse SendStop() override final;

		virtual ETrainerResponse Wait() override final;

		virtual ETrainerResponse SendConfig(
			const TSharedRef<FJsonObject>& ConfigObject,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual void AddNetwork(const FName& Name, const ULearningNeuralNetworkData& Network) override final;

		virtual bool ContainsNetwork(const FName& Name) const override final;

		virtual ETrainerResponse ReceiveNetwork(
			const FName& Name,
			ULearningNeuralNetworkData& OutNetwork,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual ETrainerResponse SendNetwork(
			const FName& Name,
			const ULearningNeuralNetworkData& Network,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual void AddReplayBuffer(const FName& Name,	const FReplayBuffer& ReplayBuffer) override final;

		virtual bool ContainsReplayBuffer(const FName& Name) const override final;

		virtual ETrainerResponse SendReplayBuffer(
			const FName& Name,
			const FReplayBuffer& ReplayBuffer,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

	private:

		/** Free and deallocate all shared memory. */
		void Deallocate();

		FString TaskName;

		FString IntermediatePath;

		FString ConfigPath;

		int32 ProcessNum;

		FSubprocess* TrainingProcess = nullptr;

		float Timeout = Trainer::DefaultTimeout;

		int32 ProcessIdx = INDEX_NONE;
		TSharedMemoryArrayView<2, volatile int32> Controls;
		TMap<FName, TSharedMemoryArrayView<1, uint8>> NeuralNetworkSharedMemoryArrayViews;
		TMap<FName, FSharedMemoryExperienceContainer> SharedMemoryExperienceContainers;
	};

	/**
	* This object allows you to launch the FSocketTrainer server as a subprocess,
	* which is convenient when you want to train using it locally.
	*/
	struct LEARNINGTRAINING_API FSocketTrainerServerProcess : public ITrainerProcess
	{
		/**
		* Creates a training server as a subprocess
		*
		* @param PythonExecutablePath		Path to the python executable used for training. In general should be the python shipped with Unreal Editor.
		* @param PythonContentPath			Path to the Python Content folder provided by the Learning plugin
		* @param IntermediatePath			Path to the intermediate folder to write temporary files, logs, and snapshots to
		* @param IpAddress					Ip address to bind the listening socket to. For a local server you will want to use 127.0.0.1
		* @param Port						Port to use for the listening socket.
		* @param TrainingProcessFlags		Training server subprocess flags
		* @param LogSettings				Logging settings to use
		*/
		FSocketTrainerServerProcess(
			const FString& PythonExecutablePath,
			const FString& PythonContentPath,
			const FString& IntermediatePath,
			const TCHAR* IpAddress = Trainer::DefaultIp,
			const uint32 Port = Trainer::DefaultPort,
			const float InTimeout = Trainer::DefaultTimeout,
			const ESubprocessFlags TrainingProcessFlags = ESubprocessFlags::None,
			const ELogSetting LogSettings = ELogSetting::Normal);

		~FSocketTrainerServerProcess();

		/**
		* Check if the server process is still running
		*/
		virtual bool IsRunning() const override final;

		/**
		* Wait for the server process to end
		*
		* @param Timeout		Timeout to wait in seconds
		* @returns				true if successful, otherwise false if it times out
		*/
		virtual bool Wait() override final;

		/**
		* Terminate the server process
		*/
		virtual void Terminate() override final;

		/** Get the training subprocess. */
		virtual FSubprocess* GetTrainingSubprocess() override final;

	private:

		FSubprocess TrainingProcess;
		float Timeout = Trainer::DefaultTimeout;
	};

	/**
	* Trainer that connects to an external training server to perform training
	*
	* This trainer can be used to allow the python training process the run
	* on a different machine to the experience gathering process.
	*/
	struct LEARNINGTRAINING_API FSocketTrainer : public IExternalTrainer
	{
		/**
		* Creates a new Socket trainer
		*
		* @param OutResponse				Response to the initial connection
		* @param ExternalTrainerProcess		The external trainer process
		* @param IpAddress					Server Ip address
		* @param Port						Server Port
		* @param Timeout					Timeout to wait in seconds for connection and initial data transfer
		*/
		FSocketTrainer(
			ETrainerResponse& OutResponse,
			const TSharedPtr<UE::Learning::ITrainerProcess>& ExternalTrainerProcess,
			const TCHAR* IpAddress = Trainer::DefaultIp,
			const uint32 Port = Trainer::DefaultPort,
			const float InTimeout = Trainer::DefaultTimeout);

		~FSocketTrainer();

		virtual void Terminate() override final;

		virtual ETrainerResponse SendStop() override final;

		virtual ETrainerResponse Wait() override final;

		virtual ETrainerResponse SendConfig(
			const TSharedRef<FJsonObject>& ConfigObject,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual void AddNetwork(const FName& Name, const ULearningNeuralNetworkData& Network) override final;

		virtual bool ContainsNetwork(const FName& Name) const override final;

		virtual ETrainerResponse ReceiveNetwork(
			const FName& Name,
			ULearningNeuralNetworkData& OutNetwork,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual ETrainerResponse SendNetwork(
			const FName& Name,
			const ULearningNeuralNetworkData& Network,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual void AddReplayBuffer(const FName& Name, const FReplayBuffer& ReplayBuffer) override final;

		virtual bool ContainsReplayBuffer(const FName& Name) const override final;

		virtual ETrainerResponse SendReplayBuffer(
			const FName& Name,
			const FReplayBuffer& ReplayBuffer,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

	private:

		TMap<FName, TLearningArray<1, uint8>> NetworkBuffers;
		TSet<FName> ExperienceBufferNames;

		float Timeout = Trainer::DefaultTimeout;

		FSubprocess* TrainingProcess = nullptr;

		FSocket* Socket = nullptr;
	};
}
