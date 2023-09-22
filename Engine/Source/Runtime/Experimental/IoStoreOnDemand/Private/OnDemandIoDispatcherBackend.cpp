// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandIoDispatcherBackend.h"

#include "AnalyticsEventAttribute.h"
#include "CancellationToken.h"
#include "Containers/BitArray.h"
#include "Containers/StringView.h"
#include "CoreHttp/LatencyTesting.h"
#include "DistributionEndpoints.h"
#include "EncryptionKeyManager.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "HAL/Event.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/Platform.h"
#include "HAL/PlatformTime.h"
#include "HAL/PreprocessorHelpers.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformFileManager.h"
#include "HttpManager.h"
#include "IO/IoAllocators.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoDispatcher.h"
#include "IO/IoOffsetLength.h"
#include "IO/IoStatus.h"
#include "IO/IoStore.h"
#include "IO/IoStoreOnDemand.h"
#include "IasCache.h"
#include "Math/NumericLimits.h"
#include "Misc/CommandLine.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "OnDemandHttpClient.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/MemoryReader.h"
#include "Statistics.h"
#include "Tasks/Task.h"

#include <atomic>

#if UE_IAS_LINKPAKFILE
#include "IPlatformFilePak.h"
#endif //UE_IAS_LINKPAKFILE

#if !UE_BUILD_SHIPPING
#include "Modules/ModuleManager.h"
#endif 

/** 
 * When enabledOnDemandToc files generated from disk will be compared to the downloaded version
 * and checked for compatibility issues. These checks will assert if a problem is found.
 */
#define UE_VALIDATE_GENERATED_TOC (0 && !UE_BUILD_SHIPPING)

namespace UE::IO::IAS
{

///////////////////////////////////////////////////////////////////////////////
int32 GIasMaxHttpConnectionCount = 8;
static FAutoConsoleVariableRef CVar_IasMaxHttpConnectionCount(
	TEXT("ias.MaxHttpConnectionCount"),
	GIasMaxHttpConnectionCount,
	TEXT("Max number of open HTTP connections to the on demand endpoint(s).")
);

int32 GIasMaxHttpRetryCount = 2;
static FAutoConsoleVariableRef CVar_IasMaxHttpRetryCount(
	TEXT("ias.MaxHttpRetryCount"),
	GIasMaxHttpRetryCount,
	TEXT("Max number of HTTP request retries before failing the I/O request.")
);

int32 GIasHttpHealthCheckWaitTime = 3000;
static FAutoConsoleVariableRef CVar_IasHttpHealthCheckWaitTime(
	TEXT("ias.HttpHealthCheckWaitTime"),
	GIasHttpHealthCheckWaitTime,
	TEXT("Number of milliseconds to wait before reconnecting to avaiable endpoint(s)")
);

int32 GIasMaxEndpointTestCountAtStartup = 1;
static FAutoConsoleVariableRef CVar_IasMaxEndpointTestCountAtStartup(
	TEXT("ias.MaxEndpointTestCountAtStartup"),
	GIasMaxEndpointTestCountAtStartup,
	TEXT("Number of endpoint(s) to test at startup")
);

int32 GIasHttpErrorSampleCount = 8;
static FAutoConsoleVariableRef CVar_IasHttpErrorSampleCount(
	TEXT("ias.HttpErrorSampleCount"),
	GIasHttpErrorSampleCount,
	TEXT("Number of samples for computing the moving average of failed HTTP requests")
);

float GIasHttpErrorHighWater = 0.5f;
static FAutoConsoleVariableRef CVar_IasHttpErrorHighWater(
	TEXT("ias.HttpErrorHighWater"),
	GIasHttpErrorHighWater,
	TEXT("High water mark when HTTP streaming will be disabled")
);

bool GIasHttpEnabled = true;
static FAutoConsoleVariableRef CVar_IasHttpEnabled(
	TEXT("ias.HttpEnabled"),
	GIasHttpEnabled,
	TEXT("Enables individual asset streaming via HTTP")
);

bool GIasHttpOptionalBulkDataEnabled = true;
static FAutoConsoleVariableRef CVar_IasHttpOptionalBulkDataEnabled(
	TEXT("ias.HttpOptionalBulkDataEnabled"),
	GIasHttpOptionalBulkDataEnabled,
	TEXT("Enables optional bulk data via HTTP")
);

static TAutoConsoleVariable<bool> CVar_IoReportAnalytics(
	TEXT("ias.ReportAnalytics"),
	true,
	TEXT("Enables reporting statics to the analytics system"));

static FAutoConsoleVariable CVar_IasGenerateOnDemandToc(
	TEXT("s.IasGenerateOnDemandToc"),
	false,
	TEXT("Enables generating the FOnDemandToc from utoc files on disk rather than downloading them"),
	ECVF_ReadOnly
);

static FAutoConsoleVariable CVar_IasEnableAsyncTocGeneration(
	TEXT("s.IasEnableThreadedTocGeneration"),
	true,
	TEXT("Enables pushing the work FOnDemandToc generation work to the task system"),
	ECVF_ReadOnly
);

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommand CVar_IasAbandonCache(
	TEXT("Ias.AbandonCache"),
	TEXT("Abandon the local file cache"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		FIoStoreOnDemandModule& Module = FModuleManager::Get().GetModuleChecked<FIoStoreOnDemandModule>("IoStoreOnDemand");
		Module.AbandonCache();
	})
);
#endif //!UE_BUILD_SHIPPING
///////////////////////////////////////////////////////////////////////////////
#if !UE_BUILD_SHIPPING
static void LatencyTest(FStringView Url, FStringView Path)
{
	int32 Results[4] = {};
	UE::IO::IAS::HTTP::LatencyTest(Url, Path, MakeArrayView(Results));
	UE_LOG(LogIas, Log, TEXT("Endpoint '%s' latency test (ms): %d %d %d %d"),
		Url.GetData(), Results[0], Results[1], Results[2], Results[3]);
}
#endif // !UE_BUILD_SHIPPING
///////////////////////////////////////////////////////////////////////////////
static int32 LatencyTest(TConstArrayView<FString> Urls, FStringView Path, std::atomic_bool& bCancel)
{
	for (int32 Idx = 0; Idx < Urls.Num() && !bCancel.load(std::memory_order_relaxed); ++Idx)
	{
		int32 LatencyMs = -1;
		UE::IO::IAS::HTTP::LatencyTest(Urls[Idx], Path, MakeArrayView(&LatencyMs, 1));
		if (LatencyMs > 0)
		{
			return Idx;
		}
	}

	return INDEX_NONE;
}
///////////////////////////////////////////////////////////////////////////////
struct FBitWindow
{
	void Reset(uint32 Count)
	{
		Count = FMath::RoundUpToPowerOfTwo(Count);
		Bits.SetNum(int32(Count), false);
		Counter = 0;
		Mask = Count - 1;
	}

	void Add(bool bValue)
	{
		const uint32 Idx = Counter++ & Mask;
		Bits[Idx] = bValue;
	}

	float AvgSetBits() const
	{
		return float(Bits.CountSetBits()) / float(Bits.Num());
	}

private:
	TBitArray<> Bits;
	uint32 Counter = 0;
	uint32 Mask = 0;
};
///////////////////////////////////////////////////////////////////////////////
FIoHash GetChunkKey(const FIoHash& ChunkHash, const FIoOffsetAndLength& Range)
{
	FIoHashBuilder HashBuilder;
	HashBuilder.Update(ChunkHash.GetBytes(), sizeof(FIoHash::ByteArray));
	HashBuilder.Update(&Range, sizeof(FIoOffsetAndLength));

	return HashBuilder.Finalize();
}

///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoStore
{
public:
	struct FTocEntry
	{
		uint32 RawSize = 0;
		uint32 EncodedSize = 0;
		uint32 BlockOffset = ~uint32(0);
		uint32 BlockCount = 0; 
		FIoHash Hash;
	};

	struct FToc;

	struct FContainer
	{
		const FToc* Toc = nullptr;
		FAES::FAESKey EncryptionKey;
		FString Name; // TODO: Consider removing when NO_LOGGING == 1 (only used for logging at the moment)
		FString EncryptionKeyGuid;
		FString ChunksDirectory;
		FName CompressionFormat;
		uint32 BlockSize = 0;

		TMap<FIoChunkId, FTocEntry> TocEntries;
		TArray<uint32> BlockSizes;
		TArray<FIoBlockHash> BlockHashes;
	};

	struct FToc
	{
		FString TocPath;
		TArray<FContainer> Containers;
	};

	using FTocArray = TChunkedArray<FToc, sizeof(FToc) * 4>;

	struct FChunkInfo
	{
		const FContainer* Container = nullptr;
		const FTocEntry* Entry = nullptr;

		bool IsValid() const { return Container && Entry; }
		operator bool() const { return IsValid(); }

		TConstArrayView<uint32> GetBlocks() const
		{
			check(Container != nullptr && Entry != nullptr);
			return TConstArrayView<uint32>(Container->BlockSizes.GetData() + Entry->BlockOffset, Entry->BlockCount);
		}
		
		TConstArrayView<FIoBlockHash> GetBlockHashes() const
		{
			check(Container != nullptr && Entry != nullptr);
			return Container->BlockHashes.IsEmpty()
				? TConstArrayView<FIoBlockHash>()
				: TConstArrayView<FIoBlockHash>(Container->BlockHashes.GetData() + Entry->BlockOffset, Entry->BlockCount);
		}
	};

	FOnDemandIoStore();
	~FOnDemandIoStore();

	void AddToc(const FString& TocPath, FOnDemandToc&& Toc);
	TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& ChunkId);
	FChunkInfo GetChunkInfo(const FIoChunkId& ChunkId);
	FString GetFirstTocPath() const;

	TArray<FIoChunkId> GetAllChunkIds(bool bIncludeOptionalChunks);

