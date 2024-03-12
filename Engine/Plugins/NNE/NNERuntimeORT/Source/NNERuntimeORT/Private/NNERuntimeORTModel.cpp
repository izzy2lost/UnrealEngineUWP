// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTModel.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "NNERuntimeORT.h"
#include "NNERuntimeORTUtils.h"
#include "RenderGraphUtils.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <unknwn.h>
#include "Microsoft/COMPointer.h"
#include "DirectML.h"
#include "dml_provider_factory.h"
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#include "ID3D12DynamicRHI.h"
#endif // PLATFORM_WINDOWS

THIRD_PARTY_INCLUDES_START
#include "cpu_provider_factory.h"
THIRD_PARTY_INCLUDES_END

// DirectML is implemented using COM on all platforms
#ifdef IID_GRAPHICS_PPV_ARGS
#define DML_PPV_ARGS(x) __uuidof(*x), IID_PPV_ARGS_Helper(x)
#else
#define DML_PPV_ARGS(x) IID_PPV_ARGS(x)
#endif

BEGIN_SHADER_PARAMETER_STRUCT(FORTModelInstanceRDGParameters, )
	RDG_BUFFER_ACCESS_ARRAY(InputBuffers)
	RDG_BUFFER_ACCESS_ARRAY(OutputBuffers)
END_SHADER_PARAMETER_STRUCT()

static int32 ORTProfilingSessionNumber = 0;
static TAutoConsoleVariable<bool> CVarNNERuntimeORTEnableProfiling(
	TEXT("nne.ort.enableprofiling"),
	false,
	TEXT("True if NNERuntimeORT plugin should create ORT sessions with profiling enabled.\n")
	TEXT("When profiling is enabled ORT will create standard performance tracing json files next to the editor executable.\n")
	TEXT("The files will be prefixed by 'NNERuntimeORTProfile_' and can be loaded for example using chrome://tracing.\n")
	TEXT("More information can be found at https://onnxruntime.ai/docs/performance/tune-performance/profiling-tools.html\n"),
	ECVF_Default);

namespace UE::NNERuntimeORT::Private
{

namespace Detail
{

TUniquePtr<Ort::SessionOptions> CreateSessionOptionsDefault(const FRuntimeConf &RuntimeConf)
{
	TUniquePtr<Ort::SessionOptions> SessionOptions = MakeUnique<Ort::SessionOptions>();
	SessionOptions->SetGraphOptimizationLevel(RuntimeConf.OptimizationLevel);

	if (CVarNNERuntimeORTEnableProfiling.GetValueOnGameThread())
	{
		FString ProfilingFilePrefix("NNERuntimeORTProfile_");
		ProfilingFilePrefix += FString::FromInt(ORTProfilingSessionNumber);
		++ORTProfilingSessionNumber;
		#if PLATFORM_WINDOWS
			SessionOptions->EnableProfiling(*ProfilingFilePrefix);
		#else
			SessionOptions->EnableProfiling(TCHAR_TO_ANSI(*ProfilingFilePrefix));
		#endif
	}

	return SessionOptions;
}

#if PLATFORM_WINDOWS
TUniquePtr<Ort::SessionOptions> CreateSessionOptionsForDirectML(const FRuntimeConf &RuntimeConf)
{
	TUniquePtr<Ort::SessionOptions> SessionOptions = CreateSessionOptionsDefault(RuntimeConf);
	SessionOptions->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
	SessionOptions->DisableMemPattern();

	// In order to use DirectML we need D3D12
	ID3D12DynamicRHI* RHI = nullptr;

	if (!GDynamicRHI)
	{
		UE_LOG(LogNNE, Error, TEXT("Error:No RHI found, could not initialize"));
		return {};
	}

	if (IsRHID3D12() )
	{
		RHI = GetID3D12DynamicRHI();
	}
	else
	{
		if (GDynamicRHI)
		{
			UE_LOG(LogNNE, Error, TEXT("Error:%s RHI is not supported by DirectML, please use D3D12."), GDynamicRHI->GetName());
			return {};
		}
		else
		{
			UE_LOG(LogNNE, Error, TEXT("Error:No RHI found"));
			return {};
		}
	}

	check(RHI);

	const int32 DeviceIndex = 0;
	ID3D12Device* D3D12Device = RHI->RHIGetDevice(DeviceIndex);

	if (!D3D12Device)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to get D3D12 Device from RHI for device index %d"), DeviceIndex);
		return {};
	}

	DML_CREATE_DEVICE_FLAGS DmlCreateFlags = DML_CREATE_DEVICE_FLAG_NONE;

	// Set debugging flags
	if (GRHIGlobals.IsDebugLayerEnabled)
	{
		DmlCreateFlags |= DML_CREATE_DEVICE_FLAG_DEBUG;
	}

