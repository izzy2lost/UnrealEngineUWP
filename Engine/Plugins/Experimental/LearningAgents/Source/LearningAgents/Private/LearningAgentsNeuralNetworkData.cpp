// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsNeuralNetworkData.h"

#include "LearningAgentsNeuralNetwork.h"
#include "LearningNeuralNetwork.h"
#include "Async/ParallelFor.h"

#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeBasicCpu.h"

void ULearningAgentsMLPNeuralNetworkData::Serialize(FArchive& Ar) 
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		bool bValid;
		Ar << bValid;

		if (bValid)
		{
			if (!Network)
			{
				Network = MakeShared<UE::Learning::FNeuralNetworkMLP>();
			}

			TArray<uint8> Bytes;
			Ar << Bytes;
			int32 Offset = 0;
			Network->DeserializeFromBytes(Offset, Bytes);
			UE_LEARNING_CHECK(Offset == Bytes.Num());
		}
		else
		{
			Network.Reset();
		}
	}
	else if (Ar.IsSaving())
	{
		bool bValid = Network != nullptr;
		Ar << bValid;

		if (bValid)
		{
			TArray<uint8> Bytes;
			Bytes.SetNumUninitialized(Network->GetSerializationByteNum());
			int32 Offset = 0;
			Network->SerializeToBytes(Offset, Bytes);
			UE_LEARNING_CHECK(Offset == Bytes.Num());
			Ar << Bytes;
		}
	}
}

TSharedPtr<UE::Learning::INeuralNetwork> ULearningAgentsMLPNeuralNetworkData::GetNetworkInterface()
{
	return Network;
}

void ULearningAgentsMLPNeuralNetworkData::CreateMLP(
	const uint32 InputSize,
	const uint32 OutputSize,
	const uint32 HiddenUnitNum,
	const uint32 LayerNum,
	const ELearningAgentsActivationFunction Activation)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsMLPNeuralNetworkData::CreateMLP);

	if (!Network)
	{
		Network = MakeShared<UE::Learning::FNeuralNetworkMLP>();
	}

	Network->Resize(
		InputSize,
		OutputSize,
		HiddenUnitNum,
		LayerNum);

	switch (Activation)
	{
	case ELearningAgentsActivationFunction::ELU: Network->ActivationFunction = UE::Learning::EActivationFunction::ELU; break;
	case ELearningAgentsActivationFunction::ReLU: Network->ActivationFunction = UE::Learning::EActivationFunction::ReLU; break;
	case ELearningAgentsActivationFunction::TanH: Network->ActivationFunction = UE::Learning::EActivationFunction::TanH; break;
	default: UE_LEARNING_CHECKF(false, TEXT("Unimplemented"));
	}
}

void ULearningAgentsMLPNeuralNetworkData::CopyFrom(const ULearningAgentsNeuralNetworkData* Other)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsMLPNeuralNetworkData::CopyFrom);

	if (const ULearningAgentsMLPNeuralNetworkData* OtherData = Cast<ULearningAgentsMLPNeuralNetworkData>(Other))
	{
		if (OtherData->Network)
		{
			*Network = *OtherData->Network;
		}
		else
		{
			Network.Reset();
		}
	}
	else
	{
		ensureMsgf(false, TEXT("Cannot copy from object of different derived type."));
	}
}

namespace UE::Learning::Agents
{
	FNeuralNetworkNNEInference::FNeuralNetworkNNEInference(
		UE::NNE::IModelCPU& InModel,
		const FNeuralNetworkInferenceSettings& InSettings,
		const int32 MaxBatchSize,
		const int32 InputSize,
		const int32 OutputSize)
		: Settings(InSettings)
	{
		InputBuffer.SetNumUninitialized({ MaxBatchSize, InputSize });
		OutputBuffer.SetNumUninitialized({ MaxBatchSize, OutputSize });

		CreateInstances(InModel);
	}

