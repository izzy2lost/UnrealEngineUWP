// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionPropertyOverrideSerialization.h"

#if WITH_EDITOR

#include "WorldPartition/WorldPartitionSettings.h"
#include "WorldPartition/WorldPartitionPropertyOverride.h"

FWorldPartitionPropertyOverrideArchive::FWorldPartitionPropertyOverrideArchive(FArchive& InArchive)
	: FObjectAndNameAsStringProxyArchive(InArchive, /*bLoadIfFindFails*/true)
{
	check(InArchive.IsPersistent());
	check(!InArchive.IsFilterEditorOnly());
	check(InArchive.ShouldSkipBulkData());
	check(!InArchive.WantBinaryPropertySerialization());

	SetIsLoading(InArchive.IsLoading());
	SetIsSaving(InArchive.IsSaving());
	SetIsTextFormat(InArchive.IsTextFormat());
	SetWantBinaryPropertySerialization(InArchive.WantBinaryPropertySerialization());
	SetIsPersistent(true);
	FArchiveProxy::SetFilterEditorOnly(InArchive.IsFilterEditorOnly());
	ArShouldSkipBulkData = true;
	PropertyOverridePolicy = UWorldPartitionSettings::Get()->GetPropertyOverridePolicy();
}

bool FWorldPartitionPropertyOverrideArchive::ShouldSkipProperty(const FProperty* InProperty) const
{
	if (PropertyOverridePolicy)
	{
		return !PropertyOverridePolicy->CanOverrideProperty(InProperty);
	}

	return true;
}

FArchive& FWorldPartitionPropertyOverrideArchive::operator<<(FLazyObjectPtr& Value)
{ 
	return FArchiveUObject::SerializeLazyObjectPtr(*this, Value); 
}

FWorldPartitionPropertyOverrideWriter::FWorldPartitionPropertyOverrideWriter(TArray<uint8, TSizedDefaultAllocator<32>>& InBytes)
	: FMemoryWriter(InBytes, true)
{
	SetFilterEditorOnly(false);
	ArShouldSkipBulkData = true;
	SetIsTextFormat(false);
	SetWantBinaryPropertySerialization(false);
}

FWorldPartitionPropertyOverrideReader::FWorldPartitionPropertyOverrideReader(const TArray<uint8>& InBytes)
	: FMemoryReader(InBytes, true)
{
	SetFilterEditorOnly(false);
	ArShouldSkipBulkData = true;
	SetIsTextFormat(false);
	SetWantBinaryPropertySerialization(false);
}

#endif