	IDMLDevice* DmlDevice = nullptr;
	HRESULT Res = DMLCreateDevice(D3D12Device, DmlCreateFlags, DML_PPV_ARGS(&DmlDevice));

	if (FAILED(Res) || !DmlDevice)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to create DirectML device, DMLCreateDevice error code :%x"), Res);
		return {};
	}

	ID3D12CommandQueue* CmdQ = RHI->RHIGetCommandQueue();

	const OrtDmlApi* DmlApi = nullptr;
	Ort::ThrowOnError(Ort::GetApi().GetExecutionProviderApi("DML", ORT_API_VERSION, reinterpret_cast<const void**>(&DmlApi)));

	if (!DmlApi)
	{
		UE_LOG(LogNNE, Error, TEXT("Ort DirectML Api not available!"));
		return {};
	}

	OrtStatusPtr Status = DmlApi->SessionOptionsAppendExecutionProvider_DML1(*SessionOptions.Get(), DmlDevice, CmdQ);

	if (Status)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to add DirectML execution provider to OnnxRuntime session options: %s"), ANSI_TO_TCHAR(Ort::GetApi().GetErrorMessage(Status)));
		return {};
	}

	return SessionOptions;
}
#endif // PLATFORM_WINDOWS

} // namespace Detail

template <class ModelInterface, class TensorBinding> 
FModelInstanceORTBase<ModelInterface, TensorBinding>::FModelInstanceORTBase(const FRuntimeConf& InRuntimeConf, TSharedPtr<Ort::Env> InEnvironment)
	: RuntimeConf(InRuntimeConf), Environment(InEnvironment)
{

}

template <class ModelInterface, class TensorBinding> 
bool FModelInstanceORTBase<ModelInterface, TensorBinding>::Init(TConstArrayView<uint8> ModelData)
{
	constexpr int32 GuidSize = sizeof(UNNERuntimeORTDml::GUID);
	constexpr int32 VersionSize = sizeof(UNNERuntimeORTDml::Version);
	TConstArrayView<uint8> ModelBuffer = TConstArrayView<uint8>(&(ModelData.GetData()[GuidSize + VersionSize]), ModelData.Num() - GuidSize - VersionSize);

	if (ModelBuffer.Num() == 0)
	{
		UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTBase::Init(): Input model data is empty."));
		return false;
	}

#if WITH_EDITOR
	try
#endif // WITH_EDITOR
	{
		if (!InitializedAndConfigureMembers())
		{
			UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTBase::Init(): InitializedAndConfigureMembers failed."));
			return false;
		}

		Session = MakeUnique<Ort::Session>(*Environment, ModelBuffer.GetData(), ModelBuffer.Num(), *SessionOptions);

		if (!ConfigureTensors(true))
		{
			UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTBase::Init(): Failed to configure Inputs tensors."));
			return false;
		}
		if (!ConfigureTensors(false))
		{
			UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTBase::Init(): Failed to configure Outputs tensors."));
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
#endif // WITH_EDITOR

	return true;
}

template <class ModelInterface, class TensorBinding> 
bool FModelInstanceORTBase<ModelInterface, TensorBinding>::InitializedAndConfigureMembers()
{
	Allocator = MakeUnique<Ort::AllocatorWithDefaultOptions>();
	AllocatorInfo = MakeUnique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU));

	return true;
}

template <class ModelInterface, class TensorBinding>
bool FModelInstanceORTBase<ModelInterface, TensorBinding>::ConfigureTensors(bool bAreTensorInputs)
{
	const uint32 NumberTensors							= bAreTensorInputs ? Session->GetInputCount() : Session->GetOutputCount();
	TArray<NNE::FTensorDesc>& SymbolicTensorDescs		= bAreTensorInputs ? NNE::Internal::FModelInstanceBase<ModelInterface>::InputSymbolicTensors : NNE::Internal::FModelInstanceBase<ModelInterface>::OutputSymbolicTensors;
	TArray<ONNXTensorElementDataType>& TensorsORTType	= bAreTensorInputs ? InputTensorsORTType	: OutputTensorsORTType;
	TArray<char*>& TensorNames							= bAreTensorInputs ? InputTensorNames		: OutputTensorNames;
	TArray<Ort::AllocatedStringPtr>& TensorNameValues	= bAreTensorInputs ? InputTensorNameValues	: OutputTensorNameValues;

	SymbolicTensorDescs.Reset();

	for (uint32 TensorIndex = 0; TensorIndex < NumberTensors; ++TensorIndex)
	{
		// Get Tensor name
		Ort::AllocatedStringPtr CurTensorName = bAreTensorInputs ? Session->GetInputNameAllocated(TensorIndex, *Allocator) : Session->GetOutputNameAllocated(TensorIndex, *Allocator);
		TensorNameValues.Emplace(MoveTemp(CurTensorName));
		TensorNames.Emplace(TensorNameValues.Last().get());

		// Get node type
		const Ort::TypeInfo CurrentTypeInfo = bAreTensorInputs ? Session->GetInputTypeInfo(TensorIndex) : Session->GetOutputTypeInfo(TensorIndex);
		const Ort::ConstTensorTypeAndShapeInfo CurrentTensorInfo = CurrentTypeInfo.GetTensorTypeAndShapeInfo();
		const ONNXTensorElementDataType ONNXTensorElementDataTypeEnum = CurrentTensorInfo.GetElementType();
		const TypeInfoORT TypeInfo = TranslateTensorTypeORTToNNE(ONNXTensorElementDataTypeEnum);

		TensorsORTType.Emplace(ONNXTensorElementDataTypeEnum);

		// Get shape
		TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> ShapeData;
		ShapeData.Reserve(CurrentTensorInfo.GetShape().size());
		for (int64 CurrentTensorSize : CurrentTensorInfo.GetShape())
		{
			ShapeData.Add((int32)CurrentTensorSize);
		}

		const NNE::FSymbolicTensorShape Shape = NNE::FSymbolicTensorShape::Make(ShapeData);
		const NNE::FTensorDesc SymbolicTensorDesc = NNE::FTensorDesc::Make(FString(TensorNames.Last()), Shape, TypeInfo.DataType);

		check(SymbolicTensorDesc.GetElementByteSize() == TypeInfo.ElementSize);
		SymbolicTensorDescs.Emplace(SymbolicTensorDesc);
	}

	return true;
}

