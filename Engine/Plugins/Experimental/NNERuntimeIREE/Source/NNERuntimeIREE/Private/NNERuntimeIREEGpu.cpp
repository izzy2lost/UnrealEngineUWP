// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREEGpu.h"

#include "EngineAnalytics.h"
#include "Interfaces/ITargetPlatform.h"
#include "Kismet/GameplayStatics.h"
#include "NNERuntimeIREECommon.h"

FGuid UNNERuntimeIREECuda::GUID = FGuid((int32)'I', (int32)'G', (int32)'C', (int32)'U');
int32 UNNERuntimeIREECuda::Version = 0x00000001;

FGuid UNNERuntimeIREEVulkan::GUID = FGuid((int32)'I', (int32)'G', (int32)'V', (int32)'U');
int32 UNNERuntimeIREEVulkan::Version = 0x00000001;

#ifdef WITH_NNE_RUNTIME_IREE

FString UNNERuntimeIREEGpu::GetRuntimeName() const
{
	return TEXT("");
}

FString UNNERuntimeIREECuda::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREECuda");
}

FString UNNERuntimeIREEVulkan::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREEVulkan");
}

bool UNNERuntimeIREEGpu::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	return	FileType.Compare(TEXT("mlir"), ESearchCase::IgnoreCase) == 0;
#else
	return false;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREEGpu::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	return TSharedPtr<UE::NNE::FSharedModelData>();
}

FString UNNERuntimeIREEGpu::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	FString PlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();
	return GetRuntimeName() + "-" + GetGUID().ToString(EGuidFormats::Digits) + "-" + FString::FromInt(GetVersion()) + "-" + FileId.ToString(EGuidFormats::Digits) + "-" + PlatformName;
}

bool UNNERuntimeIREEGpu::CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return false;
	}

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	FGuid Guid = GetGUID();
	int32 Version = GetVersion();
	int32 GuidSize = sizeof(Guid);
	int32 VersionSize = sizeof(Version);
	if (SharedDataView.Num() <= GuidSize + VersionSize)
	{
		return false;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(SharedDataView[0]), &(Guid), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(SharedDataView[GuidSize]), &(Version), VersionSize) == 0;
	return bResult;
}

TSharedPtr<UE::NNE::IModelGPU> UNNERuntimeIREEGpu::CreateModelGPU(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	if (!CanCreateModelGPU(ModelData))
	{
		return TSharedPtr<UE::NNE::IModelGPU>();
	}

	check(ModelData->GetModelData(GetRuntimeName()).IsValid());

	UE::NNE::IModelGPU* IModel = nullptr;
	TConstArrayView<uint8> SharedDataView = ModelData->GetModelData(GetRuntimeName())->GetView();

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), SharedDataView.Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelGPU>(IModel);
}

bool UNNERuntimeIREEGpu::IsAvailable() const
{
	return false;
}

bool UNNERuntimeIREECuda::IsAvailable() const
{
	return false;
}

bool UNNERuntimeIREEVulkan::IsAvailable() const
{
	return false;
}

FGuid UNNERuntimeIREEGpu::GetGUID() const
{
	return FGuid();
}

FGuid UNNERuntimeIREECuda::GetGUID() const
{
	return GUID;
}

FGuid UNNERuntimeIREEVulkan::GetGUID() const
{
	return GUID;
}

int32 UNNERuntimeIREEGpu::GetVersion() const
{
	return 0;
}

int32 UNNERuntimeIREECuda::GetVersion() const
{
	return Version;
}

int32 UNNERuntimeIREEVulkan::GetVersion() const
{
	return Version;
}

#else // WITH_NNE_RUNTIME_IREE

FString UNNERuntimeIREEGpu::GetRuntimeName() const { return TEXT(""); }
FString UNNERuntimeIREECuda::GetRuntimeName() const { return TEXT(""); }
FString UNNERuntimeIREEVulkan::GetRuntimeName() const { return TEXT(""); }

bool UNNERuntimeIREEGpu::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const { return false; };
TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREEGpu::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) { return TSharedPtr<UE::NNE::FSharedModelData>(); };
FString UNNERuntimeIREEGpu::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) { return ""; };

bool UNNERuntimeIREEGpu::CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const { return false; };
TSharedPtr<UE::NNE::IModelGPU> UNNERuntimeIREEGpu::CreateModelGPU(TObjectPtr<UNNEModelData> ModelData) { return TSharedPtr<UE::NNE::IModelGPU>(); };

bool UNNERuntimeIREEGpu::IsAvailable() const { return false; }
bool UNNERuntimeIREECuda::IsAvailable() const { return false; }
bool UNNERuntimeIREEVulkan::IsAvailable() const { return false; }

FGuid UNNERuntimeIREEGpu::GetGUID() const { return FGuid(); }
FGuid UNNERuntimeIREECuda::GetGUID() const { return GUID; }
FGuid UNNERuntimeIREEVulkan::GetGUID() const { return GUID; }

int32 UNNERuntimeIREEGpu::GetVersion() const { return 0; }
int32 UNNERuntimeIREECuda::GetVersion() const { return Version; }
int32 UNNERuntimeIREEVulkan::GetVersion() const { return Version; }

#endif // WITH_NNE_RUNTIME_IREE