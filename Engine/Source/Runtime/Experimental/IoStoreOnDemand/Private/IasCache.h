// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CancellationToken.h"
#include "Containers/StringView.h"
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
	using FGetToken = UPTRINT;
	using FGetWork = UE::Tasks::TTask<TIoStatusOr<FIoBuffer>>;

	virtual ~IIasCache() = default;

	/** Deletes the IAS object, dropping all data persisted to disk and releasing
	OS resources. As this also deletes the object thus any unique pointers should
	be released prior to abandonment; TUniquePtr->Release()->Abandon(). Be sure
	to cancel and collect any Get() tasks beforehand. */
	virtual void Abandon() = 0;

	/** Returns whether the specified cache key is present in the cache. */
	virtual bool ContainsChunk(const FIoHash& Key) const = 0;

	/** Get the chunk associated with the specified cache key. If the data is
	 already in memory it is return in OutData. Otherwise a FGetToken value is
	 returned; zero if Key is not found, or non-zero for use with Materialize. */
	virtual FGetToken Get(const FIoHash& Key, FIoBuffer& OutData) = 0;

	/** Materialize the data for a Get() if it was not immediately available */
	virtual FGetWork Materialize(
		FGetToken Token,
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

	FStringView Name = TEXT("ias");
	uint64		DiskQuota = 512ull << 20;
	uint32		MemoryQuota = 2 << 20;
	uint32		JournalQuota = 4 << 20; // description in JournalCache.cpp
	FRate		WriteRate;
	FDemand		Demand;
	bool		DropCache = false;
};

TUniquePtr<IIasCache> MakeIasCache(const TCHAR* RootDir, const FIasCacheConfig& Config);

} // namespace UE::IO::IAS
