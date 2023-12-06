// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningPolicy.h"

namespace UE::Learning
{
	FNeuralNetworkPolicy::FNeuralNetworkPolicy(
		const int32 InMaxInstanceNum,
		const int32 InObservationEncodedNum,
		const int32 InActionEncodedNum,
		const int32 InMemoryStateNum,
		const TSharedPtr<FNeuralNetwork>& InNeuralNetwork,
		const FNeuralNetworkInferenceSettings& InInferenceSettings)
		: MaxInstanceNum(InMaxInstanceNum)
		, ObservationEncodedNum(InObservationEncodedNum)
		, ActionEncodedNum(InActionEncodedNum)
		, MemoryStateNum(InMemoryStateNum)
		, NeuralNetwork(InNeuralNetwork)
		, InferenceSettings(InInferenceSettings)
	{
		UE_LEARNING_CHECK(NeuralNetwork->GetInputSize() == ObservationEncodedNum + MemoryStateNum);
		UE_LEARNING_CHECK(NeuralNetwork->GetOutputSize() == ActionEncodedNum + MemoryStateNum);

		Input.SetNumUninitialized({ MaxInstanceNum, ObservationEncodedNum + MemoryStateNum });
		Output.SetNumUninitialized({ MaxInstanceNum, ActionEncodedNum + MemoryStateNum });

		Array::Zero(Input);
		Array::Zero(Output);

		NeuralNetworkInference = NeuralNetwork->CreateInferenceObject(MaxInstanceNum, InferenceSettings);
	}

	void FNeuralNetworkPolicy::Evaluate(
		TLearningArrayView<2, float> OutputActionVectorsEncoded,
		TLearningArrayView<2, float> OutputMemoryState,
		const TLearningArrayView<2, const float> InputObservationVectorsEncoded,
		const TLearningArrayView<2, const float> InputMemoryState, 
		const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkPolicy::Evaluate);

		const int32 InstanceNum = Instances.Num();

		Array::Check(InputObservationVectorsEncoded, Instances);
		Array::Check(InputMemoryState, Instances);

		// Copy in Observation and Memory State into network input

		for (const int32 InstanceIdx : Instances)
		{
			Array::Copy(Input[InstanceIdx].Slice(0, ObservationEncodedNum), InputObservationVectorsEncoded[InstanceIdx]);
			Array::Copy(Input[InstanceIdx].Slice(ObservationEncodedNum, MemoryStateNum), InputMemoryState[InstanceIdx]);
		}

		NeuralNetworkInference->Evaluate(Output, Input, Instances);

		// Copy Out Memory State

		for (const int32 InstanceIdx : Instances)
		{
			Array::Copy(OutputActionVectorsEncoded[InstanceIdx], Output[InstanceIdx].Slice(0, ActionEncodedNum));
			Array::Copy(OutputMemoryState[InstanceIdx], Output[InstanceIdx].Slice(ActionEncodedNum, MemoryStateNum));
		}

		Array::Check(OutputActionVectorsEncoded, Instances);
		Array::Check(OutputMemoryState, Instances);
	}

	void FNeuralNetworkPolicy::UpdateNeuralNetwork(const TSharedPtr<FNeuralNetwork>& NewNeuralNetwork)
	{
		if (NeuralNetwork != NewNeuralNetwork)
		{
			UE_LEARNING_CHECK(NewNeuralNetwork->GetInputSize() == ObservationEncodedNum + MemoryStateNum);
			UE_LEARNING_CHECK(NewNeuralNetwork->GetOutputSize() == ActionEncodedNum + MemoryStateNum);
			NeuralNetwork = NewNeuralNetwork;
			NeuralNetworkInference = NeuralNetwork->CreateInferenceObject(MaxInstanceNum, InferenceSettings);
		}
	}
}