	void FNeuralNetworkNNEInference::CreateInstances(NNE::IModelCPU& InModel)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetworkNNEInference::CreateInstances);

		const int32 MaxBatchSize = InputBuffer.Num<0>();
		const int32 InputSize = InputBuffer.Num<1>();
		const int32 OutputSize = OutputBuffer.Num<1>();

		if (Settings.bParallelEvaluation)
		{
			const int32 IdealInferenceInstanceNum = ParallelForImpl::GetNumberOfThreadTasks(MaxBatchSize, Settings.MinParallelBatchSize, EParallelForFlags::None);
			const int32 InferenceInstanceSliceLength = FMath::DivideAndRoundUp((int32)MaxBatchSize, IdealInferenceInstanceNum);
			const int32 InferenceInstanceNum = FMath::DivideAndRoundUp((int32)MaxBatchSize, InferenceInstanceSliceLength);

			InferenceInstances.Empty(InferenceInstanceNum);

			for (int32 InferenceInstanceIdx = 0; InferenceInstanceIdx < InferenceInstanceNum; InferenceInstanceIdx++)
			{
				InferenceInstances.Emplace(InModel.CreateModelInstanceCPU());
				InferenceInstances.Last()->SetInputTensorShapes({ NNE::FTensorShape::Make({(uint32)InferenceInstanceSliceLength, (uint32)InputSize}) });
			}
		}
		else
		{
			InferenceInstances.Empty(1);
			InferenceInstances.Emplace(InModel.CreateModelInstanceCPU());
			InferenceInstances.Last()->SetInputTensorShapes({ NNE::FTensorShape::Make({(uint32)MaxBatchSize, (uint32)InputSize}) });
		}
	}

	void FNeuralNetworkNNEInference::Evaluate(
		TLearningArrayView<2, float> Output,
		const TLearningArrayView<2, const float> Input,
		const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetworkNNEInference::Evaluate);

		const int32 InstanceNum = Instances.Num();
		const int32 InputNum = Input.Num<1>();
		const int32 OutputNum = Output.Num<1>();

		// Gather Inputs

		Array::Check(Input, Instances);

		for (int32 InstanceIdx = 0; InstanceIdx < InstanceNum; InstanceIdx++)
		{
			Array::Copy(InputBuffer[InstanceIdx], Input[Instances[InstanceIdx]]);
		}

		// Run Inference

		Array::Check(InputBuffer.Slice(0, InstanceNum));

		if (Settings.bParallelEvaluation && InstanceNum > Settings.MinParallelBatchSize)
		{
			const int32 IdealInferenceInstanceNum = ParallelForImpl::GetNumberOfThreadTasks(InstanceNum, Settings.MinParallelBatchSize, EParallelForFlags::None);
			const int32 InferenceInstanceSliceLength = FMath::DivideAndRoundUp(InstanceNum, IdealInferenceInstanceNum);
			const int32 InferenceInstanceNum = FMath::DivideAndRoundUp(InstanceNum, InferenceInstanceSliceLength);

			check(InferenceInstanceNum <= InferenceInstances.Num());

			ParallelFor(InferenceInstanceNum, [this, InstanceNum, InputNum, InferenceInstanceSliceLength](int32 InferenceInstanceIdx)
				{
					const int32 StartIndex = InferenceInstanceIdx * InferenceInstanceSliceLength;
					const int32 StopIndex = FMath::Min((InferenceInstanceIdx + 1) * InferenceInstanceSliceLength, InstanceNum);

					TLearningArrayView<2, float> InputBufferSlice = InputBuffer.Slice(StartIndex, StopIndex - StartIndex);
					TLearningArrayView<2, float> OutputBufferSlice = OutputBuffer.Slice(StartIndex, StopIndex - StartIndex);

					InferenceInstances[InferenceInstanceIdx]->SetInputTensorShapes({ NNE::FTensorShape::Make({(uint32)(StopIndex - StartIndex), (uint32)InputNum}) });
					InferenceInstances[InferenceInstanceIdx]->RunSync(
						{ { (void*)InputBufferSlice.GetData(), InputBufferSlice.Num() * sizeof(float) } },
						{ { (void*)OutputBufferSlice.GetData(), OutputBufferSlice.Num() * sizeof(float) } });
				});
		}
		else
		{
			InferenceInstances[0]->SetInputTensorShapes({ NNE::FTensorShape::Make({(uint32)InstanceNum, (uint32)InputNum}) });
			InferenceInstances[0]->RunSync(
				{ { (void*)InputBuffer.GetData(), InputBuffer.Num() * sizeof(float) } },
				{ { (void*)OutputBuffer.GetData(), OutputBuffer.Num() * sizeof(float) } });
		}

		Array::Check(OutputBuffer.Slice(0, InstanceNum));

		// Scatter Outputs

		for (int32 InstanceIdx = 0; InstanceIdx < InstanceNum; InstanceIdx++)
		{
			Array::Copy(Output[Instances[InstanceIdx]], OutputBuffer[InstanceIdx]);
		}

		Array::Check(Output, Instances);
	}

	FNeuralNetworkNNE::FNeuralNetworkNNE(ULearningAgentsNNENeuralNetworkData& InParent) : Parent(InParent) {}

	void FNeuralNetworkNNE::ReloadFromFileData()
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetworkNNE::ReloadFromFileData);

		if (!Parent.ModelData)
		{
			Parent.ModelData = NewObject<UNNEModelData>(&Parent);
		}

		Parent.ModelData->Init(TEXT("ubnne"), Parent.FileData);

		TWeakInterfacePtr<INNERuntimeCPU> RuntimeCPU = NNE::GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeBasicCpu"));

		if (ensureMsgf(RuntimeCPU.IsValid(), TEXT("Could not find requested NNE Runtime")))
		{
			Model = RuntimeCPU->CreateModelCPU(Parent.ModelData);
		}

		// If we are not in the editor we can now clear the FileData and FileType since these will be
		// using additional memory and we are not going to save this asset and so don't require them.