private:
	void AddDeferredContainers();
	void OnEncryptionKeyAdded(const FGuid& Id, const FAES::FAESKey& Key);

	FTocArray Tocs;
	TArray<FContainer*> RegisteredContainers;
	TArray<FContainer*> DeferredContainers;
	mutable FRWLock Lock;
};

FOnDemandIoStore::FOnDemandIoStore()
{
	FEncryptionKeyManager::Get().OnKeyAdded().AddRaw(this, &FOnDemandIoStore::OnEncryptionKeyAdded);
}

FOnDemandIoStore::~FOnDemandIoStore()
{
	FEncryptionKeyManager::Get().OnKeyAdded().RemoveAll(this);
}

void FOnDemandIoStore::AddToc(const FString& TocPath, FOnDemandToc&& Toc)
{
	UE_LOG(LogIas, Log, TEXT("Adding TOC '%s'"), *TocPath);

	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::AddToc);

	FString Prefix;
	{
		int32 Idx = INDEX_NONE;
		if (TocPath.FindLastChar(TCHAR('/'), Idx))
		{
			Prefix = TocPath.Left(Idx);
		}
	}

	{
		FWriteScopeLock _(Lock);

		const FOnDemandTocHeader& Header = Toc.Header;
		FToc* NewToc = new(Tocs) FToc{TocPath};
		NewToc->Containers.SetNum(Toc.Containers.Num()); // List of containers can never change

		const FName CompressionFormat(Header.CompressionFormat);
		int32 ContainerIndex = 0;

		for (FOnDemandTocContainerEntry& Container : Toc.Containers)
		{
			FContainer* NewContainer = &NewToc->Containers[ContainerIndex++];
			NewContainer->Toc = NewToc;
			NewContainer->Name = MoveTemp(Container.ContainerName);
			NewContainer->ChunksDirectory = (Prefix.IsEmpty() ? Header.ChunksDirectory : Prefix / Header.ChunksDirectory).ToLower();
			NewContainer->CompressionFormat = CompressionFormat;
			NewContainer->BlockSize = Header.BlockSize;
			NewContainer->EncryptionKeyGuid = Container.EncryptionKeyGuid;
			
			NewContainer->TocEntries.Reserve(Container.Entries.Num());
			for (const FOnDemandTocEntry& TocEntry : Container.Entries)
			{
				check(TocEntry.RawSize <= 0xffff'ffffull);
				check(TocEntry.EncodedSize <= 0xffff'ffffull);
				NewContainer->TocEntries.Add(TocEntry.ChunkId, FTocEntry
				{
					uint32(TocEntry.RawSize),
					uint32(TocEntry.EncodedSize),
					TocEntry.BlockOffset,
					TocEntry.BlockCount,
					TocEntry.Hash,
				});
			}

			NewContainer->BlockSizes = MoveTemp(Container.BlockSizes);
			NewContainer->BlockHashes = MoveTemp(Container.BlockHashes);

			DeferredContainers.Add(NewContainer);
		}
	}

	AddDeferredContainers();
}

TIoStatusOr<uint64> FOnDemandIoStore::GetChunkSize(const FIoChunkId& ChunkId)
{
	if (FChunkInfo Info = GetChunkInfo(ChunkId))
	{
		return Info.Entry->RawSize;
	}

	return FIoStatus(EIoErrorCode::UnknownChunkID);
}

FOnDemandIoStore::FChunkInfo FOnDemandIoStore::GetChunkInfo(const FIoChunkId& ChunkId)
{
	FReadScopeLock _(Lock);

	for (const FContainer* Container : RegisteredContainers)
	{
		if (const FTocEntry* Entry = Container->TocEntries.Find(ChunkId))
		{
			return FChunkInfo{Container, Entry};
		}
	}

	return {};
}

TArray<FIoChunkId> FOnDemandIoStore::GetAllChunkIds(bool bIncludeOptionalChunks)
{
	FReadScopeLock _(Lock);

	TArray<FIoChunkId> ChunkIds;
	int32 NumChunks = 0;
	for (const FContainer* Container : RegisteredContainers)
	{
		NumChunks += Container->TocEntries.Num();
	}

	ChunkIds.Reserve(NumChunks);

	for (const FContainer* Container : RegisteredContainers)
	{
		for (const TPair<FIoChunkId, FTocEntry>& Entry : Container->TocEntries)
		{
			if (bIncludeOptionalChunks || Entry.Key.GetChunkType() != EIoChunkType::OptionalBulkData)
			{
				ChunkIds.Add(Entry.Key);
			}
		}
	}

	return ChunkIds;
}

void FOnDemandIoStore::AddDeferredContainers()
{
	FWriteScopeLock _(Lock);

	for (auto It = DeferredContainers.CreateIterator(); It; ++It)
	{
		FContainer* Container = *It;
		if (Container->EncryptionKeyGuid.IsEmpty())
		{
			check(Container->EncryptionKey.IsValid() == false);
			UE_LOG(LogIas, Log, TEXT("Mounting container '%s' (%d entries)"), *Container->Name, Container->TocEntries.Num());
			RegisteredContainers.Add(Container);
			It.RemoveCurrent();
		}
		else
		{
			FGuid KeyGuid;
			ensure(FGuid::Parse(Container->EncryptionKeyGuid, KeyGuid));
			if (const FAES::FAESKey* Key = FEncryptionKeyManager::Get().GetKey(KeyGuid))
			{
				UE_LOG(LogIas, Log, TEXT("Mounting container '%s' (%d entries)"), *Container->Name, Container->TocEntries.Num());
				Container->EncryptionKey = *Key;
				RegisteredContainers.Add(Container);
				It.RemoveCurrent();
			}
			else
			{
				UE_LOG(LogIas, Log, TEXT("Defeering container '%s', encryption key '%s' not available"), *Container->Name, *Container->EncryptionKeyGuid);
			}
		}
	}
}

void FOnDemandIoStore::OnEncryptionKeyAdded(const FGuid& Id, const FAES::FAESKey& Key)
{
	LLM_SCOPE_BYTAG(Ias);
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::OnEncryptionKeyAdded);
	AddDeferredContainers();
}

