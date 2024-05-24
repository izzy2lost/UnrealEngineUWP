// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundGenerator.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"
#include "Tasks/Pipe.h"
#include "Tasks/Task.h"
#include "Templates/UniquePtr.h"

#ifndef METASOUND_OPERATORCACHEPROFILER_ENABLED
#define METASOUND_OPERATORCACHEPROFILER_ENABLED COUNTERSTRACE_ENABLED
#endif
namespace Metasound
{
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
	namespace OperatorPoolPrivate
	{
		class FWindowedHitRate
		{
		public:
			// ctor
			FWindowedHitRate();
			void Update();
			void AddHit();
			void AddMiss();
	
		private:
			struct IntermediateResult
			{
				uint32 NumHits = 0;
				uint32 Total = 0;
				float TTLSeconds;
			};
	
			TArray<IntermediateResult> History;
	
			uint32 CurrHitCount = 0;
			uint32 CurrTotal = 0;
			uint32 RunningHitCount = 0;
			uint32 RunningTotal = 0;
	
			float CurrTTLSeconds = 0.f;
	
			uint64 PreviousTimeCycles = 0;
			bool bIsFirstUpdate = true;
	
			void FirstUpdate();
			void SetWindowLength(const float InNewLengthSeconds);
			void ExpireResult(const IntermediateResult& InResultToExpire);
			void TickResults(const float DeltaTimeSeconds);
	
		}; // class FWindowedHitRate
	} // namespace OperatorPoolPrivate
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED

	struct FOperatorPoolSettings
	{
		uint32 MaxNumOperators = 64;
	};

	// Data required to build an operator without immediately playing it
	struct METASOUNDGENERATOR_API FOperatorBuildData
	{
		FMetasoundGeneratorInitParams InitParams;
		Frontend::FGraphRegistryKey RegistryKey;
		FGuid AssetClassID;
		int32 NumInstances;

		// If true, touches existing assets and only builds remaining number if required
		bool bTouchExisting = false; 

		FOperatorBuildData() = delete;
		FOperatorBuildData(
			  FMetasoundGeneratorInitParams&& InInitParams
			, Frontend::FGraphRegistryKey InRegistryKey
			, FGuid InAssetID
			, int32 InNumInstances = 1
			, bool bInTouchExisting = false
		);

	}; // struct FOperatorPrecacheData

	// Pool of re-useable metasound operators to be used / put back by the metasound generator
	// operators can also be pre-constructed via the UMetasoundCacheSubsystem BP api.
	class METASOUNDGENERATOR_API FOperatorPool : public TSharedFromThis<FOperatorPool>
	{
	public:
		FOperatorPool(const FOperatorPoolSettings& InSettings);
		~FOperatorPool();

		FOperatorAndInputs ClaimOperator(const FGuid& InOperatorID);

		void AddOperator(const FGuid& InOperatorID, TUniquePtr<IOperator>&& InOperator, FInputVertexInterfaceData&& InputData);
		void AddOperator(const FGuid& InOperatorID, FOperatorAndInputs&& OperatorAndInputs);

		void BuildAndAddOperator(TUniquePtr<FOperatorBuildData> InBuildData);

		void TouchOperators(const FGuid& InOperatorID, int32 NumToTouch = 1);
		UE_DEPRECATED(5.5, "Operators can now be directly identified by asset class id.")
		void TouchOperatorsViaAssetClassID(const FGuid& InAssetClassID, int32 NumToTouch = 1);

		bool IsStopping() const;

		void RemoveOperatorsWithID(const FGuid& InOperatorID);
		UE_DEPRECATED(5.5, "Operators can now be directly identified by asset class id.")
		void RemoveOperatorsWithAssetClassID(const FGuid& InAssetClassID);

		int32 GetNumCachedOperatorsWithID(const FGuid& InOperatorID) const;
		UE_DEPRECATED(5.5, "Operators can now be directly identified by asset class id.")
		int32 GetNumCachedOperatorsWithAssetClassID(const FGuid& InAssetClassID) const;

		UE_DEPRECATED(5.5, "Operators can now be directly identified by asset class id, so lookup is not needed anymore.")
		void AddAssetIdToGraphIdLookUp(const FGuid& InAssetClassID, const FGuid& InOperatorID) {}

		void SetMaxNumOperators(uint32 InMaxNumOperators);
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
		void UpdateHitRateTracker();
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED

		UE_DEPRECATED(5.5, "Use StopAsyncTasks")
		void CancelAllBuildEvents();

		void StopAsyncTasks();

		using FTaskId = int32;
		using FTaskFunction = TUniqueFunction<void(FOperatorPool::FTaskId, TWeakPtr<FOperatorPool>)>;

	private:
		FTaskId LastTaskId = 0;

		void AddOperatorInternal(const FGuid& InOperatorID, FOperatorAndInputs&& OperatorAndInputs);
		bool ExecuteTaskAsync(FTaskFunction&& InFunction);
		void Trim();

		FOperatorPoolSettings Settings;
		mutable FCriticalSection CriticalSection;

#if METASOUND_OPERATORCACHEPROFILER_ENABLED
		OperatorPoolPrivate::FWindowedHitRate HitRateTracker;
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED

		// Notifies active build tasks to abort as soon as possible
		// and gates additional build tasks from being added.
		std::atomic<bool> bStopping;

		TMap<FTaskId, UE::Tasks::FTask> ActiveBuildTasks;
		UE::Tasks::FPipe AsyncBuildPipe;

		TMap<FGuid, TArray<FOperatorAndInputs>> Operators;
		TArray<FGuid> Stack;
	};
} // namespace Metasound




