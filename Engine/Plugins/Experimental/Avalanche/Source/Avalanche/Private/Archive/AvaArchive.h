// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/AvaDataDefines.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/ObjectMacros.h"

struct FAvaObjectData;
struct FAvaWorldData;
class UObject;

class FAvaArchive : public FArchiveUObject
{
	using Super = FArchiveUObject;
	
public:
	
	FAvaArchive(FAvaWorldData& InWorldData
		, FAvaObjectData& InObjectData
		, UObject* InSerializedObject
		, bool bIsLoading);

	// FArchive Interface
	virtual FString GetArchiveName() const override;
	virtual int64 TotalSize() override;
	virtual int64 Tell() override;
	virtual void Seek(int64 InPos) override;
	virtual bool ShouldSkipProperty(const FProperty* InProperty) const override;
	virtual FArchive& operator<<(FName& Value) override;
	virtual FArchive& operator<<(UObject*& Value) override;
	virtual void Serialize(void* Data, int64 Length) override;
	// ~FArchive Interface

	/* Allocates and serializes an object dependency, or gets the object, if it already exists. */
	virtual UObject* ResolveObjectDependency(FAvaObjectIndex ObjectIndex) const = 0;
	
protected:

	FAvaWorldData& WorldData;

	FAvaObjectData& ObjectData;

	UObject* SerializedObject;

	/** Flags that would cause the Property to be excluded from the Archive */
	EPropertyFlags ExcludedPropertyFlags = EPropertyFlags::CPF_None;

	int64 DataIndex = 0;
};