#if !WITH_EDITOR
		Parent.ModelData->ClearFileDataAndFileType();
#endif

		// We need to tell all the created inference objects to update their instances since the Model has changed
		// and so the instance objects themselves are either no longer valid or tied to the old model.

		for (const TWeakPtr<FNeuralNetworkNNEInference>& InferenceObject : InferenceObjects)
		{
			if (TSharedPtr<FNeuralNetworkNNEInference> InferenceObjectPtr = InferenceObject.Pin())
			{
				InferenceObjectPtr->CreateInstances(*Model);
			}
		}
	}

	bool FNeuralNetworkNNE::DeserializeFromBytes(int32& InOutOffset, const TLearningArrayView<1, const uint8> RawBytes)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetworkNNE::DeserializeFromBytes);

		if (RawBytes.Num() - InOutOffset < Parent.FileData.Num())
		{
			return false;
		}

		Array::Copy(
			TLearningArrayView<1, uint8>(Parent.FileData),
			RawBytes.Slice(InOutOffset, Parent.FileData.Num()));

		InOutOffset += Parent.FileData.Num();

		ReloadFromFileData();

		return true;
	}

	void FNeuralNetworkNNE::SerializeToBytes(int32& InOutOffset, TLearningArrayView<1, uint8> OutRawBytes) const
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetworkNNE::SerializeToBytes);

		Array::Copy(
			OutRawBytes.Slice(InOutOffset, Parent.FileData.Num()),
			TLearningArrayView<1, const uint8>(Parent.FileData));

		InOutOffset += Parent.FileData.Num();
	}

	int32 FNeuralNetworkNNE::GetSerializationByteNum() const
	{
		return Parent.FileData.Num();
	}

	TSharedRef<INeuralNetworkInference> FNeuralNetworkNNE::CreateInferenceObject(
		const int32 MaxBatchSize,
		const FNeuralNetworkInferenceSettings& InSettings)
	{
		TSharedRef<FNeuralNetworkNNEInference> InferenceObject = MakeShared<FNeuralNetworkNNEInference>(
			*Model,
			InSettings,
			MaxBatchSize,
			Parent.InputNum,
			Parent.OutputNum);

		InferenceObjects.Emplace(InferenceObject.ToWeakPtr());

		return InferenceObject;
	}

	int32 FNeuralNetworkNNE::GetInputNum() const { return Parent.InputNum; }
	int32 FNeuralNetworkNNE::GetOutputNum() const { return Parent.OutputNum; }
}


