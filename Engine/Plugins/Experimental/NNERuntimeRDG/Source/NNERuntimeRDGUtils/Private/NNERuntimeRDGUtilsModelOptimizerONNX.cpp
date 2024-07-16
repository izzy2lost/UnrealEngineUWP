// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGUtilsModelOptimizerONNX.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NNE.h"
#include "NNEOnnxruntimeEditor.h"

THIRD_PARTY_INCLUDES_START
#include <onnx/onnx_pb.h>
#include <onnx/shape_inference/implementation.h>
THIRD_PARTY_INCLUDES_END

namespace UE::NNERuntimeRDGUtils::Internal
{

class FOnnxRuntimeModelOptimizerPass : public NNE::Internal::IModelOptimizerPass
{
	ENNEInferenceFormat	TargetFormat;

public:
	FOnnxRuntimeModelOptimizerPass(ENNEInferenceFormat OutFormat) : TargetFormat(OutFormat)
	{
		check(TargetFormat == ENNEInferenceFormat::ONNX || TargetFormat == ENNEInferenceFormat::ORT);
	}

	virtual FString GetName() const
	{
		return TEXT("Onnx runtime model optimization");
	}

	virtual bool ApplyPass(FNNEModelRaw& Model, const FOptimizerOptionsMap& Options) const
	{
		if (Model.Format != ENNEInferenceFormat::ONNX)
		{
			UE_LOG(LogNNE, Warning, TEXT("%s is expecting a model in ONNX format but received %u."), *(GetName()), Model.Format);
			return false;
		}

		static const auto CVarHlslModelOptimization = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("nne.hlsl.ModelOptimization"));
		if (CVarHlslModelOptimization && CVarHlslModelOptimization->GetValueOnAnyThread() == 0)
		{
			return true;
		}

		FString ProjIntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir());
		FString ModelToOptimizePath = FPaths::CreateTempFilename(*ProjIntermediateDir, TEXT("ORTOptimizerPass_ToOptimize"), TEXT(".onnx"));
		FString TargetExtension = TargetFormat == ENNEInferenceFormat::ONNX ? TEXT(".onnx") : TEXT(".ort");
		FString ModelOptimizedPath = FPaths::CreateTempFilename(*ProjIntermediateDir, TEXT("ORTOptimizerPass_Optimized"), *TargetExtension);

		//See https://onnxruntime.ai/docs/performance/model-optimizations/graph-optimizations.html
		//We only enable all the optimization when going to ORT format itself for the CPU provider
		GraphOptimizationLevel OptimizationLevel = TargetFormat == ENNEInferenceFormat::ONNX ? ORT_ENABLE_BASIC : ORT_ENABLE_ALL;

		FFileHelper::SaveArrayToFile(Model.Data, *ModelToOptimizePath);
		{
			Ort::ThreadingOptions ThreadingOptions;
			ThreadingOptions.SetGlobalIntraOpNumThreads(1);
			ThreadingOptions.SetGlobalInterOpNumThreads(1);

			Ort::Env Env(ThreadingOptions);

			Ort::SessionOptions SessionOptions;
			SessionOptions.DisablePerSessionThreads();
			SessionOptions.SetGraphOptimizationLevel(OptimizationLevel);
			#if PLATFORM_WINDOWS
				SessionOptions.SetOptimizedModelFilePath(*ModelOptimizedPath);

				Ort::Session Session(Env, *ModelToOptimizePath, SessionOptions);
			#else
				SessionOptions.SetOptimizedModelFilePath(TCHAR_TO_ANSI(*ModelOptimizedPath));

				Ort::Session Session(Env, TCHAR_TO_ANSI(*ModelToOptimizePath), SessionOptions);
			#endif
		}
		FFileHelper::LoadFileToArray(Model.Data, *ModelOptimizedPath);

		IFileManager::Get().Delete(*ModelToOptimizePath);
		IFileManager::Get().Delete(*ModelOptimizedPath);

		Model.Format = TargetFormat;

		return true;
	}

};

class FOnnxDomainCleanupModelOptimizerPass : public NNE::Internal::IModelOptimizerPass
{
public:
	virtual FString GetName() const
	{
		return TEXT("Onnx domain cleanup");
	}