template <class ModelInterface, class TensorBinding> 
typename ModelInterface::ESetInputTensorShapesStatus FModelInstanceORTBase<ModelInterface, TensorBinding>::SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes)
{
	using ModelInstanceBase = NNE::Internal::FModelInstanceBase<ModelInterface>;

	InputTensors.Reset();
	OutputTensors.Reset();
	ModelInstanceBase::OutputTensorShapes.Reset();

	// Verify input shape are valid for the model and set InputTensorShapes
	if (typename ModelInterface::ESetInputTensorShapesStatus Status = ModelInstanceBase::SetInputTensorShapes(InInputShapes); Status != ModelInterface::ESetInputTensorShapesStatus::Ok)
	{
		return Status;
	}

	// Setup concrete input tensor
	for (int32 i = 0; i < ModelInstanceBase::InputSymbolicTensors.Num(); ++i)
	{
		NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::Make(ModelInstanceBase::InputSymbolicTensors[i].GetName(), InInputShapes[i], ModelInstanceBase::InputSymbolicTensors[i].GetDataType());
		InputTensors.Emplace(Tensor);
	}

	// Setup concrete output shapes only if all model output shapes are concretes, otherwise it will be set during Run()
	for (NNE::FTensorDesc SymbolicTensorDesc : ModelInstanceBase::OutputSymbolicTensors)
	{
		if (SymbolicTensorDesc.GetShape().IsConcrete())
		{
			NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc);
			OutputTensors.Emplace(Tensor);
			ModelInstanceBase::OutputTensorShapes.Emplace(Tensor.GetShape());
		}
	}
	if (OutputTensors.Num() != ModelInstanceBase::OutputSymbolicTensors.Num())
	{
		OutputTensors.Reset();
		ModelInstanceBase::OutputTensorShapes.Reset();
	}

	return ModelInterface::ESetInputTensorShapesStatus::Ok;
}

