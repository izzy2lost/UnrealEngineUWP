// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTUtils.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UE::NNERuntimeORT::Private
{
namespace OrtHelper
{

bool OptimizeModel(FNNEModelRaw& Model, ENNEInferenceFormat TargetFormat)
{
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

	double ONNXModelOptimisationStartTime = FPlatformTime::Seconds();
	
	FFileHelper::SaveArrayToFile(Model.Data, *ModelToOptimizePath);

#if WITH_EDITOR
	try
#endif // WITH_EDITOR
	{
		Ort::Env Env(ORT_LOGGING_LEVEL_INFO);
		Ort::SessionOptions SessOptions;

		SessOptions.SetGraphOptimizationLevel(OptimizationLevel);
#if PLATFORM_WINDOWS
		SessOptions.SetOptimizedModelFilePath(*ModelOptimizedPath);
		Ort::Session Session(Env, *ModelToOptimizePath, SessOptions);
#else
		SessOptions.SetOptimizedModelFilePath(TCHAR_TO_ANSI(*ModelOptimizedPath));
		Ort::Session Session(Env, TCHAR_TO_ANSI(*ModelToOptimizePath), SessOptions);
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

	double ONNXModelOptimisationEndTime = FPlatformTime::Seconds();
	float ONNXModelOptimisationTime = static_cast<float>(ONNXModelOptimisationEndTime - ONNXModelOptimisationStartTime);

	UE_LOG(LogNNE, Display, TEXT("NNERuntimeORT: ONNX Runtime Model Optimizer runned in %0.1f seconds."), ONNXModelOptimisationTime);

	Model.Format = TargetFormat;

	return true;
}

} // OrtHelper
} // namespace UE::NNERuntimeORT::Private