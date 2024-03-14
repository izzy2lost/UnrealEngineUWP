// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/DataLayer/ExternalDataLayerHelper.h"
#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetRegistry/ARFilter.h"
#include "ExternalPackageHelper.h"
#include "UObject/Object.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "WorldPartition/DataLayer/ExternalDataLayerManager.h"
#include "ReferencedAssetsUtils.h"
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "ExternalDataLayerHelper"

FString FExternalDataLayerHelper::GetExternalStreamingObjectPackageName(const UExternalDataLayerAsset* InExternalDataLayerAsset)
{
	check(InExternalDataLayerAsset);
	return FString::Printf(TEXT("StreamingObject_%X"), (uint32)InExternalDataLayerAsset->GetUID());
}

FString FExternalDataLayerHelper::GetExternalStreamingObjectName(const UExternalDataLayerAsset* InExternalDataLayerAsset)
{
	check(InExternalDataLayerAsset);
	return SlugStringForValidName(InExternalDataLayerAsset->GetName() + TEXT("_") + InExternalDataLayerAsset->GetUID().ToString() + TEXT("_ExternalStreamingObject"));
}

bool FExternalDataLayerHelper::BuildExternalDataLayerRootPath(const FString& InEDLMountPoint, const FExternalDataLayerUID& InExternalDataLayerUID, FString& OutExternalDataLayerRootPath)
{
	if (InEDLMountPoint.IsEmpty() || !InExternalDataLayerUID.IsValid())
	{
		return false;
	}

	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> Builder;
	Builder += TEXT("/");
	Builder += InEDLMountPoint;
	Builder += GetExternalDataLayerFolder();
	Builder += InExternalDataLayerUID.ToString();
	OutExternalDataLayerRootPath = *Builder;
	return true;
}

FString FExternalDataLayerHelper::GetExternalDataLayerLevelRootPath(const UExternalDataLayerAsset* InExternalDataLayerAsset, const FString& InLevelPackagePath)
{
	check(InExternalDataLayerAsset);
	check(InExternalDataLayerAsset->GetUID().IsValid());
	return FExternalDataLayerHelper::GetExternalDataLayerLevelRootPath(FPackageName::GetPackageMountPoint(InExternalDataLayerAsset->GetPackage()->GetName()).ToString(), InExternalDataLayerAsset->GetUID(), InLevelPackagePath);
}

FString FExternalDataLayerHelper::GetExternalDataLayerLevelRootPath(const FString& InExternalDataLayerMountPoint, const FExternalDataLayerUID& InExternalDataLayerUID, const FString& InLevelPackagePath)
{
	FString ExternalDataLayerRootPath;
	verify(BuildExternalDataLayerRootPath(InExternalDataLayerMountPoint, InExternalDataLayerUID, ExternalDataLayerRootPath));
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> Builder;
	Builder += ExternalDataLayerRootPath;
	Builder += TEXT("/");
	Builder += InLevelPackagePath;
	FString Result = *Builder;
	FPaths::RemoveDuplicateSlashes(Result);
	return Result;
}

#if WITH_EDITOR

static FName GetExternalDataLayerUIDsAssetRegistryTag()
{
	static const FName ExternalDataLayerUIDsTag("ExternalDataLayerUIDs");
	return ExternalDataLayerUIDsTag;
}

void FExternalDataLayerHelper::AddAssetRegistryTags(FAssetRegistryTagsContext OutContext, const TArray<FExternalDataLayerUID>& InExternalDataLayerUIDs)
{
	if (InExternalDataLayerUIDs.Num() > 0)
	{
		FString ExternalDataLayerUIDsStr = FString::JoinBy(InExternalDataLayerUIDs, TEXT(","), [&](const FExternalDataLayerUID& ExternalDataLayerUID) { return ExternalDataLayerUID.ToString(); });
		OutContext.AddTag(UObject::FAssetRegistryTag(GetExternalDataLayerUIDsAssetRegistryTag(), ExternalDataLayerUIDsStr, UObject::FAssetRegistryTag::TT_Hidden));
	}
}

void FExternalDataLayerHelper::GetExternalDataLayerUIDs(const FAssetData& Asset, TArray<FExternalDataLayerUID>& OutExternalDataLayerUIDs)
{
	FString ExternalDataLayerUIDsStr;
	if (Asset.GetTagValue(GetExternalDataLayerUIDsAssetRegistryTag(), ExternalDataLayerUIDsStr))
	{
		TArray<FString> ExternalDataLayerUIDStrArray;
		ExternalDataLayerUIDsStr.ParseIntoArray(ExternalDataLayerUIDStrArray, TEXT(","));
		for (const FString& ExternalDataLayerUIDStr : ExternalDataLayerUIDStrArray)
		{
			FExternalDataLayerUID ExternalDataLayerUID;
			if (FExternalDataLayerUID::Parse(ExternalDataLayerUIDStr, ExternalDataLayerUID))
			{
				OutExternalDataLayerUIDs.Add(ExternalDataLayerUID);
			}
		}
	}
}

