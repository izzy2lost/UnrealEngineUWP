// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningNeuralNetwork.h"

#include "LearningAgentsNeuralNetworkData.generated.h"

enum class ELearningAgentsActivationFunction : uint8;

class UNNEModelData;

namespace UE::NNE
{
	class IModelCPU;
	class IModelInstanceCPU;
}

class ULearningAgentsNeuralNetworkData;

namespace UE::Learning::Agents
{
	struct FNeuralNetwork;
	struct FNeuralNetworkInference;

	struct FNeuralNetwork : public INeuralNetwork
	{
		FNeuralNetwork(ULearningAgentsNeuralNetworkData& InParent);

		//~ Begin INeuralNetwork Interface
		virtual bool DeserializeFromBytes(int32& InOutOffset, const TLearningArrayView<1, const uint8> RawBytes) override final;
		virtual void SerializeToBytes(int32& InOutOffset, TLearningArrayView<1, uint8> OutRawBytes) const override final;
		virtual int32 GetSerializationByteNum() const override final;
		virtual TSharedRef<INeuralNetworkInference> CreateInferenceObject(
			const int32 MaxBatchSize,
			const FNeuralNetworkInferenceSettings& Settings = FNeuralNetworkInferenceSettings()) override final;
		virtual int32 GetInputNum() const override final;
		virtual int32 GetOutputNum() const override final;
		virtual const TCHAR* GetPythonClassName() const override final { return TEXT("NeuralNetworkNNE"); };
		//~ End INeuralNetwork Interface

		void ReloadFromFileData();

		ULearningAgentsNeuralNetworkData& Parent;
		TSharedPtr<NNE::IModelCPU> Model;
		TArray<TWeakPtr<FNeuralNetworkInference>, TInlineAllocator<64>> InferenceObjects;
	};

	struct FNeuralNetworkInference : public INeuralNetworkInference
	{
		FNeuralNetworkInference(
			NNE::IModelCPU& InModel,
			const FNeuralNetworkInferenceSettings& InSettings,
			const int32 MaxBatchSize,
			const int32 InputSize,
			const int32 OutputSize);

		//~ Begin INeuralNetworkInference Interface
		virtual void Evaluate(
			TLearningArrayView<2, float> Output,
			const TLearningArrayView<2, const float> Input,
			const FIndexSet Instances) override final;
		//~ End INeuralNetworkInference Interface

		void CreateInstances(NNE::IModelCPU& InModel);

		FNeuralNetworkInferenceSettings Settings;
		TLearningArray<2, float> InputBuffer;
		TLearningArray<2, float> OutputBuffer;
		TArray<TSharedPtr<NNE::IModelInstanceCPU>, TInlineAllocator<64>> InferenceInstances;
	};
}

UCLASS()
class LEARNINGAGENTS_API ULearningAgentsNeuralNetworkData : public UObject
{
	GENERATED_BODY()

	friend struct UE::Learning::Agents::FNeuralNetwork;

public:

	virtual void PostLoad() override final;

	TSharedPtr<UE::Learning::INeuralNetwork> GetNetworkInterface();

	void CopyFrom(const ULearningAgentsNeuralNetworkData* Other);

	void CreateMLP(
		const uint32 InputSize,
		const uint32 OutputSize,
		const uint32 HiddenUnitNum,
		const uint32 LayerNum,
		const ELearningAgentsActivationFunction Activation);

	void CreateMemoryBackbone(
		const uint32 InputSize,
		const uint32 OutputSize,
		const uint32 MemorySize,
		const uint32 HiddenUnitNum,
		const uint32 PrefixLayerNum,
		const uint32 PostfixLayerNum);

private:

	UPROPERTY()
	int32 InputNum = 0;

	UPROPERTY()
	int32 OutputNum = 0;

	UPROPERTY()
	TArray<uint8> FileData;

	UPROPERTY()
	TObjectPtr<UNNEModelData> ModelData;

	TSharedPtr<UE::Learning::Agents::FNeuralNetwork> Network;
};
