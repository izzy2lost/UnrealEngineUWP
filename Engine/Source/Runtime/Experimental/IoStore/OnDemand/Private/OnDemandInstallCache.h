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
	virtual				~IOnDemandInstallCache() = default;
	virtual bool		IsChunkCached(const FIoHash& ChunkHash) = 0;
	virtual FIoStatus	PutChunk(FIoBuffer&& Chunk, const FIoHash& ChunkHash) = 0;
	virtual FIoStatus	Purge(TMap<FIoHash, uint64>&& ChunksToInstall) = 0;
	virtual FIoStatus	Flush() = 0;
};

struct FOnDemandInstallCacheConfig
{
	FString RootDirectory;
	uint64	DiskQuota = 1ull << 30;
	bool	bDropCache = false;
};

TSharedPtr<IOnDemandInstallCache> MakeOnDemandInstallCache(
	FOnDemandIoStore& IoStore,
	const FOnDemandInstallCacheConfig& Config);

} // namespace UE::IoStore
