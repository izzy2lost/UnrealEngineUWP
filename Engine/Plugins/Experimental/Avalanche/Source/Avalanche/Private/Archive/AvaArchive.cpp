// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaArchive.h"
#include "AvaBlueprint.h"
#include "AvaBlueprint_Serialize.h"
#include "HAL/UnrealMemory.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "AvaArchive"

DEFINE_LOG_CATEGORY_STATIC(LogAvaArchive, Log, All);

FAvaArchive::FAvaArchive(FAvaWorldData& InWorldData, FAvaObjectData& InObjectData, UObject* InSerializedObject, bool bIsLoading)
	: WorldData(InWorldData)
	, ObjectData(InObjectData)
	, SerializedObject(InSerializedObject)
	, ExcludedPropertyFlags(CPF_BlueprintAssignable | CPF_Transient | CPF_Deprecated)
{
	Super::SetWantBinaryPropertySerialization(false);
	Super::SetIsTransacting(false);
	Super::SetIsPersistent(true);

	ArNoDelta = true;

	if (bIsLoading)
	{
		// Serialize properties that were valid in a previous version and are deprecated now. PostSerialize is responsible to migrate the data.
		ExcludedPropertyFlags &= ~CPF_Deprecated;
		SetPortFlags(PPF_UseDeprecatedProperties);

		Super::SetIsLoading(true);
		Super::SetIsSaving(false);

		WorldData.VersionInfo.ApplyToArchive(*this);
	}
	else
	{
		Super::SetIsLoading(false);
		Super::SetIsSaving(true);
	}
}

FString FAvaArchive::GetArchiveName() const
{
	return TEXT("FAvaArchive");
}

int64 FAvaArchive::TotalSize()
{
	return ObjectData.SerializedData.Num();
}

int64 FAvaArchive::Tell()
{
	return DataIndex;
}

void FAvaArchive::Seek(int64 InPos)
{
	checkSlow(InPos <= TotalSize());
	DataIndex = InPos;
}

bool FAvaArchive::ShouldSkipProperty(const FProperty* InProperty) const
{
	check(InProperty);

	if (InProperty->HasAnyPropertyFlags(ExcludedPropertyFlags))
	{
		return true;
	}

	return false;
}

FArchive& FAvaArchive::operator<<(FName& Value)
{
	if (IsLoading())
	{
		FAvaObjectIndex NameIndex;
		*this << NameIndex.Index;

		if (!ensureAlwaysMsgf(WorldData.SerializedNames.IsValidIndex(NameIndex.Index), TEXT("Data appears to be corrupted")))
		{
			SetError();
			return *this;
		}
		Value = WorldData.SerializedNames[NameIndex.Index];
	}
	else
	{
		FAvaObjectIndex NameIndex;
		if (const FAvaObjectIndex* ExistingIndex = WorldData.NameIndexMap.Find(Value))
		{
			NameIndex = *ExistingIndex;
		}
		else
		{
			NameIndex.Index = WorldData.SerializedNames.Add(Value);
			WorldData.NameIndexMap.Add(Value, NameIndex);
		}

		*this << NameIndex.Index;
	}
	return *this;
}

FArchive& FAvaArchive::operator<<(UObject*& Value)
{
	if (IsLoading())
	{
		FAvaObjectIndex ReferencedIndex;
		*this << ReferencedIndex.Index;

		if (!ensureAlwaysMsgf(WorldData.SerializedObjectReferences.IsValidIndex(ReferencedIndex.Index)
			, TEXT("Data appears to be corrupted")))
		{
			SetError();
			return *this;
		}

		const FSoftObjectPath& ObjectPath = WorldData.SerializedObjectReferences[ReferencedIndex.Index];
		if (ObjectPath.IsNull())
		{
			Value = nullptr;
			return *this;
		}

		Value = ResolveObjectDependency(ReferencedIndex);
	}
	else
	{
		FAvaObjectIndex ReferenceIndex = FAvaBlueprint_Serialize::AddObjectDependency(WorldData, Value);
		*this << ReferenceIndex.Index;
	}

	return *this;
}

void FAvaArchive::Serialize(void* Data, int64 Length)
{
	if (Length <= 0)
	{
		return;
	}

	if (IsLoading())
	{
		if (!ensure(DataIndex + Length <= TotalSize()))
		{
			UE_LOG(LogAvaArchive, Error, TEXT("Unable to read %d bytes at index %d (Archive size: %d), missing %d bytes."),
				Length, DataIndex, TotalSize(), DataIndex + Length - TotalSize());
			SetError();
			return;
		}

		FMemory::Memcpy(Data, &ObjectData.SerializedData[DataIndex], Length);
		DataIndex += Length;
	}
	else
	{
		const int64 RequiredEndIndex = DataIndex + Length;
		const int32 ToAlloc  = RequiredEndIndex - TotalSize();
		if (ToAlloc > 0)
		{
			ObjectData.SerializedData.AddUninitialized(ToAlloc);
		}

		FMemory::Memcpy(&ObjectData.SerializedData[DataIndex], Data, Length);
		DataIndex = RequiredEndIndex;
	}
}

#undef LOCTEXT_NAMESPACE