template <class ModelInterface, class TensorBinding>
typename ModelInterface::ERunSyncStatus FModelInstanceORTBase<ModelInterface, TensorBinding>::RunSync(TConstArrayView<TensorBinding> InInputBindings, TConstArrayView<TensorBinding> InOutputBindings)
{
	checkf(Session.IsValid(), TEXT("FModelInstanceORT::RunSync(): Called without a Session, FModelInstanceORT::Init() should have been called."));

	SCOPED_NAMED_EVENT_TEXT("FModelInstanceORTBase::RunSync", FColor::Magenta);

	// Verify the model inputs were prepared
	if (NNE::Internal::FModelInstanceBase<ModelInterface>::InputTensorShapes.Num() == 0)
	{
		UE_LOG(LogNNE, Error, TEXT("RunSync(): Input shapes are not set, please call SetInputTensorShapes."));
		return ModelInterface::ERunSyncStatus::Fail;
	}

#if WITH_EDITOR
	try
#endif // WITH_EDITOR
	{
		TArray<Ort::Value> InputOrtTensors;
		BindTensorsToORT(InInputBindings, InputTensors, InputTensorsORTType, *AllocatorInfo, InputOrtTensors);

		if (!OutputTensors.IsEmpty())
		{
			// If output shapes are known we can directly map preallocated output buffers
			TArray<Ort::Value> OutputOrtTensors;
			BindTensorsToORT(InOutputBindings, OutputTensors, OutputTensorsORTType, *AllocatorInfo, OutputOrtTensors);

			Session->Run(Ort::RunOptions{ nullptr },
				InputTensorNames.GetData(), &InputOrtTensors[0], InputTensorNames.Num(),
				OutputTensorNames.GetData(), &OutputOrtTensors[0], OutputTensorNames.Num());
		}
		else
		{
			TArray<Ort::Value> OutputOrtTensors;
			for (int32 i = 0; i < InOutputBindings.Num(); ++i)
			{
				OutputOrtTensors.Emplace(nullptr);
			}

			Session->Run(Ort::RunOptions{ nullptr },
				InputTensorNames.GetData(), &InputOrtTensors[0], InputTensorNames.Num(),
				OutputTensorNames.GetData(), &OutputOrtTensors[0], OutputTensorNames.Num());

			// Output shapes were resolved during inference: Copy the data back to bindings and expose output tensor shapes
			CopyFromORTToBindings(OutputOrtTensors, InOutputBindings, NNE::Internal::FModelInstanceBase<ModelInterface>::OutputSymbolicTensors, OutputTensors);
			check(NNE::Internal::FModelInstanceBase<ModelInterface>::OutputTensorShapes.IsEmpty());
			for (int32 i = 0; i < OutputTensors.Num(); ++i)
			{
				NNE::Internal::FModelInstanceBase<ModelInterface>::OutputTensorShapes.Emplace(OutputTensors[i].GetShape());
			}
		}
	}
#if WITH_EDITOR
	catch (const Ort::Exception& Exception)
	{
		UE_LOG(LogNNE, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
		return ModelInterface::ERunSyncStatus::Fail;
	}
	catch (...)
	{
		UE_LOG(LogNNE, Error, TEXT("Unknown exception!"));
		return ModelInterface::ERunSyncStatus::Fail;
	}
#endif // WITH_EDITOR

	return ModelInterface::ERunSyncStatus::Ok;
}

#if WITH_EDITOR
TSharedPtr<NNE::IModelInstanceCPU> FModelORTCpu::CreateModelInstanceCPU()
{
	const FRuntimeConf Conf;
	FModelInstanceORTCpu* ModelInstance = new FModelInstanceORTCpu(Conf, Environment);

	check(ModelData.IsValid());
	if (!ModelInstance->Init(ModelData->GetView()))
	{
		delete ModelInstance;
		return TSharedPtr<UE::NNE::IModelInstanceCPU>();
	}

	NNE::IModelInstanceCPU* IModelInstance = static_cast<NNE::IModelInstanceCPU*>(ModelInstance);
	return TSharedPtr<NNE::IModelInstanceCPU>(IModelInstance);
}

FModelORTCpu::FModelORTCpu(TSharedPtr<Ort::Env> InEnvironment, const TSharedPtr<UE::NNE::FSharedModelData>& InModelData) :
	Environment(InEnvironment), ModelData(InModelData)
{
}

bool FModelInstanceORTCpu::InitializedAndConfigureMembers()
{
	if (!FModelInstanceORTBase::InitializedAndConfigureMembers())
	{
		return false;
	}

	SessionOptions = Detail::CreateSessionOptionsDefault(RuntimeConf);
	if (!SessionOptions.IsValid())
	{
		return false;
	}

	SessionOptions->EnableCpuMemArena();

	return true;
}
#endif // WITH_EDITOR

#if PLATFORM_WINDOWS
TSharedPtr<NNE::IModelInstanceGPU> FModelORTDmlGPU::CreateModelInstanceGPU()
{
	const FRuntimeConf Conf;
	FModelInstanceORTDmlGPU* ModelInstance = new FModelInstanceORTDmlGPU(Conf, Environment);

	check(ModelData.IsValid());
	if (!ModelInstance->Init(ModelData->GetView()))
	{
		delete ModelInstance;
		return TSharedPtr<UE::NNE::IModelInstanceGPU>();
	}

	NNE::IModelInstanceGPU* IModelInstance = static_cast<NNE::IModelInstanceGPU*>(ModelInstance);
	return TSharedPtr<NNE::IModelInstanceGPU>(IModelInstance);
}

FModelORTDmlGPU::FModelORTDmlGPU(TSharedPtr<Ort::Env> InEnvironment, const TSharedPtr<UE::NNE::FSharedModelData>& InModelData) :
	Environment(InEnvironment), ModelData(InModelData)
{
}

bool FModelInstanceORTDmlGPU::InitializedAndConfigureMembers()
{
	if (!FModelInstanceORTBase::InitializedAndConfigureMembers())
	{
		return false;
	}

	SessionOptions = Detail::CreateSessionOptionsForDirectML(RuntimeConf);
	if (!SessionOptions.IsValid())
	{
		return false;
	}

	return true;
}

FModelORTDmlRDG::FModelORTDmlRDG(TSharedRef<Ort::Env> InEnvironment, TSharedRef<UE::NNE::FSharedModelData> InModelData) :
	Environment(InEnvironment), ModelData(InModelData)
{
}

TSharedPtr<NNE::IModelInstanceRDG> FModelORTDmlRDG::CreateModelInstanceRDG()
{
	TSharedPtr<FModelInstanceORTDmlRDG> ModelInstance = MakeShared<FModelInstanceORTDmlRDG>(ModelData, FRuntimeConf{}, Environment);
	if (!ModelInstance->Init())
	{
		return {};
	}

	return ModelInstance;
}

FModelInstanceORTDmlRDG::FModelInstanceORTDmlRDG(TSharedRef<UE::NNE::FSharedModelData> InModelData, const FRuntimeConf& InRuntimeConf, TSharedRef<Ort::Env> InEnvironment)
	:  ModelData(InModelData), RuntimeConf(InRuntimeConf), Environment(InEnvironment)
{}

bool FModelInstanceORTDmlRDG::Init()
{
	constexpr int32 GuidSize = sizeof(UNNERuntimeORTDml::GUID);
	constexpr int32 VersionSize = sizeof(UNNERuntimeORTDml::Version);
	TConstArrayView<uint8> ModelBuffer = TConstArrayView<uint8>(&(ModelData->GetView().GetData()[GuidSize + VersionSize]), ModelData->GetView().Num() - GuidSize - VersionSize);

	if (ModelBuffer.IsEmpty())
	{
		UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTDmlRDG::Init(): Input model data is empty."));
		return false;
	}

#if WITH_EDITOR
	try
#endif // WITH_EDITOR
	{
		Allocator = MakeUnique<Ort::AllocatorWithDefaultOptions>();

		SessionOptions = Detail::CreateSessionOptionsForDirectML(RuntimeConf);
		if (!SessionOptions.IsValid())
		{
			UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTDmlRDG::Init(): Failed to create session options."));
			return false;
		}

		Session = MakeUnique<Ort::Session>(*Environment, ModelBuffer.GetData(), ModelBuffer.Num(), *SessionOptions);

		if (!ConfigureTensors(*Session))
		{
			UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTDmlRDG::Init(): Failed to configure Inputs tensors."));
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
#endif // WITH_EDITOR

	return true;
}

bool FModelInstanceORTDmlRDG::ConfigureTensors(const Ort::Session& ActiveSession)
{
	if (!ConfigureTensors(ActiveSession, true))
	{
		UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTDmlRDG::ConfigureTensors(): Failed to configure Inputs tensors."));
		return false;
	}
	if (!ConfigureTensors(ActiveSession, false))
	{
		UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTDmlRDG::ConfigureTensors(): Failed to configure Outputs tensors."));
		return false;
	}

	return true;
}

bool FModelInstanceORTDmlRDG::ConfigureTensors(const Ort::Session& ActiveSession, bool bAreTensorInputs)
{
	const uint32 NumberTensors							= bAreTensorInputs ? ActiveSession.GetInputCount() : ActiveSession.GetOutputCount();
	TArray<NNE::FTensorDesc>& SymbolicTensorDescs		= bAreTensorInputs ? InputSymbolicTensors   : OutputSymbolicTensors;
	TArray<ONNXTensorElementDataType>& TensorsORTType	= bAreTensorInputs ? InputTensorsORTType	: OutputTensorsORTType;
	TArray<char*>& TensorNames							= bAreTensorInputs ? InputTensorNames		: OutputTensorNames;
	TArray<Ort::AllocatedStringPtr>& TensorNameValues	= bAreTensorInputs ? InputTensorNameValues	: OutputTensorNameValues;
	TArray<TArray<FString>>& SymbolicDimensionNames 	= bAreTensorInputs ? InputSymbolicDimensionNames : OutputSymbolicDimensionNames;

	SymbolicTensorDescs.Reset();
	SymbolicDimensionNames.SetNum(NumberTensors);

	for (uint32 TensorIndex = 0; TensorIndex < NumberTensors; ++TensorIndex)
	{
		// Get Tensor name
		Ort::AllocatedStringPtr CurTensorName = bAreTensorInputs ? ActiveSession.GetInputNameAllocated(TensorIndex, *Allocator) : ActiveSession.GetOutputNameAllocated(TensorIndex, *Allocator);
		TensorNameValues.Emplace(MoveTemp(CurTensorName));
		TensorNames.Emplace(TensorNameValues.Last().get());

		// Get node type
		const Ort::TypeInfo CurrentTypeInfo = bAreTensorInputs ? ActiveSession.GetInputTypeInfo(TensorIndex) : ActiveSession.GetOutputTypeInfo(TensorIndex);
		const Ort::ConstTensorTypeAndShapeInfo CurrentTensorInfo = CurrentTypeInfo.GetTensorTypeAndShapeInfo();
		const ONNXTensorElementDataType ONNXTensorElementDataTypeEnum = CurrentTensorInfo.GetElementType();
		const TypeInfoORT TypeInfo = TranslateTensorTypeORTToNNE(ONNXTensorElementDataTypeEnum);

		// Get dynamic shape dimension names
		TUniquePtr<const char*[]> SymbolidDimensionNames = MakeUnique<const char*[]>(CurrentTensorInfo.GetShape().size());
		CurrentTensorInfo.GetSymbolicDimensions(SymbolidDimensionNames.Get(), CurrentTensorInfo.GetShape().size());

		TArray<FString>& CurrentSymbolicDimensionNames = SymbolicDimensionNames[TensorIndex];
		CurrentSymbolicDimensionNames.SetNum(CurrentTensorInfo.GetShape().size());

		for (int32 i = 0; i < CurrentTensorInfo.GetShape().size(); i++)
		{
			CurrentSymbolicDimensionNames[i] = FString((SymbolidDimensionNames.Get())[i]);
		}

		TensorsORTType.Emplace(ONNXTensorElementDataTypeEnum);

		// Get shape
		TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> ShapeData;
		ShapeData.Reserve(CurrentTensorInfo.GetShape().size());
		for (int64 CurrentTensorSize : CurrentTensorInfo.GetShape())
		{
			ShapeData.Add((int32)CurrentTensorSize);
		}

		const NNE::FSymbolicTensorShape Shape = NNE::FSymbolicTensorShape::Make(ShapeData);
		const NNE::FTensorDesc SymbolicTensorDesc = NNE::FTensorDesc::Make(FString(TensorNames.Last()), Shape, TypeInfo.DataType);

		check(SymbolicTensorDesc.GetElementByteSize() == TypeInfo.ElementSize);
		SymbolicTensorDescs.Emplace(SymbolicTensorDesc);
	}

	return true;
}

FModelInstanceORTDmlRDG::ESetInputTensorShapesStatus FModelInstanceORTDmlRDG::SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes)
{
	InputTensors.Reset();
	OutputTensors.Reset();
	OutputTensorShapes.Reset();

	// Verify input shape are valid for the model and set InputTensorShapes
	if (ESetInputTensorShapesStatus Status = NNE::Internal::FModelInstanceBase<NNE::IModelInstanceRDG>::SetInputTensorShapes(InInputShapes); Status != ESetInputTensorShapesStatus::Ok)
	{
		return Status;
	}

	// Check whether all input tensor shapes are concrete
	bool bHasSymbolicInputShapes = false;
	for (int32 i = 0; i < InputSymbolicTensors.Num(); i++)
	{
		if (!InputSymbolicTensors[i].GetShape().IsConcrete())
		{
			bHasSymbolicInputShapes = true;

			break;
		}
	}

	if (!bHasSymbolicInputShapes)
	{
		for (int32 i = 0; i < InputSymbolicTensors.Num(); i++)
		{
			NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::Make(InputSymbolicTensors[i].GetName(), InInputShapes[i], InputSymbolicTensors[i].GetDataType());
			InputTensors.Emplace(Tensor);
		}

		// All output shapes need to be concrete now
		for (int32 i = 0; i < OutputSymbolicTensors.Num(); i++)
		{
			const NNE::FTensorDesc SymbolicTensorDesc = OutputSymbolicTensors[i];

			if (SymbolicTensorDesc.GetShape().IsConcrete())
			{
				NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc);
				OutputTensors.Emplace(Tensor);
				OutputTensorShapes.Emplace(Tensor.GetShape());
			}
			else
			{
				UE_LOG(LogNNE, Warning, TEXT("One or more output tensors contain free dimensions, but input tensors are all concrete!"));
				return ESetInputTensorShapesStatus::Fail;
			}
		}

		return ESetInputTensorShapesStatus::Ok;
	}

	// Recreate session options because potentially we add new free dimension overrides
	SessionOptions = Detail::CreateSessionOptionsForDirectML(RuntimeConf);
	if (!SessionOptions.IsValid())
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to recreate session options!"));
		return ESetInputTensorShapesStatus::Fail;
	}

	// Setup concrete input tensors
	for (int32 i = 0; i < InputSymbolicTensors.Num(); i++)
	{
		NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::Make(InputSymbolicTensors[i].GetName(), InInputShapes[i], InputSymbolicTensors[i].GetDataType());
		InputTensors.Emplace(Tensor);

		const NNE::FSymbolicTensorShape& SymbolicInputShape = InputSymbolicTensors[i].GetShape();

		// Override free dimensions of input tensors
		if (!SymbolicInputShape.IsConcrete())
		{
			check(InputTensorShapes[i].IsCompatibleWith(SymbolicInputShape));

			TConstArrayView<int32> InputSymbolicShapeData = SymbolicInputShape.GetData();
			TConstArrayView<uint32> InputShapeData = InputTensorShapes[i].GetData();

			for (int32 j = 0; j < InputShapeData.Num(); j++)
			{
				if (InputSymbolicShapeData[j] < 0)
				{
					Ort::GetApi().AddFreeDimensionOverrideByName(*SessionOptions, TCHAR_TO_ANSI(*InputSymbolicDimensionNames[i][j]), InputShapeData[j]);
				}
			}
		}
	}

	constexpr int32 GuidSize = sizeof(UNNERuntimeORTDml::GUID);
	constexpr int32 VersionSize = sizeof(UNNERuntimeORTDml::Version);
	TConstArrayView<uint8> ModelBuffer = TConstArrayView<uint8>(&(ModelData->GetView().GetData()[GuidSize + VersionSize]), ModelData->GetView().Num() - GuidSize - VersionSize);
	check(!ModelBuffer.IsEmpty());

	Session = MakeUnique<Ort::Session>(*Environment, ModelBuffer.GetData(), ModelBuffer.Num(), *SessionOptions);

	// Need to configure output tensors with new session (to apply free dimension overrides)
	if (!ConfigureTensors(*Session, false))
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to configure tensors!"));
		return ESetInputTensorShapesStatus::Fail;
	}

	// All output shapes need to be concrete now
	for (int32 i = 0; i < OutputSymbolicTensors.Num(); i++)
	{
		const NNE::FTensorDesc SymbolicTensorDesc = OutputSymbolicTensors[i];

		if (SymbolicTensorDesc.GetShape().IsConcrete())
		{
			NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc);
			OutputTensors.Emplace(Tensor);
			OutputTensorShapes.Emplace(Tensor.GetShape());
		}
		else
		{
			for (int32 j = 0; j < SymbolicTensorDesc.GetShape().Rank(); j++)
			{
				if (SymbolicTensorDesc.GetShape().GetData()[j] < 0)
				{
					UE_LOG(LogNNE, Warning, TEXT("Tensor '%hs' has free dimension '%s'."), OutputTensorNames[i], *OutputSymbolicDimensionNames[i][j]);
				}
			}

			UE_LOG(LogNNE, Error, TEXT("One or more output tensors contain free dimensions!"));
			return ESetInputTensorShapesStatus::Fail;
		}
	}

	return ESetInputTensorShapesStatus::Ok;
}

