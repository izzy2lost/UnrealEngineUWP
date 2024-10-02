// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimRetargetHelpers.h"


#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Templates/UnrealTemplate.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "AnimRetargetHelpers"

namespace UE::Anim::RetargetHelpers
{

#if WITH_EDITOR

namespace Private
{

int32 GEnablePostLoadRetargetSourceErrorReporting = 1;
static FAutoConsoleVariableRef CVarStripAdditiveRefPose(
	TEXT("a.EnablePostLoadRetargetSourceErrorReporting"),
	GEnablePostLoadRetargetSourceErrorReporting,
	TEXT("1 = Enables validation of retarget source asset data. 0 = off"));

template<typename AssetType>
ERetargetSourceAssetStatus CheckRetargetSourceAssetDataImpl(const AssetType* InAsset)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const TSoftObjectPtr<USkeletalMesh>& RetargetSourceAsset = InAsset->RetargetSourceAsset;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FAssetData AssetData;
	UE::AssetRegistry::EExists AssetDataStatus = RetargetSourceAsset.IsNull() 
		? UE::AssetRegistry::EExists::DoesNotExist
		: IAssetRegistry::GetChecked().TryGetAssetByObjectPath(RetargetSourceAsset.ToSoftObjectPath(), AssetData);

	if (InAsset->RetargetSourceAssetReferencePose.Num() > 0)
	{
		if (AssetDataStatus != AssetRegistry::EExists::Unknown)
		{
			if (AssetDataStatus == UE::AssetRegistry::EExists::Exists)
			{
				return ERetargetSourceAssetStatus::RetargetDataOk;
			}
			else // if (AssetDataStatus == UE::AssetRegistry::EExists::DoesNotExist)
			{
				UE_LOG(LogAnimation, Warning, TEXT("Asset [%s] references a missing Retarget Source Asset [%s]. Retarget Reference Pose has [%d] elements. Please, add a correct retarget source asset and resave.")
					, *InAsset->GetFullName()
					, *(RetargetSourceAsset.GetLongPackageName() + FString(TEXT("/")) + RetargetSourceAsset.GetAssetName())
					, InAsset->RetargetSourceAssetReferencePose.Num());
				return ERetargetSourceAssetStatus::RetargetSourceMissing;
			}
		}
		else // if Asset registry is not ready, we go with a slow path
		{
			const USkeletalMesh* SourceReferenceMesh = RetargetSourceAsset.LoadSynchronous();
			if (SourceReferenceMesh == nullptr)
			{
					UE_LOG(LogAnimation, Warning, TEXT("Asset [%s] references a missing Retarget Source Asset [%s]. Retarget Reference Pose has [%d] elements. Please, add a correct retarget source asset and resave.")
						, *InAsset->GetFullName()
						, *(RetargetSourceAsset.GetLongPackageName() + FString(TEXT("/")) + RetargetSourceAsset.GetAssetName())
						, InAsset->RetargetSourceAssetReferencePose.Num());
				return ERetargetSourceAssetStatus::RetargetSourceMissing;
			}
			else
			{
				return ERetargetSourceAssetStatus::RetargetDataOk;
			}
		}
	}

	return ERetargetSourceAssetStatus::NoRetargetDataSet;
}

void CheckRetargetSourceAssetData(bool bFixAssets)
{
	TArray<FAssetData> Assets;
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), Assets);
	AssetRegistry.GetAssetsByClass(UPoseAsset::StaticClass()->GetClassPathName(), Assets);

	const int32 NumAssets = Assets.Num();
	for (int32 Idx = 0; Idx < NumAssets; Idx++)
	{
		if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Assets[Idx].GetAsset()))
		{
			const ERetargetSourceAssetStatus Status = Private::CheckRetargetSourceAssetDataImpl(AnimSequence);
			if (bFixAssets)
			{
				if (Status == ERetargetSourceAssetStatus::RetargetSourceMissing)
				{
					AnimSequence->Modify();
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					AnimSequence->RetargetSourceAsset.Reset();
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
					AnimSequence->RetargetSourceAssetReferencePose.Empty();
					AnimSequence->MarkPackageDirty();
				}
			}
		}
		else if (UPoseAsset* PoseAsset = Cast<UPoseAsset>(Assets[Idx].GetAsset()))
		{
			const ERetargetSourceAssetStatus Status = Private::CheckRetargetSourceAssetDataImpl(PoseAsset);
			if (bFixAssets)
			{
				if (Status == ERetargetSourceAssetStatus::RetargetSourceMissing)
				{
					PoseAsset->Modify();
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					PoseAsset->RetargetSourceAsset.Reset();
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
					PoseAsset->RetargetSourceAssetReferencePose.Empty();
					PoseAsset->MarkPackageDirty();
				}
			}
		}
	}
}

static FAutoConsoleCommand CheckRetargetSourceAssetDataCmd(
	TEXT("a.CheckRetargetSourceAssetData"),
	TEXT("Checks if Anim Sequences and Pose Assets RetargetSourceAsset is valid. Use: 'a.CheckRetargetSourceAssetData' to check or 'a.CheckRetargetSourceAssetData true' to check and fix the assets."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() > 0)
			{
				CheckRetargetSourceAssetData(Args[0].ToBool());
			}
			else
			{
				CheckRetargetSourceAssetData(false);
			}
		}
	));
} // end namespace Private

ERetargetSourceAssetStatus CheckRetargetSourceAssetData(const UAnimSequence* InAsset)
{
	return Private::CheckRetargetSourceAssetDataImpl(InAsset);
}

ERetargetSourceAssetStatus CheckRetargetSourceAssetData(const UPoseAsset* InAsset)
{
	return Private::CheckRetargetSourceAssetDataImpl(InAsset);
}
#endif // WITH_EDITOR

} // end namespace UE::Anim::RetargetHelpers

#undef LOCTEXT_NAMESPACE 
