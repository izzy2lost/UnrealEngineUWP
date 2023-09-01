// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CancellationToken.h"
#include "IO/IoStatus.h"
#include "Memory/MemoryFwd.h"
#include "Tasks/Task.h"
#include "Templates/UniquePtr.h"

class FIoBuffer;
class FIoReadOptions;
struct FIoHash;

namespace UE::IO::IAS {

/** Cache for binary blobs with a 20 byte cache key. */
class IIasCache
{
public:
	virtual ~IIasCache() = default;

	/** Returns whether the specified cache key is present in the cache. */
	virtual bool ContainsChunk(const FIoHash& Key) const = 0;

	/** Get the chunk associated with the specified cache key. */
	virtual UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> Get(
		const FIoHash& Key,
		const FIoReadOptions& Options,
		const FIoCancellationToken* CancellationToken) = 0;

	/** Insert a new chunk into the cache. */
	virtual FIoStatus Put(const FIoHash& Key, FIoBuffer& Data) = 0;
};

struct FIasCacheConfig
{
	struct FRate
	{
		uint32	Allowance = 16 << 20;
		uint32	Ops = 32;
		uint32	Seconds = 60;
	};

	struct FDemand
	{
		uint8	Threshold = 30;
		uint8	Boost = 60;
		uint8	SuperBoost = 87;
	};

	const TCHAR*Name = TEXT("ias");
	uint64		DiskQuota = 512ull << 20;
	uint32		MemoryQuota = 2 << 20;
	uint32		JournalQuota = 4 << 20; // description in JournalCache.cpp
	uint32		JournalFlushInterval = 4;
	FRate		WriteRate;
	FDemand		Demand;
	bool		DropCache = false;
};

TUniquePtr<IIasCache> MakeIasCache(const FIasCacheConfig& Config);

} // namespace UE::IO::IAS