FString FOnDemandIoStore::GetFirstTocPath() const
{
	FReadScopeLock _(Lock);
	return Tocs.Num() > 0 ? Tocs[0].TocPath : FString();
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
class TThreadSafeIntrusiveQueue
{
public:
	void Enqueue(T* Request)
	{
		check(Request->NextRequest == nullptr);
		FScopeLock _(&CriticalSection);

		if (Tail)
		{
			Tail->NextRequest = Request;
		}
		else
		{
			check(Head == nullptr);
			Head = Request;	
		}

		Tail = Request;
	}

	void EnqueueByPriority(T* Request)
	{
		FScopeLock _(&CriticalSection);
		EnqueueByPriorityInternal(Request);
	}

	T* Dequeue()
	{
		FScopeLock _(&CriticalSection);

		T* Requests = Head;
		Head = Tail = nullptr;

		return Requests;
	}

	void Reprioritize(T* Request)
	{
		// Switch to double linked list/array if this gets too expensive
		FScopeLock _(&CriticalSection);
		if (RemoveInternal(Request))
		{
			EnqueueByPriorityInternal(Request);
		}
	}

private:
	void EnqueueByPriorityInternal(T* Request)
	{
		check(Request->NextRequest == nullptr);

		if (Head == nullptr || Request->Priority > Head->Priority)
		{
			if (Head == nullptr)
			{
				check(Tail == nullptr);
				Tail = Request;
			}

			Request->NextRequest = Head;
			Head = Request;
		}
		else if (Request->Priority <= Tail->Priority)
		{
			check(Tail != nullptr);
			Tail->NextRequest = Request;
			Tail = Request;
		}
		else
		{
			// NOTE: This can get expensive if the queue gets too long, might be better to have x number of bucket(s)
			TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::EnqueueByPriority);
			T* It = Head;
			while (It->NextRequest != nullptr && Request->Priority <= It->NextRequest->Priority)
			{
				It = It->NextRequest;
			}

			Request->NextRequest = It->NextRequest;
			It->NextRequest = Request;
		}
	}

	bool RemoveInternal(T* Request)
	{
		check(Request != nullptr);
		if (Head == nullptr)
		{
			check(Tail == nullptr);
			return false;
		}

		if (Head == Request)
		{
			Head = Request->NextRequest; 
			if (Tail == Request)
			{
				check(Head == nullptr);
				Tail = nullptr;
			}

			Request->NextRequest = nullptr;
			return true;
		}
		else
		{
			T* It = Head;
			while (It->NextRequest && It->NextRequest != Request)
			{
				It = It->NextRequest;
			}

			if (It->NextRequest == Request)
			{
				It->NextRequest = It->NextRequest->NextRequest;
				Request->NextRequest = nullptr;
				return true;
			}
		}

		return false;
	}

	FCriticalSection CriticalSection;
	T* Head = nullptr;
	T* Tail = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
struct FChunkRequestParams
{
	static FChunkRequestParams Create(const FIoOffsetAndLength& OffsetLength, FOnDemandIoStore::FChunkInfo ChunkInfo)
	{
		const uint64 RawSize = FMath::Min<uint64>(OffsetLength.GetLength(), ChunkInfo.Entry->RawSize);
		
		const FIoOffsetAndLength ChunkRange = FIoChunkEncoding::GetChunkRange(
			ChunkInfo.Entry->RawSize,
			ChunkInfo.Container->BlockSize,
			ChunkInfo.GetBlocks(),
			OffsetLength.GetOffset(),
			RawSize).ConsumeValueOrDie();

		return FChunkRequestParams{GetChunkKey(ChunkInfo.Entry->Hash, ChunkRange), ChunkRange, ChunkInfo};
	}

	static FChunkRequestParams Create(FIoRequestImpl* Request, FOnDemandIoStore::FChunkInfo ChunkInfo)
	{
		check(Request);
		check(Request->NextRequest == nullptr);
		return Create(FIoOffsetAndLength(Request->Options.GetOffset(), Request->Options.GetSize()), ChunkInfo);
	}

	const FIoHash& GetUrlHash() const
	{
		return ChunkInfo.Entry->Hash;
	}

	void GetUrl(FAnsiStringBuilderBase& Url) const
	{
		const FString HashString = LexToString(ChunkInfo.Entry->Hash);
		Url << "/" << ChunkInfo.Container->ChunksDirectory
			<< "/" << HashString.Left(2)
			<< "/" << HashString << ANSITEXTVIEW(".iochunk");
	}

	FIoChunkDecodingParams GetDecodingParams() const
	{
		const FAES::FAESKey& EncryptionKey = ChunkInfo.Container->EncryptionKey;

		FIoChunkDecodingParams Params;
		Params.EncryptionKey = EncryptionKey.IsValid() ? MakeMemoryView(EncryptionKey.Key, FAES::FAESKey::KeySize) : FMemoryView();
		Params.CompressionFormat = ChunkInfo.Container->CompressionFormat;
		Params.BlockSize = ChunkInfo.Container->BlockSize;
		Params.TotalRawSize = ChunkInfo.Entry->RawSize;
		Params.EncodedBlockSize = ChunkInfo.GetBlocks(); 
		Params.BlockHash = ChunkInfo.GetBlockHashes(); 
		Params.EncodedOffset = ChunkRange.GetOffset();

		return Params;
	}

	FIoHash ChunkKey;
	FIoOffsetAndLength ChunkRange;
	FOnDemandIoStore::FChunkInfo ChunkInfo;
};

///////////////////////////////////////////////////////////////////////////////
struct FChunkRequest
{
	explicit FChunkRequest(FIoRequestImpl* Request, const FChunkRequestParams& RequestParams)
		: NextRequest()
		, Params(RequestParams)
		, RequestHead(Request)
		, RequestTail(Request)
		, StartTime(FPlatformTime::Cycles64())
		, Priority(Request->Priority)
		, RequestCount(1)
		, HttpRetryCount(0)
		, bCached(false)
	{
		check(Request && NextRequest == nullptr);
	}

	bool AddDispatcherRequest(FIoRequestImpl* Request)
	{
		check(RequestHead && RequestTail);
		check(Request && !Request->NextRequest);

		const bool bPriorityChanged = Request->Priority > RequestHead->Priority;
		if (bPriorityChanged)
		{
			Priority = Request->Priority;
			Request->NextRequest = RequestHead;
			RequestHead = Request;
		}
		else
		{
			FIoRequestImpl* It = RequestHead;
			while (It->NextRequest != nullptr && Request->Priority <= It->NextRequest->Priority)
			{
				It = It->NextRequest;
			}

			if (RequestTail == It)
			{
				check(It->NextRequest == nullptr);
				RequestTail = Request;
			}

			Request->NextRequest = It->NextRequest;
			It->NextRequest = Request;
		}

		RequestCount++;
		return bPriorityChanged;
	}

	uint32 RemoveDispatcherRequest(FIoRequestImpl* Request)
	{
		check(Request != nullptr);
		check(RequestCount > 0);

		if (RequestHead == Request)
		{
			RequestHead = Request->NextRequest; 
			if (RequestTail == Request)
			{
				check(RequestHead == nullptr);
				RequestTail = nullptr;
			}
		}
		else
		{
			FIoRequestImpl* It = RequestHead;
			while (It->NextRequest != Request)
			{
				It = It->NextRequest;
			}
			check(It->NextRequest == Request);
			It->NextRequest = It->NextRequest->NextRequest;
		}

		Request->NextRequest = nullptr;
		RequestCount--;

		return RequestCount;
	}

	FIoRequestImpl* DeqeueDispatcherRequests()
	{
		FIoRequestImpl* Head = RequestHead;
		RequestHead = RequestTail = nullptr;
		RequestCount = 0;

		return Head;
	}

	const FIoChunkId& GetChunkId()
	{
		return RequestHead->ChunkId;
	}

	FChunkRequest* NextRequest;
	FChunkRequestParams Params;
	FIoRequestImpl* RequestHead;
	FIoRequestImpl* RequestTail;
	FIoBuffer Chunk;
	UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> CacheTask;
	UE::Tasks::FTask DecodeTask;
	FIoCancellationToken CancellationToken;
	uint64 StartTime;
	int32 Priority;
	uint16 RequestCount;
	uint16 HttpRetryCount;
	bool bCached;
};

///////////////////////////////////////////////////////////////////////////////

static void LogIoResult(
	const FIoChunkId& ChunkId,
	const FIoHash& UrlHash,
	uint64 DurationMs,
	uint64 UncompressedSize,
	uint64 UncompressedOffset,
	uint64 CompressedOffset,
	int32 Priority,
	bool bCached)
{
	const TCHAR* Prefix = [bCached, UncompressedSize]() -> const TCHAR*
	{
		if (UncompressedSize == 0)
		{
			return bCached ? TEXT("io-cache-error") : TEXT("io-http-error ");
		}
		return bCached ? TEXT("io-cache") : TEXT("io-http ");
	}();

	UE_LOG(LogIas, VeryVerbose, TEXT("%s: %5" UINT64_FMT "ms %5" UINT64_FMT "KiB[%7" UINT64_FMT "] % s: % s | %" UINT64_FMT "(%d)"),
		Prefix,
		DurationMs,
		UncompressedSize >> 10,
		UncompressedOffset,
		*LexToString(ChunkId),
		*LexToString(UrlHash),
		CompressedOffset,
		Priority);
};

/** Utility for finding all available on demand utoc files currently in valid pak directories */
static TArray<FString> FindOnDemandUtocFilesOnDisk()
{
#if UE_IAS_LINKPAKFILE
	// TODO: This line is all over the engine, should make it look a bit nicer
	FPakPlatformFile* PakPlatformFile = static_cast<FPakPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
	if (!PakPlatformFile)
	{
		return TArray<FString>();
	}

	TArray<FString> PakFolders;
	PakPlatformFile->GetPakFolders(FCommandLine::Get(), PakFolders);

	IPlatformFile* SearchFile = PakPlatformFile->GetLowerLevel();
#else
	TStringBuilder<260> PakPath;
	FPathViews::Append(PakPath, FPaths::ProjectContentDir(), TEXT("Paks"));

	TArray<FString> PakFolders;
	PakFolders.Add(PakPath.ToString());

	IPlatformFile* SearchFile = &FPlatformFileManager::Get().GetPlatformFile();
#endif // UE_IAS_LINKPAKFILE

	TArray<FString> FoundFiles;
	for (const FString& Directory : PakFolders)
	{
		//PakPlatformFile
		SearchFile->IterateDirectoryRecursively(*Directory, [&FoundFiles/*, PakPlatformFile*/](const TCHAR* Path, bool bIsDirectory) -> bool
			{
				if (!bIsDirectory)
				{
					FString Filename(Path);
					if (Filename.EndsWith(TEXT(".utoc")) && Filename.Contains(TEXT("ondemand")))
					{
						// TODO: Cannot call IsPakFileInstalled at this point
						//if (PakPlatformFile->IsPakFileInstalled(Filename))
						{
							FoundFiles.Emplace(MoveTemp(Filename));
						}
					}
				}

				return true;
			});
	}

	return FoundFiles;
}

/** Generate a FOnDemandToc based on utoc files on disk which support the OnDemand feature */
TIoStatusOr<FOnDemandToc> GenerateOnDemandTocFromDisk()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GenerateOnDemandTocFromDisk);

	FOnDemandToc OutToc;

	TArray<FString> UtocFilePaths = FindOnDemandUtocFilesOnDisk();
	const TMap<FGuid, FAES::FAESKey> EncryptionKeys = FEncryptionKeyManager::Get().GetAllKeys();

	for (const FString& UtocFilePath : UtocFilePaths)
	{
		FIoStoreReader Reader;
		FIoStatus Status = Reader.Initialize(FPathViews::GetBaseFilenameWithPath(UtocFilePath), EncryptionKeys);
		if (Status.IsOk() && EnumHasAnyFlags(Reader.GetContainerFlags(), EIoContainerFlags::OnDemand))
		{
			FOnDemandTocContainerEntry Container;

			Container.ContainerName = FPathViews::GetBaseFilename(UtocFilePath);

			const uint32 BlockSize = Reader.GetCompressionBlockSize();
			if (OutToc.Header.BlockSize == 0)
			{
				OutToc.Header.BlockSize = Reader.GetCompressionBlockSize();
			}
			check(OutToc.Header.BlockSize == Reader.GetCompressionBlockSize());

			TArray<FIoStoreTocChunkInfo> ChunkInfos;
			Reader.EnumerateChunks([&ChunkInfos](FIoStoreTocChunkInfo&& Info)
				{
					ChunkInfos.Emplace(MoveTemp(Info));
					return true;
				});

			// We can't actually hit this until we solve the FASEKey Initialize issue above
			if (EnumHasAnyFlags(Reader.GetContainerFlags(), EIoContainerFlags::Encrypted))
			{
				Container.EncryptionKeyGuid = LexToString(Reader.GetEncryptionKeyGuid());
			}

			for (const FIoStoreTocChunkInfo& ChunkInfo : ChunkInfos)
			{
				TIoStatusOr<FIoStoreCompressedChunkInfo> InfoStatus = Reader.GetChunkCompressedInfo(ChunkInfo.Id);
				if (!InfoStatus.IsOk())
				{
					return InfoStatus.Status();
				}
				FIoStoreCompressedChunkInfo CompressedChunkInfo = InfoStatus.ConsumeValueOrDie();

				const uint32 BlockOffset = Container.BlockSizes.Num();
				const uint32 BlockCount = CompressedChunkInfo.Blocks.Num();

				uint64 RawChunkSize = 0;
				uint64 EncodedChunkSize = 0;
				for (const FIoStoreCompressedBlockInfo& BlockInfo : CompressedChunkInfo.Blocks)
				{
					const uint64 EncodedBlockSize = Align(BlockInfo.CompressedSize, FAES::AESBlockSize);
					Container.BlockSizes.Add(uint32(EncodedBlockSize));

					FIoBlockHash BlockHash;
					FMemory::Memcpy(&BlockHash, &BlockInfo.DiskHash, sizeof(FIoBlockHash));
					Container.BlockHashes.Add(BlockHash);

					EncodedChunkSize += EncodedBlockSize;
					RawChunkSize += BlockInfo.UncompressedSize;

					if (OutToc.Header.CompressionFormat.IsEmpty() && BlockInfo.CompressionMethod != NAME_None)
					{
						OutToc.Header.CompressionFormat = BlockInfo.CompressionMethod.ToString();
					}
				}

				FOnDemandTocEntry& TocEntry = Container.Entries.AddDefaulted_GetRef();
				TocEntry.ChunkId = ChunkInfo.Id;
				TocEntry.Hash = CompressedChunkInfo.DiskHash;
				TocEntry.RawSize = RawChunkSize;
				TocEntry.EncodedSize = EncodedChunkSize;
				TocEntry.BlockOffset = BlockOffset;
				TocEntry.BlockCount = BlockCount;
			}

			OutToc.Containers.Emplace(MoveTemp(Container));
		}
	}

	if (!OutToc.Containers.IsEmpty())
	{
		OutToc.Header.ChunksDirectory = FString::Printf(TEXT("IoChunksV%u"), EOnDemandChunkVersion::Latest).ToLower();
	}

	return OutToc;
}

