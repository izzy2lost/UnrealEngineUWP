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
			return 1.0f / (1.0f + FMath::InvExpApprox(X));
		}
	}

	FNeuralNetworkPolicyFunction::FNeuralNetworkPolicyFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const TSharedPtr<INeuralNetwork>& InNeuralNetwork,
		const uint32 InSeed,
		const FNeuralNetworkInferenceSettings& InInferenceSettings,
		const FNeuralNetworkPolicyFunctionSettings& InSettings)
		: FFunctionObject(InInstanceData)
		, MaxInstanceNum(InMaxInstanceNum)
		, NeuralNetwork(InNeuralNetwork)
		, InferenceSettings(InInferenceSettings)
		, Settings(InSettings)
	{
		SeedHandle = InstanceData->Add<1, uint32>({ InIdentifier, TEXT("Seed") }, { InMaxInstanceNum });
		InputHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Input") }, { InMaxInstanceNum, NeuralNetwork->GetInputNum() }, 0.0f);
		OutputHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Output") }, { InMaxInstanceNum, NeuralNetwork->GetOutputNum() / 2 }, 0.0f);
		OutputNetworkHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputNetwork") }, { InMaxInstanceNum, NeuralNetwork->GetOutputNum() }, 0.0f);
		OutputMeanHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputMean") }, { InMaxInstanceNum, NeuralNetwork->GetOutputNum() / 2 }, 0.0f);
		OutputStdHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputStd") }, { InMaxInstanceNum, NeuralNetwork->GetOutputNum() / 2 }, 0.0f);
		ActionNoiseScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("ActionNoiseScale") }, { InMaxInstanceNum }, Settings.ActionNoiseScale);

		NeuralNetworkInference = NeuralNetwork->CreateInferenceObject(InMaxInstanceNum, InInferenceSettings);
		
		Random::IntArray(InstanceData->View(SeedHandle), InSeed);
	}

	void FNeuralNetworkPolicyFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkPolicyFunction::Evaluate);

		const TLearningArrayView<2, const float> Input = InstanceData->ConstView(InputHandle);
		const TLearningArrayView<1, const float> ActionNoiseScale = InstanceData->ConstView(ActionNoiseScaleHandle);
		TLearningArrayView<2, float> Output = InstanceData->View(OutputHandle);
		TLearningArrayView<2, float> OutputNetwork = InstanceData->View(OutputNetworkHandle);
		TLearningArrayView<2, float> OutputMean = InstanceData->View(OutputMeanHandle);
		TLearningArrayView<2, float> OutputStd = InstanceData->View(OutputStdHandle);
		TLearningArrayView<1, uint32> Seed = InstanceData->View(SeedHandle);

		const int32 InstanceNum = Instances.Num();
		const int32 InputNum = Input.Num<1>();
		const int32 OutputNum = Output.Num<1>();

		UE_LEARNING_CHECK(NeuralNetwork->GetInputNum() == InputNum);
		UE_LEARNING_CHECK(NeuralNetwork->GetOutputNum() == 2 * OutputNum);

		Array::Check(Input, Instances);

		NeuralNetworkInference->Evaluate(OutputNetwork, Input, Instances);

		Array::Check(OutputNetwork, Instances);

		// Apply Action Noise

		UE_LEARNING_ARRAY_VALUE_CHECK(Settings.ActionNoiseMin >= 0.0f && Settings.ActionNoiseMax >= 0.0f);
		const float LogActionNoiseMin = FMath::Loge(Settings.ActionNoiseMin + UE_KINDA_SMALL_NUMBER);
		const float LogActionNoiseMax = FMath::Loge(Settings.ActionNoiseMax + UE_KINDA_SMALL_NUMBER);

#if UE_LEARNING_ISPC
		if (Instances.IsSlice())
		{
			ispc::LearningLayerActionNoise(
				Output.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				OutputMean.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				OutputStd.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				OutputNetwork.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				Seed.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				ActionNoiseScale.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
				Instances.GetSliceNum(),
				OutputNum,
				LogActionNoiseMin,
				LogActionNoiseMax);
		}
		else
		{
			for (const int32 InstanceIdx : Instances)
			{
				ispc::LearningLayerActionNoiseSingleBatch(
					Output[InstanceIdx].GetData(),
					OutputMean[InstanceIdx].GetData(),
					OutputStd[InstanceIdx].GetData(),
					OutputNetwork[InstanceIdx].GetData(),
					Seed[InstanceIdx],
					ActionNoiseScale[InstanceIdx],
					OutputNum,
					LogActionNoiseMin,
					LogActionNoiseMax);
			}
		}
#else
		for (const int32 InstanceIdx : Instances)
		{
			for (int32 OutputIdx = 0; OutputIdx < OutputNum; OutputIdx++)
			{
				OutputMean[InstanceIdx][OutputIdx] = OutputNetwork[InstanceIdx][OutputIdx];
				OutputStd[InstanceIdx][OutputIdx] = ActionNoiseScale[InstanceIdx] *
					FMath::Exp(NeuralNetworkPolicyFunction::Private::Sigmoid(
						OutputNetwork[InstanceIdx][OutputNum + OutputIdx]) * (LogActionNoiseMax - LogActionNoiseMin) + LogActionNoiseMin);

				Output[InstanceIdx][OutputIdx] = Random::Gaussian(
					Seed[InstanceIdx] ^ 0xab744615 ^ Random::Int(OutputIdx ^ 0xf8a88a27),
					OutputMean[InstanceIdx][OutputIdx],
					OutputStd[InstanceIdx][OutputIdx]);
			}
		}
#endif

		Random::ResampleStateArray(Seed, Instances);

		Array::Check(OutputMean, Instances);
		Array::Check(OutputStd, Instances);
		Array::Check(Output, Instances);
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
		const TSharedPtr<INeuralNetwork>& InNeuralNetwork,
		const FNeuralNetworkInferenceSettings& InInferenceSettings)
		: FFunctionObject(InInstanceData)
		, MaxInstanceNum(InMaxInstanceNum)
		, NeuralNetwork(InNeuralNetwork)
		, InferenceSettings(InInferenceSettings)
	{
		InputHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Input") }, { InMaxInstanceNum, NeuralNetwork->GetInputNum() }, 0.0f);
		OutputHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Output") }, { InMaxInstanceNum }, 0.0f);

		NeuralNetworkInference = NeuralNetwork->CreateInferenceObject(InMaxInstanceNum, InInferenceSettings);
	}

	void FNeuralNetworkCriticFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkCriticFunction::Evaluate);

		const TLearningArrayView<2, const float> Input = InstanceData->ConstView(InputHandle);
		TLearningArrayView<1, float> Output = InstanceData->View(OutputHandle);

		UE_LEARNING_CHECK(NeuralNetwork->GetInputNum() == Input.Num<1>());
		UE_LEARNING_CHECK(NeuralNetwork->GetOutputNum() == 1);

		NeuralNetworkInference->Evaluate(
			TLearningArrayView<2, float>(Output.GetData(), { Output.Num(), 1 }),
			Input, Instances);

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
