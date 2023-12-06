// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningNeuralNetworkObject.h"

#include "LearningRandom.h"

#if UE_LEARNING_ISPC
#include "Learning.ispc.generated.h"
#endif

namespace UE::Learning
{
	namespace NeuralNetworkPolicyFunction::Private
	{
		static inline float Sigmoid(const float X)
		{
			return 1.0f / (1.0f + FMath::Exp(-X));
		}
	}

	FNeuralNetworkPolicyFunction::FNeuralNetworkPolicyFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InObservationNum,
		const int32 InActionNum,
		const int32 InMemoryStateNum,
		const TSharedPtr<INeuralNetwork>& InNeuralNetwork,
		const uint32 InSeed,
		const FNeuralNetworkInferenceSettings& InInferenceSettings,
		const FNeuralNetworkPolicyFunctionSettings& InSettings)
		: FFunctionObject(InInstanceData)
		, MaxInstanceNum(InMaxInstanceNum)
		, ObservationNum(InObservationNum)
		, ActionNum(InActionNum)
		, MemoryStateNum(InMemoryStateNum)
		, NeuralNetwork(InNeuralNetwork)
		, InferenceSettings(InInferenceSettings)
		, Settings(InSettings)
	{
		UE_LEARNING_CHECK(NeuralNetwork->GetInputNum() == ObservationNum + MemoryStateNum);
		UE_LEARNING_CHECK(NeuralNetwork->GetOutputNum() == 2 * ActionNum + MemoryStateNum);

		SeedHandle = InstanceData->Add<1, uint32>({ InIdentifier, TEXT("Seed") }, { InMaxInstanceNum });
		
		InputObservationHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("InputObservation") }, { InMaxInstanceNum, ObservationNum }, 0.0f);
		InputMemoryStateHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("InputMemoryState") }, { InMaxInstanceNum, MemoryStateNum }, 0.0f);
		InputNetworkHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("InputNetwork") }, { InMaxInstanceNum, ObservationNum + MemoryStateNum }, 0.0f);

		OutputNetworkHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputNetwork") }, { InMaxInstanceNum, 2 * ActionNum + MemoryStateNum }, 0.0f);
		OutputMemoryStateHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputMemoryState") }, { InMaxInstanceNum, MemoryStateNum }, 0.0f);
		OutputActionHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputAction") }, { InMaxInstanceNum, ActionNum }, 0.0f);
		OutputActionMeanHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputActionMean") }, { InMaxInstanceNum, ActionNum }, 0.0f);
		OutputActionStdHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputActionStd") }, { InMaxInstanceNum, ActionNum }, 0.0f);

		ActionNoiseScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("ActionNoiseScale") }, { InMaxInstanceNum }, Settings.ActionNoiseScale);

		NeuralNetworkInference = NeuralNetwork->CreateInferenceObject(InMaxInstanceNum, InInferenceSettings);
		
		Random::IntArray(InstanceData->View(SeedHandle), InSeed);
	}

	void FNeuralNetworkPolicyFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkPolicyFunction::Evaluate);

		TLearningArrayView<1, uint32> Seed = InstanceData->View(SeedHandle);

		const TLearningArrayView<2, const float> InputObservation = InstanceData->ConstView(InputObservationHandle);
		const TLearningArrayView<2, const float> InputMemoryState = InstanceData->ConstView(InputMemoryStateHandle);
		TLearningArrayView<2, float> InputNetwork = InstanceData->View(InputNetworkHandle);

		TLearningArrayView<2, float> OutputNetwork = InstanceData->View(OutputNetworkHandle);
		TLearningArrayView<2, float> OutputMemoryState = InstanceData->View(OutputMemoryStateHandle);
		TLearningArrayView<2, float> OutputAction = InstanceData->View(OutputActionHandle);
		TLearningArrayView<2, float> OutputActionMean = InstanceData->View(OutputActionMeanHandle);
		TLearningArrayView<2, float> OutputActionStd = InstanceData->View(OutputActionStdHandle);

		const TLearningArrayView<1, const float> ActionNoiseScale = InstanceData->ConstView(ActionNoiseScaleHandle);

		const int32 InstanceNum = Instances.Num();

		Array::Check(InputObservation, Instances);
		Array::Check(InputMemoryState, Instances);

		// Copy in Observation and Memory State into network input

		for (const int32 InstanceIdx : Instances)
		{
			Array::Copy(InputNetwork[InstanceIdx].Slice(0, ObservationNum), InputObservation[InstanceIdx]);
			Array::Copy(InputNetwork[InstanceIdx].Slice(ObservationNum, MemoryStateNum), InputMemoryState[InstanceIdx]);
		}

		NeuralNetworkInference->Evaluate(OutputNetwork, InputNetwork, Instances);

		Array::Check(OutputNetwork, Instances);

		// Copy Out Memory State

		for (const int32 InstanceIdx : Instances)
		{
			Array::Copy(
				OutputMemoryState[InstanceIdx],
				OutputNetwork[InstanceIdx].Slice(2 * ActionNum, MemoryStateNum));
		}

		Array::Check(OutputMemoryState, Instances);

		// Apply Action Noise

		UE_LEARNING_ARRAY_VALUE_CHECK(Settings.ActionNoiseMin >= 0.0f && Settings.ActionNoiseMax >= 0.0f);
		const float LogActionNoiseMin = FMath::Loge(Settings.ActionNoiseMin + UE_KINDA_SMALL_NUMBER);
		const float LogActionNoiseMax = FMath::Loge(Settings.ActionNoiseMax + UE_KINDA_SMALL_NUMBER);

