// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningNeuralNetwork.h"

#if UE_LEARNING_ISPC
#include "Learning.ispc.generated.h"
#endif

namespace UE::Learning
{
	namespace NeuralNetwork
	{
		static inline void MatMulPlusBias(
			TLearningArrayView<2, float> Output,
			const TLearningArrayView<2, const float> Input,
			const TLearningArrayView<2, const float> Weights,
			const TLearningArrayView<1, const float> Biases,
			const int32 BatchOffset,
			const int32 BatchNum)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetwork::MatMulPlusBias);

			const int32 RowNum = Weights.Num<0>();
			const int32 ColNum = Weights.Num<1>();
			const int32 BufferSize = Input.Num<1>();

#if UE_LEARNING_ISPC
			ispc::LearningLayerMatMulPlusBias(
				Output.Slice(BatchOffset, BatchNum).GetData(),
				Input.Slice(BatchOffset, BatchNum).GetData(),
				Weights.GetData(),
				Biases.GetData(),
				BatchNum,
				BufferSize,
				RowNum,
				ColNum);
#else
			for (int32 BatchIdx = BatchOffset; BatchIdx < BatchOffset + BatchNum; BatchIdx++)
			{
				for (int32 ColIdx = 0; ColIdx < ColNum; ColIdx++)
				{
					Output[BatchIdx][ColIdx] = Biases[ColIdx];
				}

				for (int32 RowIdx = 0; RowIdx < RowNum; RowIdx++)
				{
					const float Value = Input[BatchIdx][RowIdx];

					if (Value != 0.0)
					{
						for (int32 ColIdx = 0; ColIdx < ColNum; ColIdx++)
						{
							Output[BatchIdx][ColIdx] += Value * Weights[RowIdx][ColIdx];
						}
					}
				}
			}
#endif
		}

		static inline float ReLU(const float X)
		{
			return FMath::Max(X, 0.0f);
		}

		static inline float ELU(const float X)
		{
			return X > 0.0f ? X : FMath::Exp(X) - 1.0f;
		}

		static inline float TanH(const float X)
		{
			return FMath::Tanh(X);
		}

		static inline void ActivationReLU(TLearningArrayView<2, float> InputOutput, const int32 BatchOffset, const int32 BatchNum, const int32 InputOutputNum)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetwork::ActivationReLU);

			const int32 BufferSize = InputOutput.Num<1>();

#if UE_LEARNING_ISPC
			ispc::LearningLayerReLU(
				InputOutput.Slice(BatchOffset, BatchNum).GetData(),
				BatchNum,
				BufferSize, 
				InputOutputNum);
#else
			for (int32 BatchIdx = BatchOffset; BatchIdx < BatchOffset + BatchNum; BatchIdx++)
			{
				for (int32 ColIdx = 0; ColIdx < InputOutputNum; ColIdx++)
				{
					InputOutput[BatchIdx][ColIdx] = ReLU(InputOutput[BatchIdx][ColIdx]);
				}
			}
#endif
		}

		static inline void ActivationELU(TLearningArrayView<2, float> InputOutput, const int32 BatchOffset, const int32 BatchNum, const int32 InputOutputNum)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetwork::ActivationELU);

			const int32 BufferSize = InputOutput.Num<1>();

#if UE_LEARNING_ISPC
			ispc::LearningLayerELU(
				InputOutput.Slice(BatchOffset, BatchNum).GetData(),
				BatchNum,
				BufferSize,
				InputOutputNum);
#else
			for (int32 BatchIdx = BatchOffset; BatchIdx < BatchOffset + BatchNum; BatchIdx++)
			{
				for (int32 ColIdx = 0; ColIdx < InputOutputNum; ColIdx++)
				{
					InputOutput[BatchIdx][ColIdx] = ELU(InputOutput[BatchIdx][ColIdx]);
				}
			}
#endif
		}

		static inline void ActivationTanH(TLearningArrayView<2, float> InputOutput, const int32 BatchOffset, const int32 BatchNum, const int32 InputOutputNum)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetwork::ActivationTanH);

			const int32 BufferSize = InputOutput.Num<1>();

#if UE_LEARNING_ISPC
			ispc::LearningLayerTanH(
				InputOutput.Slice(BatchOffset, BatchNum).GetData(),
				BatchNum,
				BufferSize,
				InputOutputNum);
#else
			for (int32 BatchIdx = BatchOffset; BatchIdx < BatchOffset + BatchNum; BatchIdx++)
			{
				for (int32 ValueIdx = 0; ValueIdx < InputOutputNum; ValueIdx++)
				{
					InputOutput[BatchIdx][ValueIdx] = TanH(InputOutput[BatchIdx][ValueIdx]);
				}
			}