/** Validation code used during development to ensure that the results are correct */
#if UE_VALIDATE_GENERATED_TOC

static TArray<const FOnDemandTocContainerEntry*> SortContainers(const TArray<FOnDemandTocContainerEntry>& Containers)
{
	TArray<const FOnDemandTocContainerEntry*> SortedContainers;
	for (const FOnDemandTocContainerEntry& Container : Containers)
	{
		SortedContainers.Add(&Container);
	}

	Algo::Sort(SortedContainers, [](const FOnDemandTocContainerEntry* LHS, const FOnDemandTocContainerEntry* RHS)->bool
		{
			return LHS->ContainerName < RHS->ContainerName;
		});

	return SortedContainers;
}

static bool operator == (const FOnDemandTocEntry& LHS, const FOnDemandTocEntry& RHS)
{
	return FMemory::Memcmp(&LHS, &RHS, sizeof(FOnDemandTocEntry)) == 0;
}

static void ValidateToc(const FOnDemandToc& RefToc, const FOnDemandToc& NewToc)
{
	check(RefToc.Header.BlockSize == NewToc.Header.BlockSize);
	check(RefToc.Header.CompressionFormat == NewToc.Header.CompressionFormat);
	check(RefToc.Header.ChunksDirectory == NewToc.Header.ChunksDirectory);

	TArray<const FOnDemandTocContainerEntry*> RefContainers = SortContainers(RefToc.Containers);
	TArray<const FOnDemandTocContainerEntry*> NewContainers = SortContainers(NewToc.Containers);

	check(RefContainers.Num() == NewContainers.Num());

	for (int32 Index = 0; Index < RefContainers.Num(); ++Index)
	{
		const FOnDemandTocContainerEntry* RefContainer = RefContainers[Index];
		const FOnDemandTocContainerEntry* NewContainer = NewContainers[Index];

		check(RefContainer->ContainerName == NewContainer->ContainerName);

		check(RefContainer->Entries == NewContainer->Entries);
		check(RefContainer->BlockSizes == NewContainer->BlockSizes);
		check(RefContainer->BlockHashes == NewContainer->BlockHashes);
	}
}

#endif //UE_VALIDATE_GENERATED_TOC

///////////////////////////////////////////////////////////////////////////////
struct FBackendStatus
{
	enum class EFlags : uint8
	{
		None						= 0,
		CacheEnabled				= (1 << 0),
		HttpEnabled					= (1 << 1),
		HttpError					= (1 << 2),
		HttpBulkOptionalDisabled	= (1 << 3),
		AbandonCache				= (1 << 4),
	};

	bool IsHttpEnabled() const
	{
		return IsHttpEnabled(Flags.load(std::memory_order_relaxed));
	}

	bool IsHttpEnabled(EIoChunkType ChunkType) const
	{
		const uint8 CurrentFlags = Flags.load(std::memory_order_relaxed);
		return IsHttpEnabled(CurrentFlags) &&
			(ChunkType != EIoChunkType::OptionalBulkData ||
				((CurrentFlags & uint8(EFlags::HttpBulkOptionalDisabled)) == 0 && GIasHttpOptionalBulkDataEnabled));
	}

	bool IsHttpError() const
	{
		return HasAnyFlags(EFlags::HttpError);
	}

	bool IsCacheEnabled() const
	{
		return HasAnyFlags(EFlags::CacheEnabled);
	}

	bool IsCacheWriteable() const
	{
		const uint8 CurrentFlags = Flags.load(std::memory_order_relaxed);
		return (CurrentFlags & uint8(EFlags::CacheEnabled)) && IsHttpEnabled(CurrentFlags); 
	}

	bool IsCacheReadOnly() const
	{
		const uint8 CurrentFlags = Flags.load(std::memory_order_relaxed);
		return (CurrentFlags & uint8(EFlags::CacheEnabled)) && !IsHttpEnabled(CurrentFlags);
	}

	bool ShouldAbandonCache() const
	{
		return HasAnyFlags(EFlags::AbandonCache);
	}

	void SetHttpEnabled(bool bEnabled)
	{
		AddOrRemoveFlags(EFlags::HttpEnabled, bEnabled, TEXT("HTTP streaming enabled"));
		FGenericCrashContext::SetEngineData(TEXT("IAS.Enabled"), bEnabled ? TEXT("true") : TEXT("false"));
	}

	void SetHttpOptionalBulkEnabled(bool bEnabled)
	{
		AddOrRemoveFlags(EFlags::HttpBulkOptionalDisabled, bEnabled == false, TEXT("HTTP streaming of optional bulk data disabled"));
	}

	void SetCacheEnabled(bool bEnabled)
	{
		AddOrRemoveFlags(EFlags::CacheEnabled, bEnabled, TEXT("Cache enabled"));
	}

	void SetHttpError(bool bError)
	{
		AddOrRemoveFlags(EFlags::HttpError, bError, TEXT("HTTP streaming error"));
	}

	void SetAbandonCache(bool bAbandon)
	{
		AddOrRemoveFlags(EFlags::AbandonCache, bAbandon, TEXT("Abandon cache"));
	}

private:
	static bool IsHttpEnabled(uint8 FlagsToTest)
	{
		constexpr uint8 HttpFlags = uint8(EFlags::HttpEnabled) | uint8(EFlags::HttpError);
		return ((FlagsToTest & HttpFlags) == uint8(EFlags::HttpEnabled)) && GIasHttpEnabled;
	}

	bool HasAnyFlags(uint8 Contains) const
	{
		return (Flags.load(std::memory_order_relaxed) & Contains) != 0;
	}
	
	bool HasAnyFlags(EFlags Contains) const
	{
		return HasAnyFlags(uint8(Contains));
	}

