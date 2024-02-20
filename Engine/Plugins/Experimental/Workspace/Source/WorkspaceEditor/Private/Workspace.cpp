// Copyright Epic Games, Inc. All Rights Reserved.

#include "Workspace.h"

#include "WorkspaceSchema.h"
#include "WorkspaceState.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

const FName UWorkspace::ExportsAssetRegistryTag = TEXT("Exports");

bool UWorkspace::AddAsset(const FAssetData& InAsset, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(!IsAssetSupported(InAsset))
	{
		ReportError(TEXT("UWorkspace::AddAsset: Unsupported asset supplied."));
		return false;
	}

	if(bSetupUndoRedo)
	{
		Modify();
	}

	const int32 NewIndex = Assets.AddUnique(TSoftObjectPtr<UObject>(InAsset.GetSoftObjectPath()));
	if(NewIndex != INDEX_NONE)
	{
		BroadcastModified();
	}

	return NewIndex != INDEX_NONE;
}

bool UWorkspace::AddAsset(UObject* InAsset, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAsset == nullptr)
	{
		ReportError(TEXT("UWorkspace::AddAsset: Invalid asset supplied."));
		return false;
	}

	return AddAsset(FAssetData(InAsset), bSetupUndoRedo, bPrintPythonCommand);
}

bool UWorkspace::AddAssets(TConstArrayView<FAssetData> InAssets, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAssets.Num() == 0)
	{
		ReportError(TEXT("UWorkspace::AddAssets: No assets supplied."));
		return false;
	}

	bool bAdded = false;
	{
		TGuardValue<bool> DisableNotifications(bSuspendNotifications, true);
		for(const FAssetData& Asset : InAssets)
		{
			bAdded |= AddAsset(Asset, bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	if(bAdded)
	{
		BroadcastModified();
	}

	return bAdded;
}

bool UWorkspace::AddAssets(const TArray<UObject*>& InAssets, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAssets.Num() == 0)
	{
		ReportError(TEXT("UWorkspace::AddAssets: No assets supplied."));
		return false;
	}

	bool bAdded = false;
	{
		TGuardValue<bool> DisableNotifications(bSuspendNotifications, true);
		for(UObject* Asset : InAssets)
		{
			bAdded |= AddAsset(FAssetData(Asset), bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	if(bAdded)
	{
		BroadcastModified();
	}

	return bAdded;
}

bool UWorkspace::RemoveAsset(const FAssetData& InAsset, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(!IsAssetSupported(InAsset))
	{
		ReportError(TEXT("UWorkspace::RemoveAsset: Unsupported asset supplied."));
		return false;
	}

	if(bSetupUndoRedo)
	{
		Modify();
	}

	const int32 NumRemoved = Assets.Remove(TSoftObjectPtr<UObject>(InAsset.GetSoftObjectPath()));
	if(NumRemoved > 0)
	{
		BroadcastModified();
	}

	return NumRemoved > 0;
}

bool UWorkspace::RemoveAsset(UObject* InAsset, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAsset == nullptr)
	{
		ReportError(TEXT("UWorkspace::RemoveAsset: Invalid asset supplied."));
		return false;
	}

	return RemoveAsset(FAssetData(InAsset), bSetupUndoRedo, bPrintPythonCommand);
}

bool UWorkspace::RemoveAssets(TArray<UObject*> InAssets, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAssets.Num() == 0)
	{
		ReportError(TEXT("UWorkspace::RemoveAssets: No assets supplied."));
		return false;
	}

	bool bRemoved = false;
	{
		TGuardValue<bool> DisableNotifications(bSuspendNotifications, true);
		for(UObject* Asset : InAssets)
		{
			bRemoved |= RemoveAsset(FAssetData(Asset), bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	if(bRemoved)
	{
		BroadcastModified();
	}

	return bRemoved;
}

bool UWorkspace::RemoveAssets(TConstArrayView<FAssetData> InAssets, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAssets.Num() == 0)
	{
		ReportError(TEXT("UWorkspace::RemoveAssets: No assets supplied."));
		return false;
	}

	bool bRemoved = false;
	{
		TGuardValue<bool> DisableNotifications(bSuspendNotifications, true);
		for(const FAssetData& Asset : InAssets)
		{
			bRemoved |= RemoveAsset(Asset, bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	if(bRemoved)
	{
		BroadcastModified();
	}

	return bRemoved;
}

bool UWorkspace::IsAssetSupported(const FAssetData& InAsset)
{
	TConstArrayView<FTopLevelAssetPath> SupportedAssets = GetSchema()->GetSupportedAssetClassPaths();
	return SupportedAssets.IsEmpty() || SupportedAssets.Contains(InAsset.AssetClassPath);
}

UWorkspaceSchema* UWorkspace::GetSchema() const
{
	check(SchemaClass);
	return SchemaClass->GetDefaultObject<UWorkspaceSchema>();
}

void UWorkspace::LoadState()
{
	GetState()->LoadFromJson(this);
}

void UWorkspace::SaveState()
{
	GetState()->SaveToJson(this);
}

UWorkspaceState* UWorkspace::GetState() const
{
	if(State == nullptr)
	{
		State = NewObject<UWorkspaceState>(const_cast<UWorkspace*>(this));
	}

	return State;
}

void UWorkspace::BroadcastModified()
{
	if(!bSuspendNotifications)
	{
		ModifiedDelegate.Broadcast(this);
	}
}

void UWorkspace::ReportError(const TCHAR* InMessage) const
{
#if WITH_EDITOR
	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, InMessage, TEXT(""));
#endif
}

void UWorkspace::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	BroadcastModified();
}

void UWorkspace::PostLoad()
{
	Super::PostLoad();

	if(GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimNextMoveWorkspaces)
	{
		Guid = FGuid::NewGuid();
		SchemaClass = StaticLoadClass(UWorkspaceSchema::StaticClass(), nullptr, TEXT("/Script/AnimNextEditor.AnimNextWorkspaceSchema"));
	}
}

void UWorkspace::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	Guid = FGuid::NewGuid();
}

void UWorkspace::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	FWorkspaceAssetRegistryExports Exports;
	Exports.Assets.Reserve(Assets.Num());

	for(const TSoftObjectPtr<UObject>& Asset : Assets)
	{
		Exports.Assets.Emplace(Asset.GetUniqueID());
	}

	FString TagValue;
	FWorkspaceAssetRegistryExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);

	Context.AddTag(FAssetRegistryTag(ExportsAssetRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));
}
