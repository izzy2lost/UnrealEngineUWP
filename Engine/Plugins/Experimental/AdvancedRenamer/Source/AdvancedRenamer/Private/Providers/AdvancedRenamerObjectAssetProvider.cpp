// Copyright Epic Games, Inc. All Rights Reserved.

#include "Providers/AdvancedRenamerObjectAssetProvider.h"
#include "AssetRegistry/AssetData.h"

void UAdvancedRenamerAssetProvider::BP_SetAssetList_Implementation(const TArray<UObject*>& InAssetList)
{
	AssetList.Empty();

	BP_SetAssetList(InAssetList);
}

void UAdvancedRenamerAssetProvider::BP_AddAssetList_Implementation(const TArray<UObject*>& InAssetList)
{
	for (UObject* Object : InAssetList)
	{
		BP_AddAssetData(Object);
	}
}

void UAdvancedRenamerAssetProvider::BP_AddAssetData_Implementation(UObject* InAsset)
{
	AddAssetData(FAssetData(InAsset, true));
}

UObject* UAdvancedRenamerAssetProvider::BP_GetAsset_Implementation(int32 Index) const
{
	return GetAsset(Index);
}

int32 UAdvancedRenamerAssetProvider::Num() const
{
	return BP_Num();
}

bool UAdvancedRenamerAssetProvider::ExecuteRename(int32 Index, const FString& NewName)
{
	return BP_ExecuteRename(Index, NewName);
}

bool UAdvancedRenamerAssetProvider::CanRename(int32 Index) const
{
	return BP_CanRename(Index);
}

bool UAdvancedRenamerAssetProvider::RemoveIndex(int32 Index)
{
	return BP_RemoveIndex(Index);
}

FString UAdvancedRenamerAssetProvider::GetOriginalName(int32 Index) const
{
	return BP_GetOriginalName(Index);
}

uint32 UAdvancedRenamerAssetProvider::GetHash(int32 Index) const
{
	return static_cast<uint32>(BP_GetHash(Index));
}

bool UAdvancedRenamerAssetProvider::IsValidIndex(int32 Index) const
{
	return BP_IsValidIndex(Index);
}