	uint8 AddFlags(EFlags FlagsToAdd)
	{
		return Flags.fetch_or(uint8(FlagsToAdd));
	}

	uint8 RemoveFlags(EFlags FlagsToRemove)
	{
		return Flags.fetch_and(~uint8(FlagsToRemove));
	}

	uint8 AddOrRemoveFlags(EFlags FlagsToAddOrRemove, bool bValue)
	{
		return bValue ? AddFlags(FlagsToAddOrRemove) : RemoveFlags(FlagsToAddOrRemove);
	}

	void AddOrRemoveFlags(EFlags FlagsToAddOrRemove, bool bValue, const TCHAR* DebugText)
	{
		const uint8 PrevFlags = AddOrRemoveFlags(FlagsToAddOrRemove, bValue);
		TStringBuilder<128> Sb;
		Sb.Append(DebugText)
			<< TEXT(" '");
		Sb.Append(bValue ? TEXT("true") : TEXT("false"))
			<< TEXT("', backend status '(")
			<< EFlags(PrevFlags)
			<< TEXT(") -> (")
			<< EFlags(Flags.load(std::memory_order_relaxed))
			<< TEXT(")'");
		UE_LOG(LogIas, Log, TEXT("%s"), Sb.ToString());
	}

	friend FStringBuilderBase& operator<<(FStringBuilderBase& Sb, EFlags StatusFlags)
	{
		if (StatusFlags == EFlags::None)
		{
			Sb.Append(TEXT("None"));
			return Sb;
		}

		bool bFirst = true;
		auto AppendIf = [StatusFlags, &Sb, &bFirst](EFlags Contains, const TCHAR* Str)
		{
			if (uint8(StatusFlags) & uint8(Contains))
			{
				if (!bFirst)
				{
					Sb.AppendChar(TEXT('|'));
				}
				Sb.Append(Str);
				bFirst = false;
			}
		};

		AppendIf(EFlags::CacheEnabled, TEXT("CacheEnabled"));
		AppendIf(EFlags::HttpEnabled, TEXT("HttpEnabled"));
		AppendIf(EFlags::HttpError, TEXT("HttpError"));
		AppendIf(EFlags::HttpBulkOptionalDisabled, TEXT("HttpBulkOptionalDisabled"));

		return Sb;
	}

	std::atomic<uint8> Flags{0};
};
///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoBackend final
	: public FRunnable
	, public IOnDemandIoDispatcherBackend
{
	using FIoRequestQueue = TThreadSafeIntrusiveQueue<FIoRequestImpl>;
	using FChunkRequestQueue = TThreadSafeIntrusiveQueue<FChunkRequest>;

	struct FAvailableEps
	{
		bool HasCurrent() const { return Current != INDEX_NONE; }
		const FString& GetCurrent() const { return Urls[Current]; }

		int32 Current = INDEX_NONE;
		TArray<FString> Urls;
	};

	struct FBackendData
	{
		static void Attach(FIoRequestImpl* Request, const FIoHash& ChunkKey)
		{
			check(Request->BackendData == nullptr);
			Request->BackendData = new FBackendData{ChunkKey};
		}

		static TUniquePtr<FBackendData> Detach(FIoRequestImpl* Request)
		{
			check(Request->BackendData != nullptr);
			void* BackendData = Request->BackendData;
			Request->BackendData = nullptr;
			return TUniquePtr<FBackendData>(static_cast<FBackendData*>(BackendData));
		}
		
		static FBackendData& Get(FIoRequestImpl* Request)
		{
			check(Request->BackendData != nullptr);
			return *static_cast<FBackendData*>(Request->BackendData);
		}

		FIoHash ChunkKey;
	};

	struct FChunkRequests
	{
		FChunkRequest* TryUpdatePriority(FIoRequestImpl* Request)
		{
			FScopeLock _(&Mutex);

			const FBackendData& BackendData = FBackendData::Get(Request);
			if (FChunkRequest** InflightRequest = Inflight.Find(BackendData.ChunkKey))
			{
				FChunkRequest* ChunkRequest = *InflightRequest;
				if (Request->Priority > ChunkRequest->Priority)
				{
					ChunkRequest->Priority = Request->Priority;
					return ChunkRequest;
				}
			}

			return nullptr;
		}

		FChunkRequest* Create(FIoRequestImpl* Request, const FChunkRequestParams& Params, bool& bOutPending, bool& bOutUpdatePriority)
		{
			FScopeLock _(&Mutex);
			
			FBackendData::Attach(Request, Params.ChunkKey);

			if (FChunkRequest** InflightRequest = Inflight.Find(Params.ChunkKey))
			{
				FChunkRequest* ChunkRequest = *InflightRequest;
				check(!ChunkRequest->CancellationToken.IsCancelled());
				bOutPending = true;
				bOutUpdatePriority = ChunkRequest->AddDispatcherRequest(Request);

				return ChunkRequest;
			}

			bOutPending = bOutUpdatePriority = false;
			FChunkRequest* ChunkRequest = Allocator.Construct(Request, Params);
			Inflight.Add(Params.ChunkKey, ChunkRequest);

			return ChunkRequest;
		}

		bool Cancel(FIoRequestImpl* Request)
		{
			FScopeLock _(&Mutex);

			FBackendData& BackendData = FBackendData::Get(Request);
			UE_LOG(LogIas, VeryVerbose, TEXT("%s"),
				*WriteToString<256>(TEXT("Cancelling I/O request ChunkId='"), LexToString(Request->ChunkId), TEXT("' ChunkKey='"), BackendData.ChunkKey, TEXT("'")));

			if (FChunkRequest** InflightRequest = Inflight.Find(BackendData.ChunkKey))
			{
				FChunkRequest& ChunkRequest = **InflightRequest;
				const uint32 RemainingCount = ChunkRequest.RemoveDispatcherRequest(Request);
				check(Request->NextRequest == nullptr);

				if (RemainingCount == 0)
				{
					ChunkRequest.CancellationToken.Cancel();
					Inflight.Remove(BackendData.ChunkKey);
				}

				return true;
			}

			return false;
		}

		void Remove(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			Inflight.Remove(Request->Params.ChunkKey);
		}

		void Release(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			check(!IsInFlight(Request));
			Allocator.Destroy(Request);
		}
		
		void RemoveAndRelease(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			Inflight.Remove(Request->Params.ChunkKey);
			Allocator.Destroy(Request);
		}

	private:

		/** Helper intended to be called by methods that have already locked ::Mutex */
		inline bool IsInFlight(const FChunkRequest* Request) const
		{
			const FChunkRequest* const* InFlightRequest = Inflight.Find(Request->Params.ChunkKey);
			if (InFlightRequest == nullptr)
			{
				return false;
			}
			else
			{
				return *InFlightRequest == Request;
			}
		}

		TSingleThreadedSlabAllocator<FChunkRequest, 128> Allocator;
		TMap<FIoHash, FChunkRequest*> Inflight;
		FCriticalSection Mutex;
	};
public:

	FOnDemandIoBackend(TUniquePtr<IIasCache>&& InCache);
	virtual ~FOnDemandIoBackend();

	// I/O dispatcher backend
	virtual void Initialize(TSharedRef<const FIoDispatcherBackendContext> Context) override;
	virtual void Shutdown() override;
	virtual bool Resolve(FIoRequestImpl* Request) override;
	virtual void CancelIoRequest(FIoRequestImpl* Request) override;
	virtual void UpdatePriorityForIoRequest(FIoRequestImpl* Request) override;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId) const override;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId) const override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const;
	virtual FIoRequestImpl* GetCompletedRequests() override;
	virtual TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) override;

	// I/O Http backend
	virtual void Mount(const FOnDemandEndpoint& Endpoint) override;
	virtual void SetBulkOptionalEnabled(bool bEnabled) override;
	virtual void SetEnabled(bool bEnabled) override;
	virtual void AbandonCache() override;
	virtual void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const override;

	// Runnable
	virtual bool Init() override { return true; }
	virtual void Stop() override { bStopRequested = true; }
	virtual uint32 Run() override;

private:

	FString GetEndpointTestPath() const;
	void ConditionallyStartBackendThread();
	void CompleteRequest(FChunkRequest* ChunkRequest);

	FIoStatus ApplyGeneratedOnDemandToc(const FString& CdnUrl, const FString& TocPath);
	FIoStatus DownloadoadOnDemandToc(const FString& CdnUrl, const FString& TocPath);

	void AddDeferredTocs();
	void ProcessHttpRequests(FOnDemandHttpClient* HttpClient, FBitWindow& HttpErrors, int32 MaxConcurrentRequests);

	TUniquePtr<IIasCache> Cache;
	TUniquePtr<FOnDemandIoStore> IoStore;
	TSharedPtr<const FIoDispatcherBackendContext> BackendContext;
	TUniquePtr<FRunnableThread> BackendThread;
	FEventRef TickBackendEvent;
	TArray<FString> DeferredTocs;
	FChunkRequests ChunkRequests;
	FIoRequestQueue CompletedRequests;
	FChunkRequestQueue HttpRequests;
	FOnDemandIoBackendStats Stats;
	FBackendStatus BackendStatus;
	FAvailableEps AvailableEps;
	FString DistributionUrl;
	mutable FRWLock Lock;
	std::atomic_uint32_t InflightCacheRequestCount{0};
	std::atomic_bool bStopRequested{false};

	bool bGeneratedOnDemandToc = false;
	UE::Tasks::TTask<TIoStatusOr<FOnDemandToc>> OnDemandTocTask;
};

