// Copyright Epic Games, Inc. All Rights Reserved.
#include "NNERuntimeORTCpuModel.h"
#include "NNERuntimeORTCpu.h"
#include "NNERuntimeORTCpuUtils.h"
#include "NNEProfilingTimer.h"

namespace UE::NNERuntimeORTCpu::Private
{

	FModelInstanceCPU::FModelInstanceCPU() :
		bIsLoaded(false),
		bHasRun(false)
	{ }

	FModelInstanceCPU::FModelInstanceCPU(
		Ort::Env* InORTEnvironment,
		const FRuntimeConf& InRuntimeConf) :
		bIsLoaded(false),
		bHasRun(false),
		RuntimeConf(InRuntimeConf),
		ORTEnvironment(InORTEnvironment)
	{ }

	bool FModelInstanceCPU::Init(TConstArrayView<uint8> ModelData)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FModelInstanceCPU_Init"), STAT_FModelInstanceCPU_Init, STATGROUP_NNE);
		
		// Get the header size
		int32 GuidSize = sizeof(UNNERuntimeORTCustomCpuImpl::GUID);
		int32 VersionSize = sizeof(UNNERuntimeORTCustomCpuImpl::Version);

		// Clean previous networks
		bIsLoaded = false;
		TConstArrayView<uint8> ModelBuffer = TConstArrayView<uint8>(&(ModelData.GetData()[GuidSize + VersionSize]), ModelData.Num() - GuidSize - VersionSize);

		// Checking Inference Model 
		{
			if (ModelBuffer.Num() == 0)
			{
				UE_LOG(LogNNE, Warning, TEXT("FModelInstanceCPU::Load(): Input model data is empty."));
				return false;
			}

		}

#if WITH_EDITOR
		try
#endif //WITH_EDITOR
		{
			if (!InitializedAndConfigureMembers())
			{
				UE_LOG(LogNNE, Warning, TEXT("Load(): InitializedAndConfigureMembers failed."));
				return false;
			}

			{
				DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FModelInstanceCPU_Init_CreateORTSession"), STAT_FModelInstanceCPU_Init_CreateORTSession, STATGROUP_NNE);
				// Read model from InferenceModel
				Session = MakeUnique<Ort::Session>(*ORTEnvironment, ModelBuffer.GetData(), ModelBuffer.Num(), *SessionOptions);
			}

			if (!ConfigureTensors(true))
			{
				UE_LOG(LogNNE, Warning, TEXT("Load(): Failed to configure Inputs tensors."));
				return false;
			}

			if (!ConfigureTensors(false))
			{
				UE_LOG(LogNNE, Warning, TEXT("Load(): Failed to configure Outputs tensors."));
				return false;
			}
		}
#if WITH_EDITOR
		catch (const Ort::Exception& Exception)
		{
			UE_LOG(LogNNE, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
			return false;
		}
		catch (...)
		{
			UE_LOG(LogNNE, Error, TEXT("Unknown exception!"));
			return false;
		}
#endif //WITH_EDITOR

		bIsLoaded = true;

		// Reset Stats
		ResetStats();

		return IsLoaded();
	}

	bool FModelInstanceCPU::IsLoaded() const
	{
		return bIsLoaded;
	}

