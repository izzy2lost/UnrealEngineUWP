// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Async.h"
#include "Async/Mutex.h"
#include "Containers/AnsiString.h"
#include "IO/IoStoreOnDemand.h"
#include "IO/IoHash.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoChunkId.h"
#include "Misc/AES.h"
#include "Misc/EnumClassFlags.h"

namespace UE::IoStore
{

class IOnDemandPackageStoreBackend;
class IOnDemandInstallCache;
using FSharedPackageStoreBackend	= TSharedPtr<IOnDemandPackageStoreBackend>;
using FSharedInstallCache			= TSharedPtr<IOnDemandInstallCache>;

///////////////////////////////////////////////////////////////////////////////
enum class EOnDemandContainerFlags : uint8
{
	None					= 0,
	PendingEncryptionKey	= (1 << 0),
	Mounted					= (1 << 1),
	Streaming				= (1 << 2),
	Installed				= (1 << 3),
	Count
};
ENUM_CLASS_FLAGS(EOnDemandContainerFlags);

void LexToString(EOnDemandContainerFlags Flags, FStringBuilderBase& Out);
FString LexToString(EOnDemandContainerFlags Flags);

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandChunkEntry
{
	static const FOnDemandChunkEntry Null;

	FIoHash	Hash;
	uint32	RawSize = 0;
	uint32	EncodedSize = 0;
	uint32	BlockOffset = ~uint32(0);
	uint32	BlockCount = 0; 
	uint8	CompressionFormatIndex = 0;
};

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandContainer
{
	using FChunkEntryMap = TMap<FIoChunkId, FOnDemandChunkEntry>;

	FAES::FAESKey			EncryptionKey;
	FIoHash					PakHash;
	FChunkEntryMap 			ChunkEntries;
	FString					EncryptionKeyGuid;
	FString					Name;
	FString					MountId;
	FAnsiString				ChunksDirectory;
	TArray<FName>			CompressionFormats;
	TArray<uint32>			BlockSizes;
	TArray<FIoBlockHash>	BlockHashes;
	FIoContainerId			ContainerId;
	uint32					BlockSize = 0;
	EOnDemandContainerFlags Flags = EOnDemandContainerFlags ::None;

	FString					UniqueName() const;
};

using FSharedOnDemandContainer = TSharedPtr<FOnDemandContainer, ESPMode::ThreadSafe>;

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandChunkInfo
{
	FOnDemandChunkInfo()
		: Entry(FOnDemandChunkEntry::Null)
	{ }

	const									FIoHash& Hash() const { return Entry.Hash; }
	uint32									RawSize() const { return Entry.RawSize; }
	uint32									EncodedSize() const { return Entry.EncodedSize; }
	uint32									BlockSize() const { return SharedContainer->BlockSize; }
	FName									CompressionFormat() const { return SharedContainer->CompressionFormats[Entry.CompressionFormatIndex]; }
	FMemoryView								EncryptionKey() const { return FMemoryView(SharedContainer->EncryptionKey.Key, FAES::FAESKey::KeySize); }
	inline TConstArrayView<uint32>			Blocks() const;
	inline TConstArrayView<FIoBlockHash>	BlockHashes() const;
	FAnsiStringView							ChunksDirectory() const { return SharedContainer->ChunksDirectory; }

	bool									IsValid() const { return SharedContainer.IsValid(); }
	operator								bool() const { return IsValid(); }

private:
	friend class FOnDemandIoStore;

	FOnDemandChunkInfo(FSharedOnDemandContainer InContainer, const FOnDemandChunkEntry& InEntry)
		: SharedContainer(InContainer)
		, Entry(InEntry)
	{ }

	FSharedOnDemandContainer	SharedContainer;
	const FOnDemandChunkEntry&	Entry;
};

TConstArrayView<uint32> FOnDemandChunkInfo::Blocks() const
{
	return TConstArrayView<uint32>(SharedContainer->BlockSizes.GetData() + Entry.BlockOffset, Entry.BlockCount);
}

TConstArrayView<FIoBlockHash> FOnDemandChunkInfo::BlockHashes() const
{
	return SharedContainer->BlockHashes.IsEmpty()
		? TConstArrayView<FIoBlockHash>()
		: TConstArrayView<FIoBlockHash>(SharedContainer->BlockHashes.GetData() + Entry.BlockOffset, Entry.BlockCount);
}

///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoStore
{
	struct FMountRequest
	{
		FOnDemandMountArgs					MountArgs;
		FOnDemandMountCompleted				OnCompleted;
		TArray<FSharedOnDemandContainer>	Containers;
		bool								bCancelled = false;
	};

	using FSharedMountRequest	= TSharedPtr<FMountRequest>;
	using FMountRequestMap		= TMap<FString, FSharedMountRequest>;

public:
	FOnDemandIoStore();
	~FOnDemandIoStore();
	FOnDemandIoStore(const FOnDemandIoStore&) = delete;
	FOnDemandIoStore(FOnDemandIoStore&&) = delete;
	FOnDemandIoStore& operator=(const FOnDemandIoStore&) = delete;
	FOnDemandIoStore& operator=(FOnDemandIoStore&&) = delete;

	FIoStatus				Initialize();
	void					Mount(FOnDemandMountArgs&& Args, FOnDemandMountCompleted&& OnCompleted);
	FIoStatus				Unmount(FStringView MountId);
	FOnDemandChunkInfo		GetStreamingChunkInfo(const FIoChunkId& ChunkId);
	FOnDemandChunkInfo		GetInstalledChunkInfo(const FIoChunkId& ChunkId);

private:
	FOnDemandChunkInfo		GetChunkInfo(const FIoChunkId& ChunkId, EOnDemandContainerFlags ContainerFlags);
	void					TryEnterTickLoop();
	void					TickLoop();
	bool					Tick();
	FIoStatus				TickMountRequest(FMountRequest& MountRequest);
	void					ConditionallyStartTicking();
	void					OnEncryptionKeyAdded(const FGuid& Id, const FAES::FAESKey& Key);
	static void				CreateContainersFromToc(
								FStringView MountId,
								FStringView TocPath,
								FOnDemandToc& Toc,
								TArray<FSharedOnDemandContainer>& Out);
	FIoStatus				InstallContainers(
								const FString& Url,
								const TConstArrayView<FSharedOnDemandContainer>& ContainersToInstall);

	TArray<FSharedOnDemandContainer> GetMountedContainers();

	FSharedInstallCache					InstallCache;
	FSharedPackageStoreBackend			PackageStoreBackend;
	FDelegateHandle						OnMountPakHandle;
	TArray<FSharedOnDemandContainer>	Containers;
	UE::FMutex							ContainerMutex;

	FMountRequestMap					MountRequests;
	UE::FMutex							MountRequestMutex;

	bool								bTicking = false;
	bool								bTickRequested = false;
	TFuture<void>						TickFuture;
};

} // namespace UE::IoStore
