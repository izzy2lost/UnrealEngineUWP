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

UCLASS()
class LEARNINGAGENTS_API ULearningAgentsNeuralNetworkData : public UObject 
{
	GENERATED_BODY()

public:

	virtual TSharedPtr<UE::Learning::INeuralNetwork> GetNetworkInterface() { return nullptr; }

	virtual void CopyFrom(const ULearningAgentsNeuralNetworkData* Other) {}

	virtual void CreateMLP(
		const uint32 InputSize,
		const uint32 OutputSize,
		const uint32 HiddenUnitNum,
		const uint32 LayerNum,
		const ELearningAgentsActivationFunction Activation) {}
};

UCLASS()
class LEARNINGAGENTS_API ULearningAgentsMLPNeuralNetworkData : public ULearningAgentsNeuralNetworkData 
{
	GENERATED_BODY()

public:

	virtual void Serialize(FArchive& Ar) override final;

	virtual TSharedPtr<UE::Learning::INeuralNetwork> GetNetworkInterface() override final;

	virtual void CopyFrom(const ULearningAgentsNeuralNetworkData* Other) override final;

	virtual void CreateMLP(
		const uint32 InputSize,
		const uint32 OutputSize,
		const uint32 HiddenUnitNum,
		const uint32 LayerNum,
		const ELearningAgentsActivationFunction Activation) override final;

	TSharedPtr<UE::Learning::FNeuralNetworkMLP> Network;
};

class ULearningAgentsNNENeuralNetworkData;

namespace UE::Learning::Agents
{
	struct FNeuralNetworkNNE;
	struct FNeuralNetworkNNEInference;

	struct FNeuralNetworkNNE : public INeuralNetwork
	{
		FNeuralNetworkNNE(ULearningAgentsNNENeuralNetworkData& InParent);

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

		ULearningAgentsNNENeuralNetworkData& Parent;
		TSharedPtr<NNE::IModelCPU> Model;
		TArray<TWeakPtr<FNeuralNetworkNNEInference>, TInlineAllocator<64>> InferenceObjects;
	};

	struct FNeuralNetworkNNEInference : public INeuralNetworkInference
	{
		FNeuralNetworkNNEInference(
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
class LEARNINGAGENTS_API ULearningAgentsNNENeuralNetworkData : public ULearningAgentsNeuralNetworkData
{
	GENERATED_BODY()

	friend struct UE::Learning::Agents::FNeuralNetworkNNE;

public:

	virtual void PostLoad() override final;

	virtual TSharedPtr<UE::Learning::INeuralNetwork> GetNetworkInterface() override final;

	virtual void CopyFrom(const ULearningAgentsNeuralNetworkData* Other) override final;

	virtual void CreateMLP(
		const uint32 InputSize,
		const uint32 OutputSize,
		const uint32 HiddenUnitNum,
		const uint32 LayerNum,
		const ELearningAgentsActivationFunction Activation) override final;

private:

	UPROPERTY()
	int32 InputNum = 0;

	UPROPERTY()
	int32 OutputNum = 0;

	UPROPERTY()
	TArray<uint8> FileData;

	UPROPERTY()
	TObjectPtr<UNNEModelData> ModelData;

	TSharedPtr<UE::Learning::Agents::FNeuralNetworkNNE> Network;
};

using ULearningAgentsDefaultNeuralNetworkData = ULearningAgentsNNENeuralNetworkData;