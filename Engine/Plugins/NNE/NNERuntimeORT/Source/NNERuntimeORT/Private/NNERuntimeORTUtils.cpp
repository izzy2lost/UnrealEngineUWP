// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTUtils.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NNE.h"
#include "NNERuntimeORTEnv.h"

namespace UE::NNERuntimeORT::Private
{
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

bool OptimizeModel(TSharedRef<FEnvironment> InEnvironment, FNNEModelRaw& Model, ENNEInferenceFormat TargetFormat)
{
	SCOPED_NAMED_EVENT_TEXT("OrtHelper::OptimizeModel", FColor::Magenta);

	if (Model.Format != ENNEInferenceFormat::ONNX)
	{
		UE_LOG(LogNNE, Warning, TEXT("NNERuntimeORT: ONNX Runtime Model Optimizer is expecting a model in ONNX format but received %u."), Model.Format);
		return false;
	}

	FString ProjIntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir());
	FString ModelToOptimizePath = FPaths::CreateTempFilename(*ProjIntermediateDir, TEXT("ORTOptimizerPass_ToOptimize"), TEXT(".onnx"));
	FString TargetExtension = TargetFormat == ENNEInferenceFormat::ONNX ? TEXT(".onnx") : TEXT(".ort");
	FString ModelOptimizedPath = FPaths::CreateTempFilename(*ProjIntermediateDir, TEXT("ORTOptimizerPass_Optimized"), *TargetExtension);

	//See https://onnxruntime.ai/docs/performance/model-optimizations/graph-optimizations.html
	//We only enable all the optimization when going to ORT format itself for the CPU provider
	GraphOptimizationLevel OptimizationLevel = TargetFormat == ENNEInferenceFormat::ONNX ? ORT_ENABLE_BASIC : ORT_ENABLE_ALL;

	FFileHelper::SaveArrayToFile(Model.Data, *ModelToOptimizePath);

#if WITH_EDITOR
	try
#endif // WITH_EDITOR
	{
		Ort::SessionOptions SessionOptions;
		if (ensureMsgf(InEnvironment->GetConfig().bUseGlobalThreadPool, TEXT("For Model Optimizer use ONNX Runtime global threadpool to improve performance!")))
		{
			SessionOptions.DisablePerSessionThreads();
		}
		else
		{
			SessionOptions.SetIntraOpNumThreads(InEnvironment->GetConfig().IntraOpNumThreads);
			SessionOptions.SetInterOpNumThreads(InEnvironment->GetConfig().InterOpNumThreads);
		}
		SessionOptions.SetGraphOptimizationLevel(OptimizationLevel);
#if PLATFORM_WINDOWS
		SessionOptions.SetOptimizedModelFilePath(*ModelOptimizedPath);

		Ort::Session Session(InEnvironment->GetOrtEnv(), *ModelToOptimizePath, SessionOptions);
#else
		SessionOptions.SetOptimizedModelFilePath(TCHAR_TO_ANSI(*ModelOptimizedPath));
		
		Ort::Session Session(InEnvironment->GetOrtEnv(), TCHAR_TO_ANSI(*ModelToOptimizePath), SessionOptions);
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

	IFileManager::Get().Delete(*ModelToOptimizePath);
	IFileManager::Get().Delete(*ModelOptimizedPath);

	Model.Format = TargetFormat;

	return true;
}

} // OrtHelper

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