#endif
		}

		static constexpr int32 MagicNumber = 0x0cd353cf;
		static constexpr int32 VersionNumber = 1;
	}

	bool FNeuralNetworkMLP::DeserializeFromBytes(int32& InOutOffset, const TLearningArrayView<1, const uint8> RawBytes)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkMLP::DeserializeFromBytes);

		// At least Magic Number and Version Number Required
		if (RawBytes.Num() < sizeof(int32) * 2)
		{
			return false;
		}

		int32 Magic;
		UE::Learning::DeserializeFromBytes(InOutOffset, RawBytes, Magic);
		if (Magic != NeuralNetwork::MagicNumber) { return false; }

		int32 Version;
		UE::Learning::DeserializeFromBytes(InOutOffset, RawBytes, Version);
		if (Version != NeuralNetwork::VersionNumber) { return false; }

		int32 ActivationFunctionInt;
		UE::Learning::DeserializeFromBytes(InOutOffset, RawBytes, ActivationFunctionInt);
		ActivationFunction = (EActivationFunction)ActivationFunctionInt;

		int32 LayerNum;
		UE::Learning::DeserializeFromBytes(InOutOffset, RawBytes, LayerNum);

		Weights.SetNum(LayerNum);
		Biases.SetNum(LayerNum);

		for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
		{
			UE::Learning::Array::DeserializeFromBytes(InOutOffset, RawBytes, Weights[LayerIdx]);
			UE::Learning::Array::DeserializeFromBytes(InOutOffset, RawBytes, Biases[LayerIdx]);
		}

		return true;
	}

	void FNeuralNetworkMLP::SerializeToBytes(int32& InOutOffset, TLearningArrayView<1, uint8> OutRawBytes) const
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkMLP::SerializeToBytes);

		const int32 LayerNum = Weights.Num();

		UE::Learning::SerializeToBytes(InOutOffset, OutRawBytes, NeuralNetwork::MagicNumber);
		UE::Learning::SerializeToBytes(InOutOffset, OutRawBytes, NeuralNetwork::VersionNumber);
		UE::Learning::SerializeToBytes(InOutOffset, OutRawBytes, (int32)ActivationFunction);
		UE::Learning::SerializeToBytes(InOutOffset, OutRawBytes, LayerNum);

		for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
		{
			UE::Learning::Array::SerializeToBytes(InOutOffset, OutRawBytes, Weights[LayerIdx]);
			UE::Learning::Array::SerializeToBytes(InOutOffset, OutRawBytes, Biases[LayerIdx]);
		}
	}

	int32 FNeuralNetworkMLP::GetSerializationByteNum() const
	{
		const int32 LayerNum = GetLayerNum();

		int32 Total = 0;

		Total += sizeof(int32);  // Magic
		Total += sizeof(int32);  // Version
		Total += sizeof(int32);  // Activation
		Total += sizeof(int32);  // Layer Num

		for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
		{
			Total += UE::Learning::Array::SerializationByteNum<2, float>(Weights[LayerIdx].Shape());
			Total += UE::Learning::Array::SerializationByteNum<1, float>(Biases[LayerIdx].Shape());
		}

		return Total;
	}

	TSharedRef<INeuralNetworkInference> FNeuralNetworkMLP::CreateInferenceObject(
		const int32 MaxBatchSize,
		const FNeuralNetworkInferenceSettings& Settings)
	{
		return MakeShared<FNeuralNetworkInferenceMLP>(*this, MaxBatchSize, Settings);
	}

	int32 FNeuralNetworkMLP::GetInputNum() const
	{
		UE_LEARNING_CHECK(Weights.Num() > 0);
		return Weights[0].Num<0>();
	}

	int32 FNeuralNetworkMLP::GetOutputNum() const
	{
		UE_LEARNING_CHECK(Weights.Num() > 0);
		return Weights.Last().Num<1>();
	}

	int32 FNeuralNetworkMLP::GetHiddenNum() const
	{
		UE_LEARNING_CHECK(Weights.Num() > 0);
		return Weights[0].Num<1>();
	}

	int32 FNeuralNetworkMLP::GetLayerNum() const
	{
		return Weights.Num();
	}

	void FNeuralNetworkMLP::Resize(
		const int32 InputNum,
		const int32 OutputNum,
		const int32 HiddenNum,
		const int32 LayerNum)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkMLP::Resize);
		UE_LEARNING_CHECKF(LayerNum >= 2, TEXT("At least two layers required (input and output layers)"));

		Weights.SetNum(LayerNum);
		Biases.SetNum(LayerNum);

		Weights[0].SetNumUninitialized({ InputNum, HiddenNum });
		Biases[0].SetNumUninitialized({ HiddenNum });

		for (int32 LayerIdx = 0; LayerIdx < LayerNum - 2; LayerIdx++)
		{
			Weights[LayerIdx + 1].SetNumUninitialized({ HiddenNum, HiddenNum });
			Biases[LayerIdx + 1].SetNumUninitialized({ HiddenNum });
		}

		Weights[LayerNum - 1].SetNumUninitialized({ HiddenNum, OutputNum });
		Biases[LayerNum - 1].SetNumUninitialized({ OutputNum });

		for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
		{
			Array::Zero(Weights[LayerIdx]);
			Array::Zero(Biases[LayerIdx]);
		}
	}

	FNeuralNetworkInferenceMLP::FNeuralNetworkInferenceMLP(const FNeuralNetworkMLP& InNetwork, const int32 MaxBatchSize, const FNeuralNetworkInferenceSettings& InSettings)
		: Network(InNetwork)
		, Settings(InSettings)
	{
		const int32 BufferSize = FMath::Max(FMath::Max(Network.GetInputNum(), Network.GetOutputNum()), Network.GetHiddenNum());

		FrontBuffer.SetNumUninitialized({ MaxBatchSize, BufferSize });
		BackBuffer.SetNumUninitialized({ MaxBatchSize, BufferSize });
	}

	void FNeuralNetworkInferenceMLP::Evaluate(
		TLearningArrayView<2, float> Output,
		const TLearningArrayView<2, const float> Input,
		const FIndexSet Instances) 
	{
		const int32 InputNum = Network.GetInputNum();
		const int32 OutputNum = Network.GetOutputNum();
		const int32 LayerNum = Network.GetLayerNum();

		UE_LEARNING_CHECK(Input.Num<1>() == InputNum);
		UE_LEARNING_CHECK(Output.Num<1>() == OutputNum);

		// Gather Instance Inputs

		Array::Check(Input, Instances);

		const TLearningArrayView<2, float> InputBuffer = FrontBuffer;

		for (int32 InstanceIdx = 0; InstanceIdx < Instances.Num(); InstanceIdx++)
		{
			Array::Copy(InputBuffer[InstanceIdx].Slice(0, InputNum), Input[Instances[InstanceIdx]]);
		}

		// Evaluate Network

		auto EvaluationFunction = [this, LayerNum](const int32 BatchOffset, const int32 BatchSize)
		{
			for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
			{
				const TLearningArrayView<2, float> LayerInputBuffer = LayerIdx % 2 == 0 ? FrontBuffer : BackBuffer;
				const TLearningArrayView<2, float> LayerOutputBuffer = LayerIdx % 2 == 0 ? BackBuffer : FrontBuffer;
				const int32 LayerOutputNum = Network.Weights[LayerIdx].Num<1>();

				NeuralNetwork::MatMulPlusBias(
					LayerOutputBuffer,
					LayerInputBuffer,
					Network.Weights[LayerIdx],
					Network.Biases[LayerIdx],
					BatchOffset,
					BatchSize);

				if (LayerIdx != LayerNum - 1)
				{
					switch (Network.ActivationFunction)
					{
					case EActivationFunction::ReLU: NeuralNetwork::ActivationReLU(LayerOutputBuffer, BatchOffset, BatchSize, LayerOutputNum); break;
					case EActivationFunction::ELU: NeuralNetwork::ActivationELU(LayerOutputBuffer, BatchOffset, BatchSize, LayerOutputNum); break;
					case EActivationFunction::TanH: NeuralNetwork::ActivationTanH(LayerOutputBuffer, BatchOffset, BatchSize, LayerOutputNum); break;
					default: UE_LEARNING_CHECKF(false, TEXT("Unimplemented"));
					}
				}
			}
		};

		if (Settings.bParallelEvaluation && Instances.Num() > Settings.MinParallelBatchSize)
		{
			Learning::SlicedParallelFor(Instances.Num(), Settings.MinParallelBatchSize, EvaluationFunction);
		}
		else
		{
			EvaluationFunction(0, Instances.Num());
		}

		// Scatter Instance Outputs

		const TLearningArrayView<2, float> OutputBuffer = LayerNum % 2 == 0 ? FrontBuffer : BackBuffer;

		for (int32 InstanceIdx = 0; InstanceIdx < Instances.Num(); InstanceIdx++)
		{
			Array::Copy(Output[Instances[InstanceIdx]], OutputBuffer[InstanceIdx].Slice(0, OutputNum));
		}

		Array::Check(Output, Instances);
	}
}
