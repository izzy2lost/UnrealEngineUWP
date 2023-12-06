// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArrayMap.h"
#include "LearningFunctionObject.h"
#include "LearningNeuralNetwork.h" // Included for FNeuralNetworkInferenceSettings

#include "Templates/SharedPointer.h"

namespace UE::Learning
{
	/**
	* Settings object for a neural network based policy
	*/
	struct LEARNING_API FNeuralNetworkPolicyFunctionSettings
	{
		// Minimum amount of action noise to allow
		float ActionNoiseMin = 0.0f;

		// Maximum amount of action noise to allow
		float ActionNoiseMax = 0.0f;

		// Overall scale of the action noise
		float ActionNoiseScale = 1.0f;
	};

	/**
	* Neural-network based policy object. Stores various settings and intermediate
	* storage required to evaluate the given network for the provided number of instances.
	*/
	struct LEARNING_API FNeuralNetworkPolicyFunction : public FFunctionObject
	{
		FNeuralNetworkPolicyFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InObservationNum,
			const int32 InActionNum,
			const int32 InMemoryStateNum,
			const TSharedPtr<INeuralNetwork>& InNeuralNetwork,
			const uint32 InSeed,
			const FNeuralNetworkInferenceSettings& InInferenceSettings = FNeuralNetworkInferenceSettings(),
			const FNeuralNetworkPolicyFunctionSettings& InSettings = FNeuralNetworkPolicyFunctionSettings());

		virtual void Evaluate(const FIndexSet Instances) override final;

		/** Sets the NeuralNetwork and re-creates the NeuralNetworkInference object */
		void UpdateNeuralNetwork(const TSharedPtr<INeuralNetwork>& NewNeuralNetwork);

		int32 MaxInstanceNum = 0;
		int32 ObservationNum = 0;
		int32 ActionNum = 0;
		int32 MemoryStateNum = 0;
		TSharedPtr<INeuralNetwork> NeuralNetwork;
		TSharedPtr<INeuralNetworkInference> NeuralNetworkInference;
		FNeuralNetworkInferenceSettings InferenceSettings;
		FNeuralNetworkPolicyFunctionSettings Settings;

		TArrayMapHandle<1, uint32> SeedHandle;

		TArrayMapHandle<2, float> InputObservationHandle;
		TArrayMapHandle<2, float> InputMemoryStateHandle;
		TArrayMapHandle<2, float> InputNetworkHandle;

		TArrayMapHandle<2, float> OutputNetworkHandle;
		TArrayMapHandle<2, float> OutputMemoryStateHandle;
		TArrayMapHandle<2, float> OutputActionHandle;
		TArrayMapHandle<2, float> OutputActionMeanHandle;
		TArrayMapHandle<2, float> OutputActionStdHandle;
		
		TArrayMapHandle<1, float> ActionNoiseScaleHandle;
	};

	/**
	* Neural-network based critic object. Stores various settings and intermediate
	* storage required to evaluate the given network for the provided number of instances.
	*/
	struct LEARNING_API FNeuralNetworkCriticFunction : public FFunctionObject
	{
		FNeuralNetworkCriticFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InObservationNum,
			const int32 InMemoryStateNum,
			const TSharedPtr<INeuralNetwork>& InNeuralNetwork,
			const FNeuralNetworkInferenceSettings& InInferenceSettings = FNeuralNetworkInferenceSettings());

		virtual void Evaluate(const FIndexSet Instances) override final;

		/** Sets the NeuralNetwork and re-creates the NeuralNetworkInference object */
		void UpdateNeuralNetwork(const TSharedPtr<INeuralNetwork>& NewNeuralNetwork);

		int32 MaxInstanceNum = 0;
		int32 ObservationNum = 0;
		int32 MemoryStateNum = 0;
		TSharedPtr<INeuralNetwork> NeuralNetwork;
		TSharedPtr<INeuralNetworkInference> NeuralNetworkInference;
		FNeuralNetworkInferenceSettings InferenceSettings;

		TArrayMapHandle<2, float> InputObservationHandle;
		TArrayMapHandle<2, float> InputMemoryStateHandle;
		TArrayMapHandle<2, float> InputNetworkHandle;
		TArrayMapHandle<1, float> OutputHandle;
	};
}
