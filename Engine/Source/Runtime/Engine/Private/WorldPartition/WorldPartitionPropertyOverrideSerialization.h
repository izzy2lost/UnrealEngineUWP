// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreFwd.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

class UWorldPartitionPropertyOverridePolicy;

class FWorldPartitionPropertyOverrideArchive : public FObjectAndNameAsStringProxyArchive
{
public:
	FWorldPartitionPropertyOverrideArchive(FArchive& InArchive);

	virtual bool ShouldSkipProperty(const FProperty* InProperty) const override;
	
	virtual FArchive& operator<<(FLazyObjectPtr& Value) override;

private:
	UWorldPartitionPropertyOverridePolicy* PropertyOverridePolicy = nullptr;
};

class FWorldPartitionPropertyOverrideWriter : public FMemoryWriter
{
public:
	FWorldPartitionPropertyOverrideWriter(TArray<uint8, TSizedDefaultAllocator<32>>& InBytes);
};

class FWorldPartitionPropertyOverrideReader : public FMemoryReader
{
public:
	FWorldPartitionPropertyOverrideReader(const TArray<uint8>& InBytes);
};

#endif