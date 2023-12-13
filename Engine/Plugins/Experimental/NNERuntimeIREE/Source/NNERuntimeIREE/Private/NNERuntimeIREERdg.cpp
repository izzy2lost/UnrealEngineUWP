// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREERdg.h"

#include "EngineAnalytics.h"
#include "Interfaces/ITargetPlatform.h"
#include "Kismet/GameplayStatics.h"
#include "NNERuntimeIREECommon.h"

FGuid UNNERuntimeIREERdg::GUID = FGuid((int32)'I', (int32)'R', (int32)'D', (int32)'G');
int32 UNNERuntimeIREERdg::Version = 0x00000001;

#ifdef WITH_NNE_RUNTIME_IREE

FString UNNERuntimeIREERdg::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREERdg");
}

bool UNNERuntimeIREERdg::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	return	FileType.Compare(TEXT("mlir"), ESearchCase::IgnoreCase) == 0;
#else
	return false;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREERdg::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	return TSharedPtr<UE::NNE::FSharedModelData>();
}

FString UNNERuntimeIREERdg::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	FString PlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();
	return GetRuntimeName() + "-" + GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeIREERdg::Version) + "-" + FileId.ToString(EGuidFormats::Digits) + "-" + PlatformName;
}

bool UNNERuntimeIREERdg::CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return false;
	}

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	int32 GuidSize = sizeof(UNNERuntimeIREERdg::GUID);
	int32 VersionSize = sizeof(UNNERuntimeIREERdg::Version);
	if (SharedDataView.Num() <= GuidSize + VersionSize)
	{
		return false;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(SharedDataView[0]), &(UNNERuntimeIREERdg::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(SharedDataView[GuidSize]), &(UNNERuntimeIREERdg::Version), VersionSize) == 0;
	return bResult;
}

TSharedPtr<UE::NNE::IModelRDG> UNNERuntimeIREERdg::CreateModelRDG(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	if (!CanCreateModelRDG(ModelData))
	{
		return TSharedPtr<UE::NNE::IModelRDG>();
	}

	check(ModelData->GetModelData(GetRuntimeName()).IsValid());

	UE::NNE::IModelRDG* IModel = nullptr;
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

	return TSharedPtr<UE::NNE::IModelRDG>(IModel);
}

bool UNNERuntimeIREERdg::IsAvailable() const
{
	return false;
}

#else // WITH_NNE_RUNTIME_IREE

FString UNNERuntimeIREERdg::GetRuntimeName() const { return TEXT(""); }

bool UNNERuntimeIREERdg::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const { return false; };
TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREERdg::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) { return TSharedPtr<UE::NNE::FSharedModelData>(); };
FString UNNERuntimeIREERdg::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) { return ""; };

bool UNNERuntimeIREERdg::CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const { return false; };
TSharedPtr<UE::NNE::IModelRDG> UNNERuntimeIREERdg::CreateModelRDG(TObjectPtr<UNNEModelData> ModelData) { return TSharedPtr<UE::NNE::IModelRDG>(); };

bool UNNERuntimeIREERdg::IsAvailable() const { return false; }

#endif // WITH_NNE_RUNTIME_IREE