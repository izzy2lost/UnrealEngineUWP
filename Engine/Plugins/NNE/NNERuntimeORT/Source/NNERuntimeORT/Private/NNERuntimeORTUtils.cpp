// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NNE.h"
#include "NNERuntimeORTEnv.h"

#if PLATFORM_WINDOWS
#include <dxgi1_4.h>
#include "ID3D12DynamicRHI.h"
#endif // PLATFORM_WINDOWS

// DirectML is implemented using COM on all platforms
#ifdef IID_GRAPHICS_PPV_ARGS
#define DML_PPV_ARGS(x) __uuidof(*x), IID_PPV_ARGS_Helper(x)
#else
#define DML_PPV_ARGS(x) IID_PPV_ARGS(x)
#endif

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
// Check for DirectX 12-compatible hardware.
// Use DXGI to enumerate adapters and try to create a d3d12 device using the default adapter (will create dependency to dxgi.dll!)
// DXGI 1.6 should be available since Windows 10, version 1809, which is newer than the minimum SDK version
// specified in Engine\Config\Windows\Windows_SDK.json at the moment.
bool IsD3D12Available()
{
#if PLATFORM_WINDOWS
	using Microsoft::WRL::ComPtr;

	const int32 DeviceIndex = 0;

	ComPtr<IDXGIFactory4> Factory;
	CreateDXGIFactory2(0, IID_PPV_ARGS(&Factory));
	if (!Factory)
	{
		return false;
	}

	ComPtr<IDXGIAdapter1> Adapter;
	Factory->EnumAdapters1(DeviceIndex, &Adapter);
	if (!Adapter)
	{
		return false;
	}

	ComPtr<ID3D12Device> Device;
	D3D12CreateDevice(Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&Device));
	if (!Device)
	{
		return false;
	}

	return true;
#else
	return false;
#endif // PLATFORM_WINDOWS
}

// For more details about ORT graph optimization checkout
// https://onnxruntime.ai/docs/performance/model-optimizations/graph-optimizations.html

struct FGraphOptimizationLevels
{
	GraphOptimizationLevel Cooking;
	GraphOptimizationLevel Offline;
	GraphOptimizationLevel Online;
};

// CPU
static constexpr FGraphOptimizationLevels OrtCpuOptimizationLevels
{
	.Cooking = GraphOptimizationLevel::ORT_ENABLE_EXTENDED,
	.Offline = GraphOptimizationLevel::ORT_DISABLE_ALL,
	.Online = GraphOptimizationLevel::ORT_ENABLE_ALL
};

// DirectML EP
// note: optimize with DirectML EP enabled, but currently an offline optimized model can not be optimized again (only DML)!
// Therefore, if one enables offline optimization, set it to ORT_ENABLE_ALL and disable any optimization in online mode (ORT_DISABLE_ALL).
//
// note: since cooked models contain only basic graph optimizations, we need full optimization in online mode.
// Therefore, offline optimization in non-Editor can not be turned on.
static constexpr FGraphOptimizationLevels OrtDmlOptimizationLevels
{
	.Cooking = GraphOptimizationLevel::ORT_ENABLE_BASIC,
	.Offline = GraphOptimizationLevel::ORT_DISABLE_ALL,
	.Online = GraphOptimizationLevel::ORT_ENABLE_ALL
};

GraphOptimizationLevel GetGraphOptimizationLevel(const FGraphOptimizationLevels &OptimizationLevels, bool bIsOnline, bool bIsCooking)
{
	if (bIsOnline)
	{
		return OptimizationLevels.Online;
	}
	else
	{
		if (bIsCooking)
		{
			return OptimizationLevels.Cooking;
		}
		else
		{
			return OptimizationLevels.Offline;
		}
	}
}