namespace UE::Private::ExternalDataLayerHelper
{
	static bool ValidateAssetUsingAssetReferenceRestrictions(const UObject* InAsset, TSet<FString>& OutInvalidReferenceReasons)
	{
		if (!InAsset)
		{
			return false;
		}

		TArray<UClass*> IgnoreClasses;
		TArray<UPackage*> IgnorePackages;
		TSet<UObject*> ReferencedAssets;
		FFindReferencedAssets::BuildAssetList(const_cast<UObject*>(InAsset), IgnoreClasses, IgnorePackages, ReferencedAssets);

		uint32 ErrorCount = 0;
		if (ReferencedAssets.Num() > 0)
		{
			FAssetData ReferencingAssetData = FAssetData(InAsset);
			FAssetReferenceFilterContext AssetReferenceFilterContext;
			AssetReferenceFilterContext.ReferencingAssets = { ReferencingAssetData };
			TSharedPtr<IAssetReferenceFilter> AssetReferenceFilter = GEditor ? GEditor->MakeAssetReferenceFilter(AssetReferenceFilterContext) : nullptr;
			if (ensure(AssetReferenceFilter.IsValid()))
			{
				for (UObject* ReferencedAsset : ReferencedAssets)
				{
					FText FailureReason;
					if (!AssetReferenceFilter->PassesFilter(FAssetData(ReferencedAsset), &FailureReason))
					{
						OutInvalidReferenceReasons.Add(FailureReason.ToString());
						++ErrorCount;
					}
				}
			}
		}
		return !ErrorCount;
	}
}

// Only meant to be used by FExternalDataLayerHelper on stack
struct FScopeRawAssignActorExternalDataLayer
{
private:
	FScopeRawAssignActorExternalDataLayer(AActor* InActor, const UExternalDataLayerAsset* InExternalDataLayerAsset)
		: Actor(InActor)
		, ExternalDataLayerAsset(InActor->ExternalDataLayerAsset)
	{
		check(Actor);
		Actor->ExternalDataLayerAsset = InExternalDataLayerAsset;
	}

	~FScopeRawAssignActorExternalDataLayer()
	{
		Actor->ExternalDataLayerAsset = ExternalDataLayerAsset;
	}

	AActor* Actor;
	const UExternalDataLayerAsset* ExternalDataLayerAsset;

	friend class FExternalDataLayerHelper;
};