void ULearningAgentsNNENeuralNetworkData::PostLoad()
{
	if (FileData.Num() > 0)
	{
		Network = MakeShared<UE::Learning::Agents::FNeuralNetworkNNE>(*this);
	}
}

TSharedPtr<UE::Learning::INeuralNetwork> ULearningAgentsNNENeuralNetworkData::GetNetworkInterface()
{
	return Network;
}

void ULearningAgentsNNENeuralNetworkData::CreateMLP(
	const uint32 InputSize,
	const uint32 OutputSize,
	const uint32 HiddenUnitNum,
	const uint32 LayerNum,
	const ELearningAgentsActivationFunction Activation)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsNNENeuralNetworkData::CreateMLP);

	UE_LEARNING_CHECK(InputSize > 0);
	UE_LEARNING_CHECK(OutputSize > 0);
	UE_LEARNING_CHECK(HiddenUnitNum > 0);
	UE_LEARNING_CHECK(LayerNum > 0);

	TArray<TArray<float>> LinearWeightData;
	TArray<TArray<float>> LinearBiasData;

	UE::NNE::RuntimeBasic::FSequentialModelBuilder Builder;

	for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
	{
		const int32 LayerInputNum = LayerIdx == 0 ? InputSize : HiddenUnitNum;
		const int32 LayerOutputNum = LayerIdx == LayerNum - 1 ? OutputSize : HiddenUnitNum;

		LinearWeightData.AddDefaulted();
		LinearWeightData.Last().Init(0.0f, LayerInputNum * LayerOutputNum);

		LinearBiasData.AddDefaulted();
		LinearBiasData.Last().Init(0.0f, LayerOutputNum);

		Builder.AddLinear(LayerInputNum, LayerOutputNum, LinearWeightData.Last(), LinearBiasData.Last());

		if (LayerIdx != LayerNum - 1)
		{
			switch (Activation)
			{
			case ELearningAgentsActivationFunction::ELU: Builder.AddELU(); break;
			case ELearningAgentsActivationFunction::ReLU: Builder.AddReLU(); break;
			case ELearningAgentsActivationFunction::TanH: Builder.AddTanH(); break;
			default: UE_LEARNING_CHECKF(false, TEXT("Unimplemented")); break;
			}
		}
	}

	FileData.SetNumUninitialized(Builder.GetWriteByteNum());
	Builder.WriteAndReset(FileData);

	LinearWeightData.Empty();
	LinearBiasData.Empty();

	InputNum = InputSize;
	OutputNum = OutputSize;

	if (!Network)
	{
		Network = MakeShared<UE::Learning::Agents::FNeuralNetworkNNE>(*this);
	}

	Network->ReloadFromFileData();
}

void ULearningAgentsNNENeuralNetworkData::CopyFrom(const ULearningAgentsNeuralNetworkData* Other)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsNNENeuralNetworkData::CopyFrom);

	if (const ULearningAgentsNNENeuralNetworkData* OtherData = Cast<ULearningAgentsNNENeuralNetworkData>(Other))
	{
		FileData = OtherData->FileData;
		InputNum = OtherData->InputNum;
		OutputNum = OtherData->OutputNum;

		if (!Network)
		{
			Network = MakeShared<UE::Learning::Agents::FNeuralNetworkNNE>(*this);
		}

		Network->ReloadFromFileData();
	}
	else
	{
		ensureMsgf(false, TEXT("Cannot copy from object of different derived type."));
	}
}