namespace OrtHelper
{
TArray<uint32> GetShape(const Ort::Value& OrtTensor)
{
	OrtTensorTypeAndShapeInfo* TypeAndShapeInfoPtr = nullptr;
	size_t DimensionsCount = 0;

	Ort::ThrowOnError(Ort::GetApi().GetTensorTypeAndShape(OrtTensor, &TypeAndShapeInfoPtr));
	Ort::ThrowOnError(Ort::GetApi().GetDimensionsCount(TypeAndShapeInfoPtr, &DimensionsCount));

	TArray<int64_t> OrtShape;

	OrtShape.SetNumUninitialized(DimensionsCount);
	Ort::ThrowOnError(Ort::GetApi().GetDimensions(TypeAndShapeInfoPtr, OrtShape.GetData(), OrtShape.Num()));
	Ort::GetApi().ReleaseTensorTypeAndShapeInfo(TypeAndShapeInfoPtr);

	TArray<uint32> Result;

	Algo::Transform(OrtShape, Result, [](int64_t Value)
	{
		check(Value >= 0);
		return (uint32)Value;
	});

	return Result;
}
} // namespace OrtHelper

GraphOptimizationLevel GetGraphOptimizationLevelForCPU(bool bIsOnline, bool bIsCooking)
{
	return GetGraphOptimizationLevel(OrtCpuOptimizationLevels, bIsOnline, bIsCooking);
}

GraphOptimizationLevel GetGraphOptimizationLevelForDML(bool bIsOnline, bool bIsCooking)
{
	return GetGraphOptimizationLevel(OrtDmlOptimizationLevels, bIsOnline, bIsCooking);
}

TUniquePtr<Ort::SessionOptions> CreateSessionOptionsDefault(const TSharedRef<FEnvironment> &Environment)
{
	const FEnvironment::FConfig Config = Environment->GetConfig();

	TUniquePtr<Ort::SessionOptions> SessionOptions = MakeUnique<Ort::SessionOptions>();

	// Configure Threading
	if (Config.bUseGlobalThreadPool)
	{
		SessionOptions->DisablePerSessionThreads();
	}
	else
	{
		SessionOptions->SetIntraOpNumThreads(Config.IntraOpNumThreads);
		SessionOptions->SetInterOpNumThreads(Config.InterOpNumThreads);
	}

	// Configure Profiling
	// Note: can be called on game or render thread
	if (CVarNNERuntimeORTEnableProfiling.GetValueOnAnyThread())
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

TUniquePtr<Ort::SessionOptions> CreateSessionOptionsForDirectML(const TSharedRef<FEnvironment> &Environment, bool bRHID3D12Required)
{
#if PLATFORM_WINDOWS
	TUniquePtr<Ort::SessionOptions> SessionOptions = CreateSessionOptionsDefault(Environment);
	if (!SessionOptions.IsValid())
	{
		return {};
	}

	// Configure for DirectML
	SessionOptions->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
	SessionOptions->DisableMemPattern();

	if (!bRHID3D12Required && !IsRHID3D12())
	{
		const int32 DeviceIndex = 0;

		const OrtDmlApi* DmlApi = nullptr;
		Ort::ThrowOnError(Ort::GetApi().GetExecutionProviderApi("DML", ORT_API_VERSION, reinterpret_cast<const void**>(&DmlApi)));
		if (!DmlApi)
		{
			UE_LOG(LogNNE, Error, TEXT("Ort DirectML Api not available!"));
			return {};
		}

		OrtStatusPtr Status = DmlApi->SessionOptionsAppendExecutionProvider_DML(*SessionOptions.Get(), DeviceIndex);
		if (Status)
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to add DirectML execution provider to OnnxRuntime session options: %s"), ANSI_TO_TCHAR(Ort::GetApi().GetErrorMessage(Status)));
			return {};
		}

		return SessionOptions;
	}

	if (!GDynamicRHI)
	{
		UE_LOG(LogNNE, Error, TEXT("Error:No RHI found, could not initialize"));
		return {};
	}

	// In order to use DirectML we need D3D12
	ID3D12DynamicRHI* RHI = nullptr;
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

	OrtStatusPtr Status = DmlApi->SessionOptionsAppendExecutionProvider_DML1(*SessionOptions, DmlDevice, CmdQ);

	if (Status)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to add DirectML execution provider to OnnxRuntime session options: %s"), ANSI_TO_TCHAR(Ort::GetApi().GetErrorMessage(Status)));
		return {};
	}

	return SessionOptions;