bool FExternalDataLayerHelper::CanMoveActorsToExternalDataLayer(const TArray<AActor*>& InActors, const UExternalDataLayerInstance* InExternalDataLayerInstance, FText* OutFailureReason)
{
	auto CanMoveActorToExternalDataLayer = [](AActor* InActor, const UExternalDataLayerInstance* InExternalDataLayerInstance, FText& OutFailureReason)
	{
		check(!InActor->IsTemplate());
		check(InActor->GetLevel());

		const UExternalDataLayerAsset* NewExternalDataLayerAsset = InExternalDataLayerInstance ? InExternalDataLayerInstance->GetExternalDataLayerAsset() : nullptr;
		if (!InActor->IsPackageExternal())
		{
			OutFailureReason = FText::Format(LOCTEXT("CantMoveActorToEDL_NotPackageExternal", "Actor {0} is not using external package."), FText::FromString(InActor->GetName()));
			return false;
		}
		
		if (!InActor->IsUserManaged())
		{
			OutFailureReason = FText::Format(LOCTEXT("CantMoveActorToEDL_NotUserManaged", "Actor {0} cannot be manually modified."), FText::FromString(InActor->GetName()));
			return false;
		}

		if (!InActor->GetExternalDataLayerAsset() && !NewExternalDataLayerAsset)
		{
			OutFailureReason = FText::Format(LOCTEXT("CantMoveActorToEDL_NoExternalDataLayer", "Actor {0} has already no External Data Layer."), FText::FromString(InActor->GetName()));
			return false;
		}

		if (InActor->GetExternalDataLayerAsset() == NewExternalDataLayerAsset)
		{
			OutFailureReason = FText::Format(LOCTEXT("CantMoveActorToEDL_SameExternalDataLayer", "Actor {0} is already assigned to this External Data Layer."), FText::FromString(InActor->GetName()));
			return false;
		}

		if (InExternalDataLayerInstance && InExternalDataLayerInstance->IsReadOnly())
		{
			OutFailureReason = FText::Format(LOCTEXT("CantMoveActorToEDL_ReadOnlyExternalDataLayer", "External Data Layer is read-only."), FText::FromString(InExternalDataLayerInstance->GetDataLayerShortName()));
			return false;
		}

		UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
		if (!EditorActorSubsystem)
		{
			OutFailureReason = LOCTEXT("CantMoveActorToEDL_MissingEditorActorSubsystem", "Missing EditorActorSubsystem.");
			return false;
		}

		// Create a temporary transient copy of the actor
		AActor* DuplicatedActorToValidate = nullptr;
		{
			// Avoid triggering any dirty package notifications as this is a transient object
			TGuardValue<bool> IsEditorLoadingPackageGuard(GIsEditorLoadingPackage, true);
			{
				UWorld* World = InActor->GetWorld();
				// Temporarily set actor to target value for the duplication to use this value
				FScopeRawAssignActorExternalDataLayer ScopeSetActorExternalDataLayer(InActor, NewExternalDataLayerAsset);
				// Flag the temporary actor transient to make sure IsAsset returns false (to avoid considering this temporary actor by any other systems)
				FDelegateHandle PreSpawnDelegateHandle = World->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* DuplicatedActorToValidate) { DuplicatedActorToValidate->SetFlags(RF_Transient); }));
				DuplicatedActorToValidate = EditorActorSubsystem->DuplicateActor(InActor);
				World->RemoveOnActorPreSpawnInitialization(PreSpawnDelegateHandle);
			}
		}
		// Destroy the temporary actor
		ON_SCOPE_EXIT{ DuplicatedActorToValidate->GetWorld()->EditorDestroyActor(DuplicatedActorToValidate, false); };

		// Validate the temporary actor's asset references
		TSet<FString> InvalidReferenceReasons;
		if (!UE::Private::ExternalDataLayerHelper::ValidateAssetUsingAssetReferenceRestrictions(DuplicatedActorToValidate, InvalidReferenceReasons))
		{
			const FString JoinedReasons = FString::Join(InvalidReferenceReasons, TEXT("\n"));
			if (NewExternalDataLayerAsset)
			{
				OutFailureReason = FText::Format(LOCTEXT("CantMoveActorToEDLReferenceRestrictions", "Can't move Actor {0} to External Data Layer {1}. Reason: {2}."), FText::FromString(InActor->GetName()), FText::FromString(NewExternalDataLayerAsset->GetName()), FText::FromString(JoinedReasons));
			}
			else
			{
				const UExternalDataLayerAsset* OldExternalDataLayerAsset = InActor->GetExternalDataLayerAsset();
				check(OldExternalDataLayerAsset);
				OutFailureReason = FText::Format(LOCTEXT("CantRemoveEDLFromActorReferenceRestrictions", "Can't remove External Data Layer {0} from Actor {1}. Reason: {2}."), FText::FromString(OldExternalDataLayerAsset->GetName()), FText::FromString(InActor->GetName()), FText::FromString(JoinedReasons));
			}
			return false;
		}

		return true;
	};

	// Validate that all actors can change their External Data Layer asset
	for (AActor* Actor : InActors)
	{
		FText FailureReason;
		if (!CanMoveActorToExternalDataLayer(Actor, InExternalDataLayerInstance, FailureReason))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FailureReason;
			}
			return false;
		}
	}

	return true;
}