///////////////////////////////////////////////////////////////////////////////
FOnDemandIoBackend::FOnDemandIoBackend(TUniquePtr<IIasCache>&& InCache)
	: Cache(MoveTemp(InCache))
{
	IoStore = MakeUnique<FOnDemandIoStore>();
	BackendStatus.SetHttpEnabled(true);
	BackendStatus.SetCacheEnabled(Cache.IsValid());
}

FOnDemandIoBackend::~FOnDemandIoBackend()
{
	Shutdown();
}

void FOnDemandIoBackend::Initialize(TSharedRef<const FIoDispatcherBackendContext> Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::Initialize);
	LLM_SCOPE_BYTAG(Ias);
	UE_LOG(LogIas, Log, TEXT("Initializing on demand I/O dispatcher backend"));
	BackendContext = Context;

	if (DistributionUrl.IsEmpty() == false)
	{
		TSharedPtr<FDistributionEndpoints> Resolver = MakeShared<FDistributionEndpoints>();
		Resolver->ResolveEndpoints(DistributionUrl,
			[this, Resolver](const FString& DistributionEp, TConstArrayView<FString> Eps)
			{
				UE_CLOG(Eps.IsEmpty(), LogIas, Warning, TEXT("Failed to resolve available endpoint(s) from '%s'"), *DistributionEp);
				{
					FWriteScopeLock _(Lock);
					for (const FString& Ep : Eps)
					{
						AvailableEps.Urls.Add(Ep.Replace(TEXT("https"), TEXT("http")));
					}
				}

				{
					TConstArrayView<FString> Urls = AvailableEps.Urls;
					const int32 MaxUrls = FMath::Min(GIasMaxEndpointTestCountAtStartup, AvailableEps.Urls.Num());
					const FString TestPath = GetEndpointTestPath();
					if (int32 Idx = LatencyTest(Urls.Left(MaxUrls), TestPath, bStopRequested); Idx != INDEX_NONE)
					{
						AvailableEps.Current = Idx;
						UE_LOG(LogIas, Log, TEXT("Using endpoint '%s'"), *AvailableEps.GetCurrent());
						AddDeferredTocs();
					}
					else
					{
						BackendStatus.SetHttpError(true);
					}
				}
				ConditionallyStartBackendThread();
			});
		Resolver->ResolveDeferredEndpoints();
		DistributionUrl.Reset();
	}
}

void FOnDemandIoBackend::Shutdown()
{
	if (bStopRequested)
	{
		return;
	}

	UE_LOG(LogIas, Log, TEXT("Shutting down on demand I/O dispatcher backend"));

	bStopRequested = true;
	TickBackendEvent->Trigger();
	BackendThread.Reset();
	BackendContext.Reset();
}

FString FOnDemandIoBackend::GetEndpointTestPath() const
{
	FString TestPath = IoStore.IsValid() ? IoStore->GetFirstTocPath() : FString();
	if (TestPath.IsEmpty())
	{
		FReadScopeLock _(Lock);
		if (DeferredTocs.IsEmpty() == false)
		{
			TestPath = DeferredTocs[0];
		}
	}
	return TestPath;
}

void FOnDemandIoBackend::ConditionallyStartBackendThread()
{
	FWriteScopeLock _(Lock);
	if (BackendThread.IsValid() == false)
	{
		BackendThread.Reset(FRunnableThread::Create(this, TEXT("Ias.Http"), 0, TPri_AboveNormal));
	}
}

void FOnDemandIoBackend::CompleteRequest(FChunkRequest* ChunkRequest)
{
	LLM_SCOPE_BYTAG(Ias);
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::CompleteRequest);
	check(ChunkRequest != nullptr);
	const bool bCancelled = ChunkRequest->CancellationToken.IsCancelled();

	if (bCancelled)
	{
		check(ChunkRequest->RequestHead == nullptr);
		check(ChunkRequest->RequestTail == nullptr);
		return ChunkRequests.RemoveAndRelease(ChunkRequest);
	}

	ChunkRequests.Remove(ChunkRequest);
	
	FIoBuffer Chunk = MoveTemp(ChunkRequest->Chunk);
	FIoChunkDecodingParams DecodingParams = ChunkRequest->Params.GetDecodingParams();

	// Only cache chunks if HTTP streaming is enabled
	bool bCacheChunk = ChunkRequest->bCached == false && Chunk.GetSize() > 0;
	FIoRequestImpl* NextRequest = ChunkRequest->DeqeueDispatcherRequests();
	while (NextRequest)
	{
		FIoRequestImpl* Request = NextRequest;
		NextRequest = Request->NextRequest;
		Request->NextRequest = nullptr;

		bool bDecoded = false;
		if (Chunk.GetSize() > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::DecodeBlocks);
			const uint64 RawSize = FMath::Min<uint64>(Request->Options.GetSize(), ChunkRequest->Params.ChunkInfo.Entry->RawSize);
			Request->CreateBuffer(RawSize);
			DecodingParams.RawOffset = Request->Options.GetOffset(); 
			bDecoded = FIoChunkEncoding::Decode(DecodingParams, Chunk.GetView(), Request->GetBuffer().GetMutableView());
		}
		
		const uint64 DurationMs = Request->GetStartTime() > 0 ?
			(uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Request->GetStartTime()) : 0;

		if (bDecoded)
		{
			Stats.OnIoRequestComplete(Request->GetBuffer().GetSize(), DurationMs);
			LogIoResult(Request->ChunkId, ChunkRequest->Params.GetUrlHash(), DurationMs,
				Request->GetBuffer().DataSize(), Request->Options.GetOffset(),
				ChunkRequest->Params.ChunkRange.GetOffset(), ChunkRequest->Priority, ChunkRequest->bCached);
				
		}
		else
		{
			bCacheChunk = false;
			Request->SetFailed();

			Stats.OnIoRequestError();
			LogIoResult(Request->ChunkId, ChunkRequest->Params.GetUrlHash(), DurationMs,
				0, Request->Options.GetOffset(),
				ChunkRequest->Params.ChunkRange.GetOffset(), ChunkRequest->Priority, ChunkRequest->bCached);
		}

		CompletedRequests.Enqueue(Request);
		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}

	if (bCacheChunk && BackendStatus.IsCacheWriteable())
	{
		Cache->Put(ChunkRequest->Params.ChunkKey, Chunk);
	}

	ChunkRequests.Release(ChunkRequest);

	if (BackendStatus.ShouldAbandonCache() && InflightCacheRequestCount.load(std::memory_order_relaxed) == 0)
	{
		TickBackendEvent->Trigger();
	}
}

bool FOnDemandIoBackend::Resolve(FIoRequestImpl* Request)
{
	using namespace UE::Tasks;

	FOnDemandIoStore::FChunkInfo ChunkInfo = IoStore->GetChunkInfo(Request->ChunkId);
	if (!ChunkInfo.IsValid())
	{
		return false;
	}

	FChunkRequestParams RequestParams = FChunkRequestParams::Create(Request, ChunkInfo);

	if (BackendStatus.IsHttpEnabled(Request->ChunkId.GetChunkType()) == false)
	{ 
		// If the cache is not readonly the chunk may get evicted before the request is completed
		if (BackendStatus.IsCacheReadOnly() == false || Cache->ContainsChunk(RequestParams.ChunkKey) == false)
		{
			return false;
		}
	}

	Stats.OnIoRequestEnqueue();
	bool bPending = false;
	bool bUpdatePriority = false;
	FChunkRequest* ChunkRequest = ChunkRequests.Create(Request, RequestParams, bPending, bUpdatePriority);

	if (bPending)
	{
		if (bUpdatePriority)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::UpdatePriorityForIoRequest);
			HttpRequests.Reprioritize(ChunkRequest);
		}
		// The chunk for the request is already inflight 
		return true;
	}

	if (Cache.IsValid())
	{
		const FIoHash& Key = ChunkRequest->Params.ChunkKey;
		FIoBuffer& Buffer = ChunkRequest->Chunk;
		IIasCache::FGetToken GetToken = Cache->Get(Key, Buffer);

		if (Buffer.GetData() != nullptr)
		{
			ChunkRequest->bCached = true;
			Launch(UE_SOURCE_LOCATION, [this, ChunkRequest] { CompleteRequest(ChunkRequest); });
			return true;
		}

		if (GetToken == 0)
		{
			Stats.OnHttpEnqueue();
			HttpRequests.EnqueueByPriority(ChunkRequest);
			TickBackendEvent->Trigger();
			return true;
		}

		//TODO: Pass priority to cache
		InflightCacheRequestCount.fetch_add(1, std::memory_order_relaxed);
		ChunkRequest->CacheTask = Cache->Materialize(GetToken, FIoReadOptions(), &ChunkRequest->CancellationToken);
	}

	const ETaskPriority TaskPriority = ChunkRequest->Priority > IoDispatcherPriority_Medium ? ETaskPriority::High : ETaskPriority::Normal;
	Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
	{
		LLM_SCOPE_BYTAG(Ias);
		TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::CompleteOrEnqueueHttpRequest);
		if (ChunkRequest->CacheTask.IsValid())
		{
			InflightCacheRequestCount.fetch_sub(1, std::memory_order_relaxed);
			if (TIoStatusOr<FIoBuffer> Status = ChunkRequest->CacheTask.GetResult(); Status.IsOk())
			{
				ChunkRequest->Chunk = Status.ConsumeValueOrDie();
				ChunkRequest->bCached = true;
				return CompleteRequest(ChunkRequest);
			}
			else if (Status.Status().GetErrorCode() == EIoErrorCode::ReadError)
			{
				FOnDemandIoBackendStats::Get()->OnCacheError();
			}
		}

		if (ChunkRequest->CancellationToken.IsCancelled() || BackendStatus.IsHttpEnabled() == false)
		{
			UE_CLOG(BackendStatus.IsHttpEnabled() == false, LogIas, Log, TEXT("Chunk was not found in the cache and HTTP is disabled"));
			return CompleteRequest(ChunkRequest);
		}

		Stats.OnHttpEnqueue();
		HttpRequests.EnqueueByPriority(ChunkRequest);
		TickBackendEvent->Trigger();
	}, ChunkRequest->CacheTask, TaskPriority);

	return true;
}

