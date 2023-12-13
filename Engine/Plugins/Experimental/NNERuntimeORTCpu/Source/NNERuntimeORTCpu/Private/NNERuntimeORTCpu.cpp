// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTCpu.h"

#include "EngineAnalytics.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "NNEAttributeMap.h"
#include "NNEModelData.h"
#include "NNEModelOptimizerInterface.h"
#include "NNEProfilingTimer.h"
#include "NNERuntimeORTCpuModel.h"
#include "NNERuntimeORTCpuUtils.h"
#include "NNEUtilsModelOptimizer.h"

FGuid UNNERuntimeORTCustomCpuImpl::GUID = FGuid((int32)'O', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeORTCustomCpuImpl::Version = 0x00000001;

bool UNNERuntimeORTCustomCpuImpl::CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0;
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeORTCustomCpuImpl::CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	if (!CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform))
	{
		return {};
	}

	TUniquePtr<UE::NNE::Internal::IModelOptimizer> Optimizer = UE::NNEUtils::Internal::CreateONNXToONNXModelOptimizer();

	FNNEModelRaw InputModel;
	InputModel.Data = FileData;
	InputModel.Format = ENNEInferenceFormat::ONNX;
	FNNEModelRaw OutputModel;
	UE::NNEUtils::Internal::FOptimizerOptionsMap Options;
	if (!Optimizer->Optimize(InputModel, OutputModel, Options))
	{
		return {};
	}

	int32 GuidSize = sizeof(UNNERuntimeORTCustomCpuImpl::GUID);
	int32 VersionSize = sizeof(UNNERuntimeORTCustomCpuImpl::Version);
	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	Writer << UNNERuntimeORTCustomCpuImpl::GUID;
	Writer << UNNERuntimeORTCustomCpuImpl::Version;
	Writer.Serialize(OutputModel.Data.GetData(), OutputModel.Data.Num());

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
}

FString UNNERuntimeORTCustomCpuImpl::GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeORTCustomCpuImpl::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeORTCustomCpuImpl::Version);
}

bool UNNERuntimeORTCustomCpuImpl::CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);
	
	int32 GuidSize = sizeof(UNNERuntimeORTCustomCpuImpl::GUID);
	int32 VersionSize = sizeof(UNNERuntimeORTCustomCpuImpl::Version);
	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());

	if (!SharedData.IsValid())
	{
		return false;
	}

	TConstArrayView<uint8> Data = SharedData->GetView();
	
	if (Data.Num() <= GuidSize + VersionSize)
	{
		return false;
	}
	
	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeORTCustomCpuImpl::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeORTCustomCpuImpl::Version), VersionSize) == 0;
	return bResult;
}

TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeORTCustomCpuImpl::CreateModelCPU(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	
	if (!CanCreateModelCPU(ModelData))
	{
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	TSharedPtr<UE::NNE::FSharedModelData> Data = ModelData->GetModelData(GetRuntimeName());
	check(Data.IsValid());
	UE::NNERuntimeORTCpu::Private::FModelCPU* Model = new UE::NNERuntimeORTCpu::Private::FModelCPU(&NNEEnvironmentCPU, Data);
	UE::NNE::IModelCPU* IModel = static_cast<UE::NNE::IModelCPU*>(Model);

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), Data->GetView().Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelCPU>(IModel);
}