bool FExternalDataLayerHelper::MoveActorsToExternalDataLayer(const TArray<AActor*>& InActors, const UExternalDataLayerInstance* InExternalDataLayerInstance, FText* OutFailureReason)
{
	auto MoveActorToExternalDataLayer = [](AActor * InActor, const UExternalDataLayerInstance* InExternalDataLayerInstance)
	{
		const UExternalDataLayerAsset* NewExternalDataLayerAsset = InExternalDataLayerInstance ? InExternalDataLayerInstance->GetExternalDataLayerAsset() : nullptr;
		const bool bShouldDirty = true;
		bool bLevelPackageWasDirty = InActor->GetLevel()->GetPackage()->IsDirty();
		InActor->SetPackageExternal(false, bShouldDirty);

		// If set, remove EDL from actor
		const UExternalDataLayerAsset* OldExternalDataLayerAsset = InActor->GetExternalDataLayerAsset();
		if (OldExternalDataLayerAsset)
		{
			FAssignActorDataLayer::RemoveDataLayerAsset(InActor, OldExternalDataLayerAsset);
			for (const UDataLayerInstance* DataLayerInstance : InActor->GetDataLayerInstances())
			{
				if (DataLayerInstance->GetRootExternalDataLayerInstance())
				{
					FAssignActorDataLayer::RemoveDataLayerAsset(InActor, DataLayerInstance->GetAsset());
				}
			}
		}

		// If set, add actor to new EDL
		if (NewExternalDataLayerAsset)
		{
			FAssignActorDataLayer::AddDataLayerAsset(InActor, NewExternalDataLayerAsset);
		}

		InActor->SetPackageExternal(true, bShouldDirty);
		if (!bLevelPackageWasDirty)
		{
			InActor->GetLevel()->GetPackage()->SetDirtyFlag(false);
		}

		return (InActor->GetExternalDataLayerAsset() == NewExternalDataLayerAsset);
	};
	
	// First, validate that the whole operation can be done without any validation errors
	if (!CanMoveActorsToExternalDataLayer(InActors, InExternalDataLayerInstance, OutFailureReason))
	{
		return false;
	}

	// Change all actors External Data Layer asset
	for (AActor* Actor : InActors)
	{
		if (ensure(MoveActorToExternalDataLayer(Actor, InExternalDataLayerInstance)))
		{
			// Basic validation on the actor and its new External Data Layer asset
			UExternalDataLayerManager* ExternalDataLayerManager = UExternalDataLayerManager::GetExternalDataLayerManager(Actor);
			check(ExternalDataLayerManager->ValidateOnActorExternalDataLayerAssetChanged(Actor));

			// Notify actor's External Data Layer asset changed
			FProperty* ExternalDataLayerAssetChangeProperty = FindFProperty<FProperty>(Actor->GetClass(), "ExternalDataLayerAsset");
			FPropertyChangedEvent PropertyChangedEvent(ExternalDataLayerAssetChangeProperty);
			Actor->PostEditChangeProperty(PropertyChangedEvent);
		}
	}
	return true;
}

void FExternalDataLayerHelper::ForEachExternalDataLayerLevelPackagePath(const FString& InLevelPackageName, TFunctionRef<void(const FString&)> Func)
{
	UClass* GameFeatureDataClass = FindObject<UClass>(nullptr, TEXT("/Script/GameFeatures.GameFeatureData"));
	if (GameFeatureDataClass)
	{
		FARFilter Filter;
		Filter.bIncludeOnlyOnDiskAssets = false;
		Filter.ClassPaths = { GameFeatureDataClass->GetClassPathName() };
		Filter.bRecursivePaths = true;
		TArray<FAssetData> AssetsData;
		FExternalPackageHelper::GetSortedAssets(Filter, AssetsData);

		for (const FAssetData& AssetData : AssetsData)
		{
			const FString MountPoint = FPackageName::GetPackageMountPoint(AssetData.PackagePath.ToString()).ToString();

			TArray<FExternalDataLayerUID> ExternalDataLayerUIDs;
			FExternalDataLayerHelper::GetExternalDataLayerUIDs(AssetData, ExternalDataLayerUIDs);
			for (const FExternalDataLayerUID& ExternalDataLayerUID : ExternalDataLayerUIDs)
			{
				if (ExternalDataLayerUID.IsValid())
				{
					FString LevelPackageEDLPath = FExternalDataLayerHelper::GetExternalDataLayerLevelRootPath(MountPoint, ExternalDataLayerUID, InLevelPackageName);
					Func(LevelPackageEDLPath);
				}
			}
		}
	}
}

bool FExternalDataLayerHelper::IsExternalDataLayerPath(FStringView InExternalDataLayerPath, FExternalDataLayerUID* OutExternalDataLayerUID)
{
	int32 ExternalDataLayerFolderIdx = UE::String::FindFirst(InExternalDataLayerPath, GetExternalDataLayerFolder(), ESearchCase::IgnoreCase);
	if (ExternalDataLayerFolderIdx != INDEX_NONE)
	{
		FStringView RelativeExternalDataLayerPath = InExternalDataLayerPath.RightChop(ExternalDataLayerFolderIdx + GetExternalDataLayerFolder().Len());
		int32 ExternalDataLayerUIDEndIdx = UE::String::FindFirst(RelativeExternalDataLayerPath, TEXT("/"), ESearchCase::IgnoreCase);
		if (ExternalDataLayerUIDEndIdx != INDEX_NONE)
		{
			if (RelativeExternalDataLayerPath.RightChop(ExternalDataLayerUIDEndIdx + 1).Len() > 0) // + 1 to remove the "/"
			{
				FExternalDataLayerUID UID;
				const FString ExternalDataLayerUIDStr = FString(RelativeExternalDataLayerPath.Mid(0, ExternalDataLayerUIDEndIdx));
				return FExternalDataLayerUID::Parse(ExternalDataLayerUIDStr, OutExternalDataLayerUID ? *OutExternalDataLayerUID : UID);
			}
		}
	}
	return false;
}

#endif

#undef LOCTEXT_NAMESPACE