Ort::Value CreateTensor(const OrtDmlApi* DmlApi, const Ort::MemoryInfo& MemoryInfo, FRHIBuffer* Buffer, const NNE::Internal::FTensor& Tensor, ONNXTensorElementDataType ElementDataType,
	TArray<std::unique_ptr<void, void (*)(void*)>>& DmlAllocatorResources)
{
	ID3D12Resource* NativeD3D12Resource = GetID3D12DynamicRHI()->RHIGetResource(Buffer);

	void* DmlAllocatorResourcePtr;
	Ort::ThrowOnError(DmlApi->CreateGPUAllocationFromD3DResource(NativeD3D12Resource, &DmlAllocatorResourcePtr));

	std::unique_ptr<void, void (*)(void*)> DmlAllocatorResource(DmlAllocatorResourcePtr,
		[] (void* Ptr)
	{
		const OrtDmlApi* DmlApi;
		Ort::ThrowOnError(Ort::GetApi().GetExecutionProviderApi("DML", ORT_API_VERSION, reinterpret_cast<const void**>(&DmlApi)));

		DmlApi->FreeGPUAllocation(Ptr);
	});

	uint64 SizeInBytes = static_cast<uint64>(NativeD3D12Resource->GetDesc().Width);

	TUniquePtr<int64_t[]> Shape = MakeUnique<int64_t[]>(Tensor.GetShape().Rank());
	for (int32 i = 0; i < Tensor.GetShape().Rank(); ++i)
	{
		Shape.Get()[i] = Tensor.GetShape().GetData()[i];
	}
	const uint32 ShapeLen{ (uint32)Tensor.GetShape().Rank() };

	Ort::Value Result = Ort::Value::CreateTensor(MemoryInfo, DmlAllocatorResource.get(), SizeInBytes, Shape.Get(), ShapeLen, ElementDataType);

	DmlAllocatorResources.Add(MoveTemp(DmlAllocatorResource));

	return Result;
}

