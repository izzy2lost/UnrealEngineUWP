// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"

#include "Templates/SharedPointer.h"

namespace UE::Learning
{
	struct INeuralNetworkInference;
	
	/**
	* Settings object for a neural network instance
	*/
	struct LEARNING_API FNeuralNetworkInferenceSettings
	{
		// If to allow for multi-threaded evaluation
		bool bParallelEvaluation = true;

		// Minimum batch size to use for multi-threaded evaluation
		uint16 MinParallelBatchSize = 16;
	};

	/**
	* Interface for a Neural Network
	*/
	struct INeuralNetwork
	{
		virtual ~INeuralNetwork() {}

		/** Deserialize the network from the raw bytes starting at the given offset. Returns true if successful. */
		virtual bool DeserializeFromBytes(int32& InOutOffset, const TLearningArrayView<1, const uint8> RawBytes) = 0;

		/** Serialize the network to the raw bytes starting at the given offset. */
		virtual void SerializeToBytes(int32& InOutOffset, TLearningArrayView<1, uint8> OutRawBytes) const = 0;

		/** Get the number of bytes required to serialize this network. */
		virtual int32 GetSerializationByteNum() const = 0;

		/** Create a new inference object for this network with the given maximum batch size and inference settings. */
		virtual TSharedRef<INeuralNetworkInference> CreateInferenceObject(
			const int32 MaxBatchSize,
			const FNeuralNetworkInferenceSettings& Settings = FNeuralNetworkInferenceSettings()) = 0;

		/** Get the number of inputs expected by this network */
		virtual int32 GetInputNum() const = 0;

		/** Get the number of outputs expected by this network */
		virtual int32 GetOutputNum() const = 0;

		/** Get the name of the python class used to represent this type of network during training. */
		virtual const TCHAR* GetPythonClassName() const = 0;
	};

	/**
	* Interface for a Neural Network Inference
	*/
	struct INeuralNetworkInference
	{
		virtual ~INeuralNetworkInference() {}

		virtual void Evaluate(
			TLearningArrayView<2, float> Output,
			const TLearningArrayView<2, const float> Input,
			const FIndexSet Instances) = 0;
	};

	/**
	* Activation Function for use in basic MLP Neural Network
	*/
	enum class EActivationFunction : uint8
	{
		// ReLU Activation - Fast to train and evaluate but occasionally causes gradient collapse and untrainable networks.
		ReLU = 0,

		// ELU Activation - Generally performs better than ReLU and is not prone to gradient collapse but slower to evaluate.
		ELU = 1,

		// TanH Activation - Smooth activation function that is slower to train and evaluate but sometimes more stable for certain tasks.
		TanH = 2,
	};

	/**
	* Basic Implementation for a MLP Neural Network
	*/
	struct LEARNING_API FNeuralNetworkMLP : public INeuralNetwork
	{
		friend struct FNeuralNetworkInstanceMLP;

		//~ Begin INeuralNetwork Interface
		virtual bool DeserializeFromBytes(int32& InOutOffset, const TLearningArrayView<1, const uint8> RawBytes) override final;
		virtual void SerializeToBytes(int32& InOutOffset, TLearningArrayView<1, uint8> OutRawBytes) const override final;
		virtual int32 GetSerializationByteNum() const override final;
		virtual TSharedRef<INeuralNetworkInference> CreateInferenceObject(
			const int32 MaxBatchSize,
			const FNeuralNetworkInferenceSettings& Settings = FNeuralNetworkInferenceSettings()) override final;
		virtual int32 GetInputNum() const override final;
		virtual int32 GetOutputNum() const override final;
		virtual const TCHAR* GetPythonClassName() const override final { return TEXT("NeuralNetworkMLP"); };
		//~ End INeuralNetwork Interface

		int32 GetHiddenNum() const;
		int32 GetLayerNum() const;

		void Resize(
			const int32 InputNum,
			const int32 OutputNum,
			const int32 HiddenNum,
			const int32 LayerNum);

		EActivationFunction ActivationFunction = EActivationFunction::ELU;
		TArray<TLearningArray<2, float>, TInlineAllocator<16>> Weights;
		TArray<TLearningArray<1, float>, TInlineAllocator<16>> Biases;
	};

	/**
	* Basic Implementation for a MLP Neural Network Inference
	*/
	struct LEARNING_API FNeuralNetworkInferenceMLP : public INeuralNetworkInference
	{
		FNeuralNetworkInferenceMLP(
			const FNeuralNetworkMLP& InNetwork,
			const int32 MaxBatchSize,
			const FNeuralNetworkInferenceSettings& InSettings);

		//~ Begin INeuralNetworkInference Interface
		virtual void Evaluate(
			TLearningArrayView<2, float> Output,
			const TLearningArrayView<2, const float> Input,
			const FIndexSet Instances) override final;
		//~ End INeuralNetworkInference Interface

		const FNeuralNetworkMLP& Network;
		FNeuralNetworkInferenceSettings Settings;
		TLearningArray<2, float> FrontBuffer;
		TLearningArray<2, float> BackBuffer;
	};
}