	bool FModelInstanceCPU::InitializedAndConfigureMembers()
	{
		// Initialize 
		// Set up ORT and create an environment
		Allocator = MakeUnique<Ort::AllocatorWithDefaultOptions>();
		AllocatorInfo = MakeUnique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU));

		// Configure Session
		SessionOptions = MakeUnique<Ort::SessionOptions>();

		// Configure number threads
		//SessionOptions->SetIntraOpNumThreads(RuntimeConf.NumberOfThreads);
		// Uncomment if you want to change the priority of the threads, by default is TPri_Normal
		//SessionOptions->SetPriorityOpThreads(RuntimeConf.ThreadPriority);

		// ORT CPU
		SessionOptions->SetGraphOptimizationLevel(RuntimeConf.OptimizationLevel); // ORT_ENABLE_ALL, ORT_ENABLE_EXTENDED, ORT_ENABLE_BASIC, ORT_DISABLE_ALL
		SessionOptions->EnableCpuMemArena();

		return true;
	}


	bool FModelInstanceCPU::ConfigureTensors(const bool InIsInput)
	{
		const bool bIsInput = InIsInput;

		const uint32 NumberTensors = bIsInput ? Session->GetInputCount() : Session->GetOutputCount();

		TArray<NNE::FTensorDesc>& SymbolicTensorDescs = bIsInput ? InputSymbolicTensors : OutputSymbolicTensors;
		TArray<ONNXTensorElementDataType>& TensorsORTType = bIsInput ? InputTensorsORTType : OutputTensorsORTType;
		TArray<Ort::AllocatedStringPtr>& TensorNames = bIsInput ? InputTensorNames : OutputTensorNames;
		TArray<const char*>& TensorNamePtrs = bIsInput ? InputTensorNamePtrs : OutputTensorNamePtrs;

		for (uint32 TensorIndex = 0; TensorIndex < NumberTensors; ++TensorIndex)
		{
			// Get Tensor name
			Ort::AllocatedStringPtr CurTensorName = bIsInput ? Session->GetInputNameAllocated(TensorIndex, *Allocator) : Session->GetOutputNameAllocated(TensorIndex, *Allocator);
			const char* CurTensorNamePtr = CurTensorName.get();

			TensorNames.Add(MoveTemp(CurTensorName));
			TensorNamePtrs.Add(CurTensorNamePtr);

			// Get node type
			Ort::TypeInfo CurrentTypeInfo = bIsInput ? Session->GetInputTypeInfo(TensorIndex) : Session->GetOutputTypeInfo(TensorIndex);
			Ort::ConstTensorTypeAndShapeInfo CurrentTensorInfo = CurrentTypeInfo.GetTensorTypeAndShapeInfo();
			const ONNXTensorElementDataType ONNXTensorElementDataTypeEnum = CurrentTensorInfo.GetElementType();
			CurrentTypeInfo.release();

			TensorsORTType.Emplace(ONNXTensorElementDataTypeEnum);

			TypeInfoORT TypeInfo = TranslateTensorTypeORTToNNE(ONNXTensorElementDataTypeEnum);

			TArray<int32> ShapeData;
			ShapeData.Reserve(CurrentTensorInfo.GetShape().size());
			for (const int64_t CurrentTensorSize : CurrentTensorInfo.GetShape())
			{
				ShapeData.Add((int32)CurrentTensorSize);
			}

			NNE::FSymbolicTensorShape Shape = NNE::FSymbolicTensorShape::Make(ShapeData);
			NNE::FTensorDesc SymbolicTensorDesc = NNE::FTensorDesc::Make(FString(CurTensorNamePtr), Shape, TypeInfo.DataType);

			check(SymbolicTensorDesc.GetElementByteSize() == TypeInfo.ElementSize);
			SymbolicTensorDescs.Emplace(SymbolicTensorDesc);
		}

		return true;
	}
	
	FModelInstanceCPU::ESetInputTensorShapeStatus FModelInstanceCPU::SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes)
	{
		InputTensors.Reset();
		OutputTensors.Reset();
		OutputTensorShapes.Reset();

		// Verify input shape are valid for the model and set InputTensorShapes
		if (ESetInputTensorShapeStatus Status = FModelInstanceBase<IModelInstanceCPU>::SetInputTensorShapes(InInputShapes); Status != ESetInputTensorShapeStatus::Ok)
		{
			return Status;
		}

		// Setup concrete input tensor
		for (int32 i = 0; i < InputSymbolicTensors.Num(); ++i)
		{
			NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::Make(InputSymbolicTensors[i].GetName(), InInputShapes[i], InputSymbolicTensors[i].GetDataType());
			InputTensors.Emplace(Tensor);
		}

		// Here model optimization could be done now that we know the input shapes, for some models
		// this would allow to resolve output shapes here rather than during inference.

		// Setup concrete output shapes only if all model output shapes are concretes, otherwise it will be set during Run()
		for (NNE::FTensorDesc SymbolicTensorDesc : OutputSymbolicTensors)
		{
			if (SymbolicTensorDesc.GetShape().IsConcrete())
			{
				NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc);
				OutputTensors.Emplace(Tensor);
				OutputTensorShapes.Emplace(Tensor.GetShape());
			}
		}
		if (OutputTensors.Num() != OutputSymbolicTensors.Num())
		{
			OutputTensors.Reset();
			OutputTensorShapes.Reset();
		}

		return ESetInputTensorShapeStatus::Ok;
	}

	FModelInstanceCPU::ERunSyncStatus FModelInstanceCPU::RunSync(TConstArrayView<NNE::FTensorBindingCPU> InInputBindings, TConstArrayView<NNE::FTensorBindingCPU> InOutputBindings)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FModelInstanceCPU_Run"), STAT_FModelInstanceCPU_Run, STATGROUP_NNE);

		// Sanity check
		if (!bIsLoaded)
		{
			UE_LOG(LogNNE, Warning, TEXT("FModelInstanceCPU::Run(): Call FModelInstanceCPU::Load() to load a model first."));
			return ERunSyncStatus::Fail;
		}

		// Verify the model inputs were prepared
		if (InputTensorShapes.Num() == 0)
		{
			UE_LOG(LogNNE, Error, TEXT("Run(): Input shapes are not set, please call SetInputTensorShapes."));
			return ERunSyncStatus::Fail;
		}

		NNEProfiling::Internal::FTimer RunTimer;
		RunTimer.Tic();

		if (!bHasRun)
		{
			bHasRun = true;
		}