#if UE_LEARNING_ISPC
		if (Instances.IsSlice())
		{
			ispc::LearningLayerActionNoise(
				OutputAction.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				OutputActionMean.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				OutputActionStd.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				OutputNetwork.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				Seed.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				ActionNoiseScale.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				Instances.GetSliceNum(),
				ActionNum,
				MemoryStateNum,
				LogActionNoiseMin,
				LogActionNoiseMax);
		}
		else
		{
			for (const int32 InstanceIdx : Instances)
			{
				ispc::LearningLayerActionNoiseSingleBatch(
					OutputAction[InstanceIdx].GetData(),
					OutputActionMean[InstanceIdx].GetData(),
					OutputActionStd[InstanceIdx].GetData(),
					OutputNetwork[InstanceIdx].GetData(),
					Seed[InstanceIdx],
					ActionNoiseScale[InstanceIdx],
					ActionNum,
					MemoryStateNum,
					LogActionNoiseMin,
					LogActionNoiseMax);
			}
		}
#else
		for (const int32 InstanceIdx : Instances)
		{
			for (int32 ActionIdx = 0; ActionIdx < ActionNum; ActionIdx++)
			{
				OutputActionMean[InstanceIdx][ActionIdx] = OutputNetwork[InstanceIdx][ActionIdx];
				OutputActionStd[InstanceIdx][ActionIdx] = ActionNoiseScale[InstanceIdx] *
					FMath::Exp(NeuralNetworkPolicyFunction::Private::Sigmoid(
						OutputNetwork[InstanceIdx][ActionNum + ActionIdx]) * (LogActionNoiseMax - LogActionNoiseMin) + LogActionNoiseMin);

				OutputAction[InstanceIdx][ActionIdx] = Random::Gaussian(
					Seed[InstanceIdx] ^ 0xab744615 ^ Random::Int(ActionIdx ^ 0xf8a88a27),
					OutputActionMean[InstanceIdx][ActionIdx],
					OutputActionStd[InstanceIdx][ActionIdx]);
			}
		}
#endif

		Random::ResampleStateArray(Seed, Instances);

		Array::Check(OutputActionMean, Instances);
		Array::Check(OutputActionStd, Instances);
		Array::Check(OutputAction, Instances);
	}

	void FNeuralNetworkPolicyFunction::UpdateNeuralNetwork(const TSharedPtr<INeuralNetwork>& NewNeuralNetwork)
	{
		if (NeuralNetwork != NewNeuralNetwork)
		{
			NeuralNetwork = NewNeuralNetwork;
			NeuralNetworkInference = NeuralNetwork->CreateInferenceObject(MaxInstanceNum, InferenceSettings);
		}
	}

	FNeuralNetworkCriticFunction::FNeuralNetworkCriticFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InObservationNum,
		const int32 InMemoryStateNum,
		const TSharedPtr<INeuralNetwork>& InNeuralNetwork,
		const FNeuralNetworkInferenceSettings& InInferenceSettings)
		: FFunctionObject(InInstanceData)
		, MaxInstanceNum(InMaxInstanceNum)
		, ObservationNum(InObservationNum)
		, MemoryStateNum(InMemoryStateNum)
		, NeuralNetwork(InNeuralNetwork)
		, InferenceSettings(InInferenceSettings)
	{
		UE_LEARNING_CHECK(NeuralNetwork->GetInputNum() == ObservationNum + MemoryStateNum);
		UE_LEARNING_CHECK(NeuralNetwork->GetOutputNum() == 1);

		InputObservationHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("InputObservation") }, { InMaxInstanceNum, ObservationNum }, 0.0f);
		InputMemoryStateHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("InputMemoryState") }, { InMaxInstanceNum, MemoryStateNum }, 0.0f);
		InputNetworkHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("InputNetwork") }, { InMaxInstanceNum, ObservationNum + MemoryStateNum }, 0.0f);
		OutputHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Output") }, { InMaxInstanceNum }, 0.0f);

		NeuralNetworkInference = NeuralNetwork->CreateInferenceObject(InMaxInstanceNum, InInferenceSettings);
	}

	void FNeuralNetworkCriticFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkCriticFunction::Evaluate);

		const TLearningArrayView<2, const float> InputObservation = InstanceData->ConstView(InputObservationHandle);
		const TLearningArrayView<2, const float> InputMemoryState = InstanceData->ConstView(InputMemoryStateHandle);
		TLearningArrayView<2, float> InputNetwork = InstanceData->View(InputNetworkHandle);
		TLearningArrayView<1, float> Output = InstanceData->View(OutputHandle);

		// Copy in Observation and Memory State into network input

		for (const int32 InstanceIdx : Instances)
		{
			Array::Copy(InputNetwork[InstanceIdx].Slice(0, ObservationNum), InputObservation[InstanceIdx]);
			Array::Copy(InputNetwork[InstanceIdx].Slice(ObservationNum, MemoryStateNum), InputMemoryState[InstanceIdx]);
		}

		NeuralNetworkInference->Evaluate(
			TLearningArrayView<2, float>(Output.GetData(), { Output.Num(), 1 }),
			InputNetwork, 
			Instances);

		Array::Check(Output, Instances);
	}

	void FNeuralNetworkCriticFunction::UpdateNeuralNetwork(const TSharedPtr<INeuralNetwork>& NewNeuralNetwork)
	{
		if (NeuralNetwork != NewNeuralNetwork)
		{
			NeuralNetwork = NewNeuralNetwork;
			NeuralNetworkInference = NeuralNetwork->CreateInferenceObject(MaxInstanceNum, InferenceSettings);
		}
	}
}
