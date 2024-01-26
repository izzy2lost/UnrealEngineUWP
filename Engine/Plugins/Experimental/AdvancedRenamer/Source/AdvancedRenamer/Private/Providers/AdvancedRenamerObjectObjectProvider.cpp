// Copyright Epic Games, Inc. All Rights Reserved.

#include "Providers/AdvancedRenamerObjectObjectProvider.h"

void UAdvancedRenamerObjectObjectProvider::BP_SetObjectList_Implementation(const TArray<UObject*>& InObjectList)
{
	ObjectList.Empty();

	BP_AddObjectList(InObjectList);
}

void UAdvancedRenamerObjectObjectProvider::BP_AddObjectList_Implementation(const TArray<UObject*>& InObjectList)
{
	for (UObject* Object : InObjectList)
	{
		BP_AddObjectData(Object);
	}
}

void UAdvancedRenamerObjectObjectProvider::BP_AddObjectData_Implementation(UObject* InObject)
{
	AddObjectData(InObject);
}

UObject* UAdvancedRenamerObjectObjectProvider::BP_GetObject_Implementation(int32 Index) const
{
	return GetObject(Index);
}

int32 UAdvancedRenamerObjectObjectProvider::Num() const
{
	return BP_Num();
}

bool UAdvancedRenamerObjectObjectProvider::ExecuteRename(int32 Index, const FString& NewName)
{
	return BP_ExecuteRename(Index, NewName);
}

bool UAdvancedRenamerObjectObjectProvider::CanRename(int32 Index) const
{
	return BP_CanRename(Index);
}

bool UAdvancedRenamerObjectObjectProvider::RemoveIndex(int32 Index)
{
	return BP_RemoveIndex(Index);
}

FString UAdvancedRenamerObjectObjectProvider::GetOriginalName(int32 Index) const
{
	return BP_GetOriginalName(Index);
}

uint32 UAdvancedRenamerObjectObjectProvider::GetHash(int32 Index) const
{
	return static_cast<uint32>(BP_GetHash(Index));
}

bool UAdvancedRenamerObjectObjectProvider::IsValidIndex(int32 Index) const
{
	return BP_IsValidIndex(Index);
}
