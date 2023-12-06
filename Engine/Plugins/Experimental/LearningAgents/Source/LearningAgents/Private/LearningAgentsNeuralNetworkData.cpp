// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsNeuralNetworkData.h"

#include "LearningAgentsNeuralNetwork.h"
#include "LearningNeuralNetwork.h"

#include "Async/ParallelFor.h"
#include "Modules/ModuleManager.h"

#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeBasicCpu.h"

namespace UE::Learning::Agents
{
	FNeuralNetworkInference::FNeuralNetworkInference(
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

	void FNeuralNetworkInference::CreateInstances(NNE::IModelCPU& InModel)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetworkInference::CreateInstances);

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

	void FNeuralNetworkInference::Evaluate(
		TLearningArrayView<2, float> Output,
		const TLearningArrayView<2, const float> Input,
		const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetworkInference::Evaluate);

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
				{ { (void*)InputBuffer.GetData(), InputBuffer.Slice(0, InstanceNum).Num() * sizeof(float) } },
				{ { (void*)OutputBuffer.GetData(), OutputBuffer.Slice(0, InstanceNum).Num() * sizeof(float) } });
		}

		Array::Check(OutputBuffer.Slice(0, InstanceNum));

		// Scatter Outputs

		for (int32 InstanceIdx = 0; InstanceIdx < InstanceNum; InstanceIdx++)
		{
			Array::Copy(Output[Instances[InstanceIdx]], OutputBuffer[InstanceIdx]);
		}

		Array::Check(Output, Instances);
	}

	FNeuralNetwork::FNeuralNetwork(ULearningAgentsNeuralNetworkData& InParent) : Parent(InParent) {}

	void FNeuralNetwork::ReloadFromFileData()
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetwork::ReloadFromFileData);

		if (!Parent.ModelData)
		{
			Parent.ModelData = NewObject<UNNEModelData>(&Parent);
		}

		Parent.ModelData->Init(TEXT("ubnne"), Parent.FileData);

		ensureMsgf(FModuleManager::Get().LoadModule(TEXT("NNERuntimeBasicCpu")), TEXT("Unable to load module for NNE runtime NNERuntimeBasicCpu."));

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

		for (const TWeakPtr<FNeuralNetworkInference>& InferenceObject : InferenceObjects)
		{
			if (TSharedPtr<FNeuralNetworkInference> InferenceObjectPtr = InferenceObject.Pin())
			{
				InferenceObjectPtr->CreateInstances(*Model);
			}
		}
	}

	bool FNeuralNetwork::DeserializeFromBytes(int32& InOutOffset, const TLearningArrayView<1, const uint8> RawBytes)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetwork::DeserializeFromBytes);

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

	void FNeuralNetwork::SerializeToBytes(int32& InOutOffset, TLearningArrayView<1, uint8> OutRawBytes) const
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Agents::FNeuralNetwork::SerializeToBytes);

		Array::Copy(
			OutRawBytes.Slice(InOutOffset, Parent.FileData.Num()),
			TLearningArrayView<1, const uint8>(Parent.FileData));

		InOutOffset += Parent.FileData.Num();
	}

	int32 FNeuralNetwork::GetSerializationByteNum() const
	{
		return Parent.FileData.Num();
	}

	TSharedRef<INeuralNetworkInference> FNeuralNetwork::CreateInferenceObject(
		const int32 MaxBatchSize,
		const FNeuralNetworkInferenceSettings& InSettings)
	{
		TSharedRef<FNeuralNetworkInference> InferenceObject = MakeShared<FNeuralNetworkInference>(
			*Model,
			InSettings,
			MaxBatchSize,
			Parent.InputNum,
			Parent.OutputNum);

		InferenceObjects.Emplace(InferenceObject.ToWeakPtr());

		return InferenceObject;
	}

	int32 FNeuralNetwork::GetInputNum() const { return Parent.InputNum; }
	int32 FNeuralNetwork::GetOutputNum() const { return Parent.OutputNum; }
}


void ULearningAgentsNeuralNetworkData::PostLoad()
{
	Super::PostLoad();

	if (FileData.Num() > 0)
	{
		Network = MakeShared<UE::Learning::Agents::FNeuralNetwork>(*this);
		Network->ReloadFromFileData();
	}
}