void FOnDemandIoBackend::CancelIoRequest(FIoRequestImpl* Request)
{
	if (ChunkRequests.Cancel(Request))
	{
		CompletedRequests.Enqueue(Request);
		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}
}

void FOnDemandIoBackend::UpdatePriorityForIoRequest(FIoRequestImpl* Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::UpdatePriorityForIoRequest);
	if (FChunkRequest* ChunkRequest = ChunkRequests.TryUpdatePriority(Request))
	{
		HttpRequests.Reprioritize(ChunkRequest);
	}
}

bool FOnDemandIoBackend::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	const TIoStatusOr<uint64> ChunkSize = GetSizeForChunk(ChunkId);
	return ChunkSize.IsOk();
}

bool FOnDemandIoBackend::DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const
{
	uint64 Unused = 0;
	const TIoStatusOr<uint64> ChunkSize = GetSizeForChunk(ChunkId, ChunkRange, Unused);
	return ChunkSize.IsOk();
}

TIoStatusOr<uint64> FOnDemandIoBackend::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	uint64 Unused = 0;
	const FIoOffsetAndLength ChunkRange(0, MAX_uint64);
	return GetSizeForChunk(ChunkId, ChunkRange, Unused);
}

TIoStatusOr<uint64> FOnDemandIoBackend::GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const
{
	OutAvailable = 0;

	const FOnDemandIoStore::FChunkInfo ChunkInfo = IoStore.IsValid() ? IoStore->GetChunkInfo(ChunkId) : FOnDemandIoStore::FChunkInfo();
	if (ChunkInfo.IsValid() == false)
	{
		return FIoStatus(EIoErrorCode::UnknownChunkID);
	}

	FIoOffsetAndLength RequestedRange(ChunkRange.GetOffset(), FMath::Min<uint64>(ChunkInfo.Entry->RawSize, ChunkRange.GetLength()));
	OutAvailable = ChunkInfo.Entry->RawSize;

	if (BackendStatus.IsHttpEnabled(ChunkId.GetChunkType()) == false)
	{
		// If the cache is not readonly the chunk may get evicted before the request is resolved
		if (BackendStatus.IsCacheReadOnly() == false)
		{
			return FIoStatus(EIoErrorCode::UnknownChunkID);
		}

		check(Cache.IsValid());
		const FChunkRequestParams RequestParams = FChunkRequestParams::Create(RequestedRange, ChunkInfo);
		if (Cache->ContainsChunk(RequestParams.ChunkKey) == false)
		{
			return FIoStatus(EIoErrorCode::UnknownChunkID);
		}

		// Only the specified chunk range is available 
		OutAvailable = RequestedRange.GetLength();
	}

	return TIoStatusOr<uint64>(ChunkInfo.Entry->RawSize);
}

FIoRequestImpl* FOnDemandIoBackend::GetCompletedRequests()
{
	FIoRequestImpl* Requests = CompletedRequests.Dequeue();

	for (FIoRequestImpl* It = Requests; It != nullptr; It = It->NextRequest)
	{
		TUniquePtr<FBackendData> BackendData = FBackendData::Detach(It);
		check(It->BackendData == nullptr);
	}

	return Requests;
}

TIoStatusOr<FIoMappedRegion> FOnDemandIoBackend::OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options)
{
	return FIoStatus::Unknown;
}

FIoStatus FOnDemandIoBackend::ApplyGeneratedOnDemandToc(const FString& CdnUrl, const FString& TocPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::ApplyGeneratedOnDemandToc);

	if (bGeneratedOnDemandToc || !CVar_IasGenerateOnDemandToc->GetBool())
	{
		return FIoStatus::Ok;
	}

	TIoStatusOr<FOnDemandToc> GeneratedTocResult;

	if (OnDemandTocTask.IsValid())
	{
		GeneratedTocResult = MoveTemp(OnDemandTocTask.GetResult());
	}
	else
	{
		GeneratedTocResult = GenerateOnDemandTocFromDisk();
	}
	
	if (!GeneratedTocResult.IsOk())
	{
		return GeneratedTocResult.Status();
	}

#if UE_VALIDATE_GENERATED_TOC == 0
	IoStore->AddToc(TocPath, GeneratedTocResult.ConsumeValueOrDie());
#else
	FOnDemandToc UrlToc = LoadTocFromUrl(CdnUrl, TocPath, 1).ConsumeValueOrDie();
	FOnDemandToc GeneratedToc = GeneratedTocResult.ConsumeValueOrDie();

	UE::IO::IAS::ValidateToc(UrlToc, GeneratedToc);

	IoStore->AddToc(TocPath, MoveTemp(GeneratedToc));
#endif // UE_VALIDATE_GENERATED_TOC

	bGeneratedOnDemandToc = true;

	return FIoStatus::Ok;
}

FIoStatus FOnDemandIoBackend::DownloadoadOnDemandToc(const FString& CdnUrl, const FString& TocPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::LoadOnDemandToc);

	UE_LOG(LogIas, Log, TEXT("Downloading OnDemandToc from CDN"));

	TIoStatusOr<FOnDemandToc> TocResult = LoadTocFromUrl(CdnUrl, TocPath, 1);
	if (TocResult.IsOk())
	{
		IoStore->AddToc(TocPath, TocResult.ConsumeValueOrDie());
		return FIoStatus::Ok;
	}
	else
	{
		return TocResult.Status();
	}
}

void FOnDemandIoBackend::AddDeferredTocs()
{
	TArray<FString> TocPaths;
	{
		FWriteScopeLock _(Lock);
		if (DeferredTocs.IsEmpty() || AvailableEps.HasCurrent() == false)
		{
			return;
		}
		TocPaths = MoveTemp(DeferredTocs);
	}

	// We are still reliant on a valid TocPath to generate the FOnDemandToc from disk as the path contains
	// info that we still need.
	if (!TocPaths.IsEmpty())
	{
		FIoStatus Result = ApplyGeneratedOnDemandToc(AvailableEps.GetCurrent(), TocPaths[0]);
		if (!Result.IsOk())
		{
			UE_LOG(LogIas, Error, TEXT("Failed to add generated toc', reason '%s'"), *Result.ToString());
		}
	}

	// Generating the FOnDemandToc is relying on DeferredTocs having entries and cribbing off the callback from resolving the CDN endpoint
	// This is to a) make it easier to toggle during development b) easier to validate the results.
	// So keeping that in mind we need to prevent the toc being downloaded if we are supposed to be using the disk generated version 
	// instead.
	// This can all be cleaned up when we either remove the toggle or drop the validation requirements AND clean up the initialization flow.
	if (!CVar_IasGenerateOnDemandToc->GetBool())
	{
		for (const FString& TocPath : TocPaths)
		{
			FIoStatus Result = DownloadoadOnDemandToc(AvailableEps.GetCurrent(), TocPath);
			if (!Result.IsOk())
			{
				UE_LOG(LogIas, Error, TEXT("Failed to add TOC '%s/%s', reason '%s'"), *AvailableEps.GetCurrent(), *TocPath, *Result.ToString());
			}
		}
	}
}

