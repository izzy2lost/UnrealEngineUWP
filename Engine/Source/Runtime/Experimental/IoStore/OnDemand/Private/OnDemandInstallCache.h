// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoDispatcherBackend.h"
#include "Templates/SharedPointer.h"
#include "Containers/StringFwd.h"

struct FIoContainerHeader; 

namespace UE::IoStore
{

class FOnDemandIoStore;
struct FOnDemandContainer;

class IOnDemandInstallCache 
	: public IIoDispatcherBackend
{
public:
	virtual ~IOnDemandInstallCache() = default;
	virtual bool ContainsChunk(const FIoHash& Hash) = 0;
	virtual FIoStatus PutChunk(FIoBuffer&& Chunk, const FIoHash& Hash) = 0;
};

struct FOnDemandInstallCacheConfig
{
	FString RootDirectory;
	bool bDropCache = false;
};

TSharedPtr<IOnDemandInstallCache> MakeOnDemandInstallCache(
	FOnDemandIoStore& IoStore,
	const FOnDemandInstallCacheConfig& Config);

} // namespace UE::IoStore