	virtual bool ApplyPass(FNNEModelRaw& Model, const FOptimizerOptionsMap& Options) const
	{
		if (Model.Format != ENNEInferenceFormat::ONNX)
		{
			UE_LOG(LogNNE, Warning, TEXT("%s is expecting a model in ONNX format but received %u."), *(GetName()), Model.Format);
			return false;
		}

		onnx::ModelProto ModelProto;
		const bool result = ModelProto.ParseFromArray(Model.Data.GetData(), Model.Data.Num());
		if (!result)
		{
			UE_LOG(LogNNE, Warning, TEXT("%s could not parse the input model as a ModelProto."), *(GetName()));
			return false;
		}

		TArray<FString> UsedDomains;
		const onnx::GraphProto& graph = ModelProto.graph();
		for (const onnx::NodeProto& node : graph.node())
		{
			const char* DomainPtr = node.domain().c_str();
			FString Domain = DomainPtr;
			UsedDomains.AddUnique(Domain);
		}

		google::protobuf::RepeatedPtrField<onnx::OperatorSetIdProto> UsedOperatorSet;
		for (const onnx::OperatorSetIdProto& OpSet : ModelProto.opset_import())
		{
			const char* DomainPtr = OpSet.domain().c_str();
			FString Domain = DomainPtr;
			bool IsUsed = UsedDomains.Contains(Domain);
			if (IsUsed)
			{
				UsedOperatorSet.Add()->CopyFrom(OpSet);
			}
		}

		ModelProto.mutable_opset_import()->Clear();
		ModelProto.mutable_opset_import()->Add(UsedOperatorSet.cbegin(), UsedOperatorSet.cend());

		Model.Data.SetNumUninitialized(ModelProto.ByteSizeLong());
		ModelProto.SerializeToArray(Model.Data.GetData(), Model.Data.Num());

		return true;
	}

};

class FOnnxShapeInferenceModelOptimizerPass : public NNE::Internal::IModelOptimizerPass
{
public:
	virtual FString GetName() const
	{
		return TEXT("Onnx shape inference");
	}

	virtual bool ApplyPass(FNNEModelRaw& Model, const FOptimizerOptionsMap& Options) const
	{
		if (Model.Format != ENNEInferenceFormat::ONNX)
		{
			UE_LOG(LogNNE, Warning, TEXT("%s is expecting a mNNEdel in ONNX format but received %u."), *(GetName()), Model.Format);
			return false;
		}

		onnx::ModelProto ModelProto;
		const bool result = ModelProto.ParseFromArray(Model.Data.GetData(), Model.Data.Num());
		if (!result)
		{
			UE_LOG(LogNNE, Warning, TEXT("%s could not parse the input model as a ModelProto."), *(GetName()));
			return false;
		}

#ifdef ONNX_NO_EXCEPTIONS
		UE_LOG(LogNNE, Warning, TEXT("ONNX Shape inference can't be run as exception are disabled."));
		return true;
#else

		static onnx::OpSchemaRegistry* OnnxSchemaRegistry = onnx::OpSchemaRegistry::Instance();

		try
		{
			onnx::shape_inference::InferShapes(ModelProto, OnnxSchemaRegistry);
		}
		catch (onnx::InferenceError& e)
		{
			UE_LOG(LogNNE, Warning, TEXT("Shape inference failed with : %s."), ANSI_TO_TCHAR(e.what()));
		}
#endif
		
		ModelProto.SerializeToArray(Model.Data.GetData(), Model.Data.Num());

		return true;
	}

};

FModelOptimizerONNXToONNX::FModelOptimizerONNXToONNX()
{
	AddOptimizationPass(MakeShared<FOnnxRuntimeModelOptimizerPass>(ENNEInferenceFormat::ONNX));
	AddOptimizationPass(MakeShared<FOnnxDomainCleanupModelOptimizerPass>());
	AddOptimizationPass(MakeShared<FOnnxShapeInferenceModelOptimizerPass>());
	AddValidator(MakeShared<FModelValidatorONNX>());
}

FModelOptimizerONNXToORT::FModelOptimizerONNXToORT()
{
	AddOptimizationPass(MakeShared<FOnnxRuntimeModelOptimizerPass>(ENNEInferenceFormat::ORT));
	AddValidator(MakeShared<FModelValidatorONNX>());
}

} // namespace UE::NNERuntimeRDGUtils::Internal