#else
	return {};
#endif // PLATFORM_WINDOWS
}

bool OptimizeModel(const TSharedRef<FEnvironment> &Environment, Ort::SessionOptions &SessionOptions, ENNEInferenceFormat TargetFormat, FNNEModelRaw& Model)
{
	SCOPED_NAMED_EVENT_TEXT("OrtHelper::OptimizeModel", FColor::Magenta);

	if (Model.Format != ENNEInferenceFormat::ONNX)
	{
		UE_LOG(LogNNE, Warning, TEXT("NNERuntimeORT: ONNX Runtime Model Optimizer is expecting a model in ONNX format but received %u."), Model.Format);
		return false;
	}

	FString ProjIntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir());
	FString TargetExtension = TargetFormat == ENNEInferenceFormat::ONNX ? TEXT(".onnx") : TEXT(".ort");
	FString ModelOptimizedPath = FPaths::CreateTempFilename(*ProjIntermediateDir, TEXT("ORTOptimizerPass_Optimized"), *TargetExtension);

#if WITH_EDITOR
	try
#endif // WITH_EDITOR
	{
#if PLATFORM_WINDOWS
		SessionOptions.SetOptimizedModelFilePath(*ModelOptimizedPath);

		Ort::Session Session(Environment->GetOrtEnv(), Model.Data.GetData(), Model.Data.Num(), SessionOptions);
#else
		SessionOptions.SetOptimizedModelFilePath(TCHAR_TO_ANSI(*ModelOptimizedPath));

		Ort::Session Session(Environment->GetOrtEnv(), Model.Data.GetData(), Model.Data.Num(), SessionOptions);
#endif
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

	FFileHelper::LoadFileToArray(Model.Data, *ModelOptimizedPath);

	IFileManager::Get().Delete(*ModelOptimizedPath);

	Model.Format = TargetFormat;

	return true;
}

TypeInfoORT TranslateTensorTypeORTToNNE(ONNXTensorElementDataType OrtDataType)
{
	ENNETensorDataType DataType = ENNETensorDataType::None;
	uint64 ElementSize = 0;

	switch (OrtDataType) {
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED:
		DataType = ENNETensorDataType::None;
		ElementSize = 0;
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
		DataType = ENNETensorDataType::Float;
		ElementSize = sizeof(float);
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
		DataType = ENNETensorDataType::UInt8;
		ElementSize = sizeof(uint8);
		break;
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
		DataType = ENNETensorDataType::Int8;
		ElementSize = sizeof(int8);
		break;
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
		DataType = ENNETensorDataType::UInt16;
		ElementSize = sizeof(uint16);
		break;
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
		DataType = ENNETensorDataType::Int16;
		ElementSize = sizeof(int16);
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
		DataType = ENNETensorDataType::Int32;
		ElementSize = sizeof(int32);
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
		DataType = ENNETensorDataType::Int64;
		ElementSize = sizeof(int64);
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING:
		DataType = ENNETensorDataType::Char;
		ElementSize = sizeof(char);
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
		DataType = ENNETensorDataType::Boolean;
		ElementSize = sizeof(bool);
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
		DataType = ENNETensorDataType::Half;
		ElementSize = 2;
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
		DataType = ENNETensorDataType::Double;
		ElementSize = sizeof(double);
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
		DataType = ENNETensorDataType::UInt32;
		ElementSize = sizeof(uint32);
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
		DataType = ENNETensorDataType::UInt64;
		ElementSize = sizeof(uint64);
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64:
		DataType = ENNETensorDataType::Complex64;
		ElementSize = 8;
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128:
		DataType = ENNETensorDataType::Complex128;
		ElementSize = 16;
		break;

	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
		DataType = ENNETensorDataType::BFloat16;
		ElementSize = 2;
		break;

	default:
		DataType = ENNETensorDataType::None;
		break;
	}

	return TypeInfoORT{ DataType, ElementSize };
}

} // namespace UE::NNERuntimeORT::Private