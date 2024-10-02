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

static bool GRunningCheckRetargetSourceAssetDataCmd = false;

int32 GEnablePostLoadRetargetSourceErrorReporting = 1;
static FAutoConsoleVariableRef CVarStripAdditiveRefPose(
	TEXT("a.EnablePostLoadRetargetSourceErrorReporting"),
	GEnablePostLoadRetargetSourceErrorReporting,
	TEXT("1 = Enables validation of retarget source asset data. 0 = off"));

template<typename AssetType>
ERetargetSourceAssetStatus CheckRetargetSourceAssetDataImpl(const AssetType* InAsset, bool bLogIssues)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const TSoftObjectPtr<USkeletalMesh>& RetargetSourceAsset = InAsset->RetargetSourceAsset;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FAssetData AssetData;
	UE::AssetRegistry::EExists AssetDataStatus = RetargetSourceAsset.IsNull() 
		? UE::AssetRegistry::EExists::DoesNotExist
		: IAssetRegistry::GetChecked().TryGetAssetByObjectPath(RetargetSourceAsset.ToSoftObjectPath(), AssetData);

	if (AssetDataStatus != AssetRegistry::EExists::Unknown)
	{
		if (InAsset->RetargetSourceAssetReferencePose.IsEmpty())
		{
			return ERetargetSourceAssetStatus::NoRetargetDataSet;
		}
		else // InAsset->RetargetSourceAssetReferencePose.Num() > 0
		{
			if (AssetDataStatus == UE::AssetRegistry::EExists::Exists)
			{
				return ERetargetSourceAssetStatus::RetargetDataOk;
			}
			else // if (AssetDataStatus == UE::AssetRegistry::EExists::DoesNotExist)
			{
				if (bLogIssues)
				{
					UE_LOG(LogAnimation, Warning, TEXT("Asset [%s] references a missing Retarget Source Asset [%s]. Retarget Reference Pose has [%d] elements. Please, add a correct retarget source asset and resave.")
						, *InAsset->GetFullName()
						, *(RetargetSourceAsset.GetLongPackageName() + FString(TEXT("/")) + RetargetSourceAsset.GetAssetName())
						, InAsset->RetargetSourceAssetReferencePose.Num());
				}
				return ERetargetSourceAssetStatus::RetargetSourceMissing;
			}
		}
	}

	// if Asset registry is not ready, we go with a slow path
	if (InAsset->RetargetSourceAssetReferencePose.Num() > 0)
	{
		const USkeletalMesh* SourceReferenceMesh = InAsset->GetRetargetSourceAsset();
		if (SourceReferenceMesh == nullptr)
		{
			if (bLogIssues)
			{
				UE_LOG(LogAnimation, Warning, TEXT("Asset [%s] references a missing Retarget Source Asset [%s]. Retarget Reference Pose has [%d] elements. Please, add a correct retarget source asset and resave.")
					, *InAsset->GetFullName()
					, *(RetargetSourceAsset.GetLongPackageName() + FString(TEXT("/")) + RetargetSourceAsset.GetAssetName())
					, InAsset->RetargetSourceAssetReferencePose.Num());
			}
			return ERetargetSourceAssetStatus::RetargetSourceMissing;
		}
		else
		{
			return ERetargetSourceAssetStatus::RetargetDataOk;
		}
	}

	return ERetargetSourceAssetStatus::NoRetargetDataSet;
}

void CheckRetargetSourceAssetData(bool bFixAssets)
{
	TGuardValue<bool> CompilationGuard(GRunningCheckRetargetSourceAssetDataCmd, true);

	TArray<FAssetData> Assets;
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), Assets);
	AssetRegistry.GetAssetsByClass(UPoseAsset::StaticClass()->GetClassPathName(), Assets);

	const int32 NumAssets = Assets.Num();
	for (int32 Idx = 0; Idx < NumAssets; Idx++)
	{
		if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Assets[Idx].GetAsset()))
		{
			const ERetargetSourceAssetStatus Status = Private::CheckRetargetSourceAssetDataImpl(AnimSequence, true);
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
			const ERetargetSourceAssetStatus Status = Private::CheckRetargetSourceAssetDataImpl(PoseAsset, true);
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

bool ShouldCheckRetargetSourceAssetData()
{
#if WITH_EDITOR
	return !Private::GRunningCheckRetargetSourceAssetDataCmd;
#else
	return false;
#endif
}

ERetargetSourceAssetStatus CheckRetargetSourceAssetData(const UAnimSequence* InAsset)
{
	return Private::CheckRetargetSourceAssetDataImpl(InAsset, Private::GEnablePostLoadRetargetSourceErrorReporting != 0);
}

ERetargetSourceAssetStatus CheckRetargetSourceAssetData(const UPoseAsset* InAsset)
{
	return Private::CheckRetargetSourceAssetDataImpl(InAsset, Private::GEnablePostLoadRetargetSourceErrorReporting != 0);
}
#endif // WITH_EDITOR

} // end namespace UE::Anim::RetargetHelpers

#undef LOCTEXT_NAMESPACE 