TSharedPtr<UE::Learning::INeuralNetwork> ULearningAgentsNeuralNetworkData::GetNetworkInterface()
{
	return Network;
}

void ULearningAgentsNeuralNetworkData::CreateMLP(
	const uint32 InputSize,
	const uint32 OutputSize,
	const uint32 HiddenUnitNum,
	const uint32 LayerNum,
	const ELearningAgentsActivationFunction Activation)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsNeuralNetworkData::CreateMLP);

	UE_LEARNING_CHECK(InputSize > 0);
	UE_LEARNING_CHECK(OutputSize > 0);
	UE_LEARNING_CHECK(HiddenUnitNum > 0);
	UE_LEARNING_CHECK(LayerNum > 0);

	TArray<float> ZerosData;
	ZerosData.Init(0.0f, FMath::Max(FMath::Max(InputSize * HiddenUnitNum, OutputSize * HiddenUnitNum), HiddenUnitNum * HiddenUnitNum));
	TConstArrayView<float> ZerosDataView = ZerosData;

	UE::NNE::RuntimeBasic::FSequentialModelBuilder Builder;

	for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
	{
		const int32 LayerInputNum = LayerIdx == 0 ? InputSize : HiddenUnitNum;
		const int32 LayerOutputNum = LayerIdx == LayerNum - 1 ? OutputSize : HiddenUnitNum;

		Builder.AddLinear(LayerInputNum, LayerOutputNum, ZerosDataView.Slice(0, LayerInputNum * LayerOutputNum), ZerosDataView.Slice(0, LayerOutputNum));

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

	ZerosData.Empty();

	InputNum = InputSize;
	OutputNum = OutputSize;

	if (!Network)
	{
		Network = MakeShared<UE::Learning::Agents::FNeuralNetwork>(*this);
	}

	Network->ReloadFromFileData();
}

void ULearningAgentsNeuralNetworkData::CreateMemoryBackbone(
	const uint32 InputSize,
	const uint32 OutputSize,
	const uint32 MemorySize,
	const uint32 HiddenUnitNum,
	const uint32 PrefixLayerNum,
	const uint32 PostfixLayerNum)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsNeuralNetworkData::CreateMemoryBackbone);

	UE_LEARNING_CHECK(InputSize > 0);
	UE_LEARNING_CHECK(OutputSize > 0);
	UE_LEARNING_CHECK(MemorySize > 0);
	UE_LEARNING_CHECK(HiddenUnitNum > 0);
	UE_LEARNING_CHECK(PrefixLayerNum > 0);
	UE_LEARNING_CHECK(PostfixLayerNum > 0);

	UE::NNE::RuntimeBasic::FMemoryBackboneModelBuilder Builder;
	Builder.BuildEmptyModel(
		InputSize, 
		OutputSize, 
		MemorySize, 
		HiddenUnitNum, 
		PrefixLayerNum, 
		PostfixLayerNum);

	FileData.SetNumUninitialized(Builder.GetWriteByteNum());
	Builder.WriteAndReset(FileData);

	InputNum = InputSize + MemorySize;
	OutputNum = OutputSize + MemorySize;

	if (!Network)
	{
		Network = MakeShared<UE::Learning::Agents::FNeuralNetwork>(*this);
	}

	Network->ReloadFromFileData();
}

void ULearningAgentsNeuralNetworkData::CopyFrom(const ULearningAgentsNeuralNetworkData* Other)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsNeuralNetworkData::CopyFrom);

	if (const ULearningAgentsNeuralNetworkData* OtherData = Cast<ULearningAgentsNeuralNetworkData>(Other))
	{
		FileData = OtherData->FileData;
		InputNum = OtherData->InputNum;
		OutputNum = OtherData->OutputNum;

		if (!Network)
		{
			Network = MakeShared<UE::Learning::Agents::FNeuralNetwork>(*this);
		}

		Network->ReloadFromFileData();
	}
	else
	{
		ensureMsgf(false, TEXT("Cannot copy from object of different derived type."));
	}
}