void FOnDemandIoBackend::Mount(const FOnDemandEndpoint& Endpoint)
{
	LLM_SCOPE_BYTAG(Ias);
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::Mount);

	if ((Endpoint.DistributionUrl.IsEmpty() && Endpoint.ServiceUrl.IsEmpty()) || Endpoint.TocPath.IsEmpty())
	{
		UE_LOG(LogIas, Error, TEXT("Trying to mount an invalid on demand endpoint"));
		return;
	}

	if (CVar_IasGenerateOnDemandToc->GetBool() && CVar_IasEnableAsyncTocGeneration->GetBool())
	{
		OnDemandTocTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [this]() -> TIoStatusOr<FOnDemandToc>
			{
				return GenerateOnDemandTocFromDisk();
			});
	}

	if (EnumHasAnyFlags(Endpoint.EndpointType, EOnDemandEndpointType::CDN))
	{
		{
			FWriteScopeLock _(Lock);
			if (Endpoint.ServiceUrl.IsEmpty())
			{
				if (AvailableEps.HasCurrent() == false)
				{
					if (DistributionUrl.IsEmpty())
					{
						DistributionUrl = Endpoint.DistributionUrl;
					}
					DeferredTocs.Add(Endpoint.TocPath);
					return;
				}
			}
			else if (AvailableEps.Urls.IsEmpty())
			{
				AvailableEps.Urls.Add(Endpoint.ServiceUrl.Replace(TEXT("https"), TEXT("http")));
				AvailableEps.Current = 0;
			}
		}

		check(AvailableEps.HasCurrent());

		FIoStatus GeneratedResult = ApplyGeneratedOnDemandToc(AvailableEps.GetCurrent(), Endpoint.TocPath);
		if (!GeneratedResult.IsOk())
		{
			UE_LOG(LogIas, Error, TEXT("Failed to add generated toc', reason '%s'"), *GeneratedResult.ToString());
		}

		if (!CVar_IasGenerateOnDemandToc->GetBool())
		{
			FIoStatus Result = DownloadoadOnDemandToc(AvailableEps.GetCurrent(), Endpoint.TocPath);
			if (!Result.IsOk())
			{
				UE_LOG(LogIas, Error, TEXT("Deferring TOC '%s/%s' due to '%s'"), *AvailableEps.GetCurrent(), *Endpoint.TocPath, *Result.ToString());
					BackendStatus.SetHttpError(true);
					FWriteScopeLock _(Lock);
					DeferredTocs.Add(Endpoint.TocPath);
			}
	}

		ConditionallyStartBackendThread();
	}
	else
	{
		UE_LOG(LogIas, Log, TEXT("Mounting ZEN endpoint, Url='%s'"), *Endpoint.ServiceUrl);
	}
}

void FOnDemandIoBackend::SetBulkOptionalEnabled(bool bEnabled)
{
	BackendStatus.SetHttpOptionalBulkEnabled(bEnabled);
}

void FOnDemandIoBackend::SetEnabled(bool bEnabled)
{
	BackendStatus.SetHttpEnabled(bEnabled);
}

void FOnDemandIoBackend::AbandonCache()
{
	BackendStatus.SetCacheEnabled(false);
	BackendStatus.SetAbandonCache(true);
}

void FOnDemandIoBackend::ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	if (!CVar_IoReportAnalytics.GetValueOnAnyThread())
	{
		return;
	}

	if (AvailableEps.HasCurrent())
	{
		FString CdnUrl = AvailableEps.GetCurrent();

		// Strip the prefix from the url as some analytics systems may have trouble dealing with it
		if (!CdnUrl.RemoveFromStart(TEXT("http://")))
		{
			CdnUrl.RemoveFromStart(TEXT("https://"));
		}

		AppendAnalyticsEventAttributeArray(OutAnalyticsArray, TEXT("IasCdnUrl"), MoveTemp(CdnUrl));

		Stats.ReportAnalytics(OutAnalyticsArray);
	}
}

void FOnDemandIoBackend::ProcessHttpRequests(FOnDemandHttpClient* HttpClient, FBitWindow& HttpErrors, int32 MaxConcurrentRequests)
{
	int32 NumConcurrentRequests = 0;
	FChunkRequest* NextChunkRequest = HttpRequests.Dequeue();

	while (NextChunkRequest)
	{
		while (NextChunkRequest)
		{
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::IssueHttpGet);
				FChunkRequest* ChunkRequest = NextChunkRequest;
				NextChunkRequest = ChunkRequest->NextRequest;
				ChunkRequest->NextRequest = nullptr;

				Stats.OnHttpDequeue();

				if (BackendStatus.IsHttpEnabled() == false)
				{
					UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
					{
						CompleteRequest(ChunkRequest);
					});
				}
				else
				{
					check(HttpClient);
					TAnsiStringBuilder<256> Url;
					ChunkRequest->Params.GetUrl(Url);

					NumConcurrentRequests++;
					HttpClient->Get(Url.ToView(), ChunkRequest->Params.ChunkRange,
						[this, &NextChunkRequest, ChunkRequest, &NumConcurrentRequests, &HttpErrors](TIoStatusOr<FIoBuffer> Status, uint64 DurationMs)
						{
							NumConcurrentRequests--;

							if (Status.Status().GetErrorCode() == EIoErrorCode::ReadError)
							{
								if (++ChunkRequest->HttpRetryCount <= GIasMaxHttpRetryCount)
								{
									Stats.OnHttpRetry();
									ChunkRequest->NextRequest = NextChunkRequest;
									NextChunkRequest = ChunkRequest;
									return;
								}
							}

							if (Status.IsOk())
							{
								HttpErrors.Add(false);
								ChunkRequest->Chunk = Status.ConsumeValueOrDie();
								Stats.OnHttpGet(ChunkRequest->Chunk.DataSize(), DurationMs);
							}
							else
							{
								Stats.OnHttpError();
								HttpErrors.Add(true);

								const float Average = HttpErrors.AvgSetBits();
								const bool bAboveHighWaterMark = Average > GIasHttpErrorHighWater;
								UE_LOG(LogIas, Warning, TEXT("%.2f%% the last %d HTTP requests failed"), Average * 100.0f, GIasHttpErrorSampleCount);

								if (bAboveHighWaterMark)
								{
									BackendStatus.SetHttpError(true);
									UE_LOG(LogIas, Warning, TEXT("HTTP streaming disabled due to high water mark of %.2f of the last %d requests reached"),
										GIasHttpErrorHighWater * 100.0f, GIasHttpErrorSampleCount);
								}
							}

							UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
							{
								CompleteRequest(ChunkRequest);
							});
						});
				}
			}

			if (NumConcurrentRequests >= MaxConcurrentRequests)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::TickHttpSaturated);
				while (NumConcurrentRequests >= MaxConcurrentRequests)
				{
					HttpClient->Tick(/*Block*/true);
				}
			}

			if (!NextChunkRequest)
			{
				NextChunkRequest = HttpRequests.Dequeue();
			}
		}

		{
			// Keep processing pending connections until all requests are completed or a new one is issued
			TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::TickHttp);
			while (HttpClient->Tick(/*Block*/false))
			{
				if (!NextChunkRequest)
				{
					NextChunkRequest = HttpRequests.Dequeue();
				}
				if (NextChunkRequest)
				{
					break;
				}
			}
		}
	} 
}

uint32 FOnDemandIoBackend::Run()
{
	LLM_SCOPE_BYTAG(Ias);

	FBitWindow HttpErrors;
	HttpErrors.Reset(GIasHttpErrorSampleCount);
	
	TUniquePtr<FOnDemandHttpClient> HttpClient;
	if (AvailableEps.HasCurrent())
	{
		HttpClient = MakeUnique<FOnDemandHttpClient>(AvailableEps.GetCurrent(), GIasMaxHttpConnectionCount);
#if !UE_BUILD_SHIPPING
		LatencyTest(AvailableEps.GetCurrent(), GetEndpointTestPath());
#endif 
	}

	while (!bStopRequested)
	{
		// Process HTTP request(s) even if the client is invalid to ensure enqueued request(s) gets completed.
		ProcessHttpRequests(HttpClient.Get(), HttpErrors, FMath::Min(2 * GIasMaxHttpConnectionCount, 64));

		if (!bStopRequested)
		{
			uint32 WaitTime = MAX_uint32;
			if (BackendStatus.IsHttpError())
			{
				WaitTime = GIasHttpHealthCheckWaitTime;
				if (HttpClient.IsValid())
				{
					HttpClient.Reset();
					HttpErrors.Reset(GIasHttpErrorSampleCount);
					AvailableEps.Current = INDEX_NONE;
				}

				UE_LOG(LogIas, Log, TEXT("Trying to reconnect to any available endpoint"));
				const FString TestPath = GetEndpointTestPath(); 
				if (int32 Idx = LatencyTest(AvailableEps.Urls, TestPath, bStopRequested); Idx != INDEX_NONE)
				{
					AvailableEps.Current = Idx;
					HttpClient = MakeUnique<FOnDemandHttpClient>(AvailableEps.GetCurrent(), GIasMaxHttpConnectionCount);
					BackendStatus.SetHttpError(false);
					UE_LOG(LogIas, Log, TEXT("Successfully reconnected to '%s'"), *AvailableEps.GetCurrent());
					AddDeferredTocs();
				}
			}
			if (BackendStatus.ShouldAbandonCache())
			{
				BackendStatus.SetAbandonCache(false);
				check(BackendStatus.IsCacheEnabled() == false);
				if (Cache.IsValid())
				{
					UE_LOG(LogIas, Log, TEXT("Abandoning cache, local file cache is no longer available"));
					Cache.Release()->Abandon(); // Will delete its self
				}
			}
			TickBackendEvent->Wait(WaitTime);
		}
	}

	return 0;
}

TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(TUniquePtr<IIasCache>&& Cache)
{
	return MakeShareable<IOnDemandIoDispatcherBackend>(new FOnDemandIoBackend(MoveTemp(Cache)));
}

} // namespace UE::IO::IAS

#undef UE_VALIDATE_GENERATED_TOC
