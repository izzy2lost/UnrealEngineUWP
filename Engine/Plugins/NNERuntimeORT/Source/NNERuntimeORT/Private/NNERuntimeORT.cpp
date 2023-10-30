// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORT.h"

#include "EngineAnalytics.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "NNEAttributeMap.h"
#include "NNEModelData.h"
#include "NNEModelOptimizerInterface.h"
#include "NNERuntimeORTModel.h"
#include "NNERuntimeORTUtils.h"
#include "NNEUtilitiesModelOptimizer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NNERuntimeORT)

FGuid UNNERuntimeORTDml::GUID = FGuid((int32)'O', (int32)'G', (int32)'P', (int32)'U');
int32 UNNERuntimeORTDml::Version = 0x00000001;

FGuid UNNERuntimeORTCpu::GUID = FGuid((int32)'O', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeORTCpu::Version = 0x00000001;

bool UNNERuntimeORTDml::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const
{
	return !FileData.IsEmpty() && FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0;
}

bool UNNERuntimeORTCpu::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const
{
	return !FileData.IsEmpty() && FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0;
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeORTDml::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	if (!CanCreateModelData(FileType, FileData, FileId, TargetPlatform))
	{
		return {};
	}

	TUniquePtr<UE::NNE::Internal::IModelOptimizer> Optimizer = UE::NNEUtilities::Internal::CreateONNXToONNXModelOptimizer();

	FNNEModelRaw InputModel;
	InputModel.Data = FileData;
	InputModel.Format = ENNEInferenceFormat::ONNX;
	FNNEModelRaw OutputModel;
	UE::NNE::Internal::FOptimizerOptionsMap Options;
	if (!Optimizer->Optimize(InputModel, OutputModel, Options))
	{
		return {};
	}

	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	Writer << UNNERuntimeORTDml::GUID;
	Writer << UNNERuntimeORTDml::Version;
	Writer.Serialize(OutputModel.Data.GetData(), OutputModel.Data.Num());

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeORTCpu::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	if (!CanCreateModelData(FileType, FileData, FileId, TargetPlatform))
	{
		return {};
	}

	TUniquePtr<UE::NNE::Internal::IModelOptimizer> Optimizer = UE::NNEUtilities::Internal::CreateONNXToONNXModelOptimizer();

	FNNEModelRaw InputModel;
	InputModel.Data = FileData;
	InputModel.Format = ENNEInferenceFormat::ONNX;
	FNNEModelRaw OutputModel;
	UE::NNE::Internal::FOptimizerOptionsMap Options;
	if (!Optimizer->Optimize(InputModel, OutputModel, Options))
	{
		return {};
	}

	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	Writer << UNNERuntimeORTCpu::GUID;
	Writer << UNNERuntimeORTCpu::Version;
	Writer.Serialize(OutputModel.Data.GetData(), OutputModel.Data.Num());

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
}

FString UNNERuntimeORTDml::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeORTDml::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeORTDml::Version);
}

FString UNNERuntimeORTCpu::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeORTCpu::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeORTCpu::Version);
}

void UNNERuntimeORTDml::Init()
{
	check(!ORTEnvironment.IsValid());
	ORTEnvironment = MakeShared<Ort::Env>();
}

void UNNERuntimeORTCpu::Init()
{
	check(!ORTEnvironment.IsValid());
	ORTEnvironment = MakeShared<Ort::Env>();
}

FString UNNERuntimeORTDml::GetRuntimeName() const
{
	return TEXT("NNERuntimeORTDml");
}

FString UNNERuntimeORTCpu::GetRuntimeName() const
{
	return TEXT("NNERuntimeORTCpu");
}

bool UNNERuntimeORTCpu::CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	constexpr int32 GuidSize = sizeof(UNNERuntimeORTCpu::GUID);
	constexpr int32 VersionSize = sizeof(UNNERuntimeORTCpu::Version);
	const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());

	if (!SharedData.IsValid())
	{
		return false;
	}

	TConstArrayView<uint8> Data = SharedData->GetView();

	if (Data.Num() <= GuidSize + VersionSize)
	{
		return false;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeORTCpu::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeORTCpu::Version), VersionSize) == 0;
	return bResult;
}

TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeORTCpu::CreateModelCPU(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	check(ORTEnvironment.IsValid());

	if (!CanCreateModelCPU(ModelData))
	{
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	check(SharedData.IsValid());

	UE::NNE::IModelCPU* IModel = static_cast<UE::NNE::IModelCPU*>(new UE::NNERuntimeORT::Private::FModelORTCpu(ORTEnvironment, SharedData));
	check(IModel != nullptr);

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), SharedData->GetView().Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelCPU>(IModel);
}

#if PLATFORM_WINDOWS
bool UNNERuntimeORTDml::CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	constexpr int32 GuidSize = sizeof(UNNERuntimeORTDml::GUID);
	constexpr int32 VersionSize = sizeof(UNNERuntimeORTDml::Version);
	const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());

	if (!SharedData.IsValid())
	{
		return false;
	}

	TConstArrayView<uint8> Data = SharedData->GetView();

	if (Data.Num() <= GuidSize + VersionSize)
	{
		return false;
	}

	static const FGuid DeprecatedGUID = FGuid((int32)'O', (int32)'D', (int32)'M', (int32)'L');

	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeORTDml::GUID), GuidSize) == 0;
	bResult |= FGenericPlatformMemory::Memcmp(&(Data[0]), &(DeprecatedGUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeORTDml::Version), VersionSize) == 0;
	return bResult;
}

TSharedPtr<UE::NNE::IModelGPU> UNNERuntimeORTDml::CreateModelGPU(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	check(ORTEnvironment.IsValid());

	if (!CanCreateModelGPU(ModelData))
	{
		return TSharedPtr<UE::NNE::IModelGPU>();
	}

	const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	check(SharedData.IsValid());

	UE::NNE::IModelGPU* IModel = static_cast<UE::NNE::IModelGPU*>(new UE::NNERuntimeORT::Private::FModelORTDml(ORTEnvironment, SharedData));
	check(IModel != nullptr);

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), SharedData->GetView().Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelGPU>(IModel);
}

#else // PLATFORM_WINDOWS

bool UNNERuntimeORTDml::CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const
{
	return false;
}

TSharedPtr<UE::NNE::IModelGPU> UNNERuntimeORTDml::CreateModelGPU(TObjectPtr<UNNEModelData> ModelData)
{
	return TSharedPtr<UE::NNE::IModelGPU>();
}

#endif // PLATFORM_WINDOWS