#if WITH_EDITOR
		try
#endif //WITH_EDITOR
		{
			TArray<Ort::Value> InputOrtTensors;
			BindTensorsToORT(InInputBindings, InputTensors, InputTensorsORTType, AllocatorInfo.Get(), InputOrtTensors);

			if (!OutputTensors.IsEmpty())
			{
				// If output shapes are known we can directly map preallocated output buffers
				TArray<Ort::Value> OutputOrtTensors;
				BindTensorsToORT(InOutputBindings, OutputTensors, OutputTensorsORTType, AllocatorInfo.Get(), OutputOrtTensors);

				Session->Run(Ort::RunOptions{ nullptr },
					InputTensorNamePtrs.GetData(), &InputOrtTensors[0], InputTensorNamePtrs.Num(),
					OutputTensorNamePtrs.GetData(), &OutputOrtTensors[0], OutputTensorNamePtrs.Num());
			}
			else
			{
				TArray<Ort::Value> OutputOrtTensors;
				for (int32 i = 0; i < InOutputBindings.Num(); ++i)
				{
					OutputOrtTensors.Emplace(nullptr);
				}

				Session->Run(Ort::RunOptions{ nullptr },
					InputTensorNamePtrs.GetData(), &InputOrtTensors[0], InputTensorNamePtrs.Num(),
					OutputTensorNamePtrs.GetData(), &OutputOrtTensors[0], OutputTensorNamePtrs.Num());

				// Output shapes were resolved during inference: Copy the data back to bindings and expose output tensor shapes
				CopyFromORTToBindings(OutputOrtTensors, InOutputBindings, OutputSymbolicTensors, OutputTensors);
				check(OutputTensorShapes.IsEmpty());
				for (int32 i = 0; i < OutputTensors.Num(); ++i)
				{
					OutputTensorShapes.Emplace(OutputTensors[i].GetShape());
				}
			}
		}
#if WITH_EDITOR
		catch (const Ort::Exception& Exception)
		{
			UE_LOG(LogNNE, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
			return ERunSyncStatus::Fail;
		}
		catch (...)
		{
			UE_LOG(LogNNE, Error, TEXT("Unknown exception!"));
			return ERunSyncStatus::Fail;
		}
#endif //WITH_EDITOR

		RunStatisticsEstimator.StoreSample(RunTimer.Toc());

		return ERunSyncStatus::Ok;
	}

	float FModelInstanceCPU::GetLastRunTimeMSec() const
	{
		return RunStatisticsEstimator.GetLastSample();
	}

	NNEProfiling::Internal::FStatistics FModelInstanceCPU::GetRunStatistics() const
	{
		return RunStatisticsEstimator.GetStats();
	}

	NNEProfiling::Internal::FStatistics FModelInstanceCPU::GetInputMemoryTransferStats() const
	{
		return InputTransferStatisticsEstimator.GetStats();
	}

	void FModelInstanceCPU::ResetStats()
	{
		RunStatisticsEstimator.ResetStats();
		InputTransferStatisticsEstimator.ResetStats();
	}

	TSharedPtr<NNE::IModelInstanceCPU> FModelCPU::CreateModelInstanceCPU()
	{
		const NNERuntimeORTCpu::Private::FRuntimeConf InConf;
		NNERuntimeORTCpu::Private::FModelInstanceCPU* ModelInstance = new NNERuntimeORTCpu::Private::FModelInstanceCPU(ORTEnvironment, InConf);

		check(ModelData.IsValid());
		if (!ModelInstance->Init(ModelData->GetView()))
		{
			delete ModelInstance;
			return TSharedPtr<NNE::IModelInstanceCPU>();
		}

		NNE::IModelInstanceCPU* IModelInstance = static_cast<NNE::IModelInstanceCPU*>(ModelInstance);
		return TSharedPtr<NNE::IModelInstanceCPU>(IModelInstance);
	}

	FModelCPU::FModelCPU(Ort::Env* InORTEnvironment, const TSharedPtr<UE::NNE::FSharedModelData>& InModelData) :
		ORTEnvironment(InORTEnvironment),
		ModelData(InModelData)
	{

	}

} // namespace UE::NNERuntimeORTCpu::Private