FModelInstanceORTDmlRDG::EEnqueueRDGStatus FModelInstanceORTDmlRDG::EnqueueRDG(FRDGBuilder& GraphBuilder, TConstArrayView<NNE::FTensorBindingRDG> Inputs, TConstArrayView<NNE::FTensorBindingRDG> Outputs)
{
	checkf(Session.IsValid(), TEXT("FModelInstanceORTDmlRDG::EnqueueRDG(): Called without a Session, FModelInstanceORTDmlRDG::Init() should have been called."));

	SCOPED_NAMED_EVENT_TEXT("FModelInstanceORTDmlRDG::EnqueueRDG", FColor::Magenta);

	// Verify the model inputs were prepared
	if (InputTensorShapes.Num() == 0)
	{
		UE_LOG(LogNNE, Error, TEXT("EnqueueRDG(): Input shapes are not set, please call SetInputTensorShapes."));
		return EEnqueueRDGStatus::Fail;
	}

	FORTModelInstanceRDGParameters* PassParameters = GraphBuilder.AllocParameters<FORTModelInstanceRDGParameters>();
	for (const NNE::FTensorBindingRDG& Binding : Inputs)
	{
		PassParameters->InputBuffers.Emplace(Binding.Buffer, ERHIAccess::CopySrc);
	}
	for (const NNE::FTensorBindingRDG& Binding : Outputs)
	{
		PassParameters->OutputBuffers.Emplace(Binding.Buffer, ERHIAccess::CopySrc);
	}

	GraphBuilder.AddPass(RDG_EVENT_NAME("FModelInstanceORTDmlRDG::EnqueueRDG.AddPass"), PassParameters, ERDGPassFlags::Readback,
	[this, PassParameters](FRHICommandListImmediate& RHICmdList)
	{
		SCOPED_NAMED_EVENT_TEXT("FModelInstanceORTDmlRDG::EnqueueRDG.AddPass", FColor::Magenta);

		TArray<FRHIBuffer*> InputBuffers;
		InputBuffers.SetNumUninitialized(PassParameters->InputBuffers.Num());
		for (int32 i = 0; i < PassParameters->InputBuffers.Num(); i++)
		{
			InputBuffers[i] = PassParameters->InputBuffers[i]->GetRHI();
		}
		TArray<FRHIBuffer*> OutputBuffers;
		OutputBuffers.SetNumUninitialized(PassParameters->OutputBuffers.Num());
		for (int32 i = 0; i < PassParameters->OutputBuffers.Num(); i++)
		{
			OutputBuffers[i] = PassParameters->OutputBuffers[i]->GetRHI();
		}

		// Submit previous work here to the GPU to avoid ORT Session Run() dispatching its work first
		RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

		RHICmdList.EnqueueLambda([this, InputBuffersCopy = InputBuffers, OutputBuffersCopy = OutputBuffers](FRHICommandListImmediate& RHICmdList)
		{
			GetID3D12PlatformDynamicRHI()->RHIRunOnQueue(ED3D12RHIRunOnQueueType::Graphics, [this, InputBuffersCopyCopy = InputBuffersCopy, OutputBuffersCopyCopy = OutputBuffersCopy](ID3D12CommandQueue* D3D12CommandQueue)
			{
#if WITH_EDITOR
				try
#endif // WITH_EDITOR
				{
					const OrtDmlApi* DmlApi;
					Ort::ThrowOnError(Ort::GetApi().GetExecutionProviderApi("DML", ORT_API_VERSION, reinterpret_cast<const void**>(&DmlApi)));
					
					Ort::MemoryInfo MemoryInfo("DML", OrtAllocatorType::OrtDeviceAllocator, 0, OrtMemType::OrtMemTypeDefault);
					
					TArray<std::unique_ptr<void, void (*)(void*)>> DmlAllocatorResources;
					TArray<Ort::Value> OrtInputTensors;
					TArray<Ort::Value> OrtOutputTensors;

					Ort::IoBinding IoBinding = Ort::IoBinding(*Session);

					for (int32 i = 0; i < InputBuffersCopyCopy.Num(); i++)
					{
						OrtInputTensors.Add(CreateTensor(DmlApi, MemoryInfo, InputBuffersCopyCopy[i], InputTensors[i], InputTensorsORTType[i], DmlAllocatorResources));

						IoBinding.BindInput(InputTensorNames[i], OrtInputTensors[i]);
					}
					for (int32 i = 0; i < OutputBuffersCopyCopy.Num(); i++)
					{
						OrtOutputTensors.Add(CreateTensor(DmlApi, MemoryInfo, OutputBuffersCopyCopy[i], OutputTensors[i], OutputTensorsORTType[i], DmlAllocatorResources));

						IoBinding.BindOutput(OutputTensorNames[i], OrtOutputTensors[i]);
					}

					// Don't use this sync, its CPU to GPU, but we need GPU to GPU
					// IoBinding.SynchronizeInputs();

					Session->Run({}, IoBinding);

					// // Don't use this sync, its CPU to GPU, but we need GPU to GPU
					// IoBinding.SynchronizeOutputs();
				}
#if WITH_EDITOR
				catch (const Ort::Exception& Exception)
				{
					UE_LOG(LogNNE, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
				}
				catch (...)
				{
					UE_LOG(LogNNE, Error, TEXT("Unknown exception!"));
				}
#endif // WITH_EDITOR
			}, false);
		});
	});

	return EEnqueueRDGStatus::Ok;
}
#endif //PLATFORM_WINDOWS
	
} // namespace UE::NNERuntimeORT::Private