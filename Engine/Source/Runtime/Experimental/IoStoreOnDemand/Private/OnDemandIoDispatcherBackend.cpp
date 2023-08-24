// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandIoDispatcherBackend.h"

#include "AnalyticsEventAttribute.h"
#include "CancellationToken.h"
#include "Containers/StringView.h"
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
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "OnDemandHttpClient.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/MemoryReader.h"
#include "Statistics.h"
#include "Tasks/Task.h"

#include <atomic>

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

///////////////////////////////////////////////////////////////////////////////
#if !UE_BUILD_SHIPPING
static void LatencyTest(FStringView InUrl, FStringView InPath)
{
	auto AnsiUrl = StringCast<ANSICHAR>(InUrl.GetData(), InUrl.Len());

	using namespace UE::IO::IAS::HTTP;

	FConnectionPool::FParams PoolParams;
	PoolParams.SetHostFromUrl(AnsiUrl);
	PoolParams.ConnectionCount = 1;
	FConnectionPool Pool(PoolParams);

	TAnsiStringBuilder<256> AnsiPath;
	AnsiPath << "/";
	AnsiPath << InPath;

	FEventLoop Loop;
	int32 Results[4] = {};
	for (uint32 i = 0; i < UE_ARRAY_COUNT(Results); ++i)
	{
		bool Ok = false;

		FRequest Request = Loop.Request("HEAD", AnsiPath, Pool);
		Loop.Send(MoveTemp(Request), [&] (const FTicketStatus& Status)
		{
			if (Status.GetId() != FTicketStatus::EId::Response)
				return;

			const FResponse& Response = Status.GetResponse();
			Ok = (Response.GetStatus() == EStatusCodeClass::Successful);
		});

		uint64 Cycles = FPlatformTime::Cycles64();
		while (Loop.Tick(-1) != 0);
		Cycles = FPlatformTime::Cycles64() - Cycles;

		Results[i] = Ok ? int32(Cycles) : -1;
	}

	int64 Freq = int64(1.0 / FPlatformTime::GetSecondsPerCycle());
	for (int32& Result : Results)
	{
		if (Result == -1)
			continue;

		Result = int32((int64(Result) * 1000) / Freq);
	}

	UE_LOG(LogIas, VeryVerbose, TEXT("HEAD latencies (ms); %d %d %d %d (%s)"),
		Results[0], Results[1], Results[2], Results[3], InUrl.GetData());
}
#endif // !UE_BUILD_SHIPPING

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
		FIoHash Hash;
		uint64 RawSize = 0;
		uint64 EncodedSize = 0;
		uint32 BlockOffset = ~uint32(0);
		uint32 BlockCount = 0; 
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
	};

	struct FToc
	{
		FOnDemandEndpoint Endpoint;
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
	};

	FOnDemandIoStore();
	~FOnDemandIoStore();

	void AddToc(const FOnDemandEndpoint& Ep, FOnDemandToc&& Toc);
	TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& ChunkId);
	FChunkInfo GetChunkInfo(const FIoChunkId& ChunkId);

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

void FOnDemandIoStore::AddToc(const FOnDemandEndpoint& Ep, FOnDemandToc&& Toc)
{
	check(Ep.IsValid());
	UE_LOG(LogIas, Log, TEXT("Adding TOC '%s/%s'"), *Ep.ServiceUrl, *Ep.TocPath);

	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::AddToc);

	FString Prefix;
	{
		int32 Idx = INDEX_NONE;
		if (Ep.TocPath.FindLastChar(TCHAR('/'), Idx))
		{
			Prefix = Ep.TocPath.Left(Idx);
		}
	}

	{
		FWriteScopeLock _(Lock);

		const FOnDemandTocHeader& Header = Toc.Header;
		FToc* NewToc = new(Tocs) FToc{Ep};
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
				NewContainer->TocEntries.Add(TocEntry.ChunkId, FTocEntry
				{
					TocEntry.Hash,
					TocEntry.RawSize,
					TocEntry.EncodedSize,
					TocEntry.BlockOffset,
					TocEntry.BlockCount
				});
			}

			NewContainer->BlockSizes = MoveTemp(Container.BlockSizes);

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
			UE_LOG(LogIas, Log, TEXT("Mounting container '%s'"), *Container->Name);
			RegisteredContainers.Add(Container);
			It.RemoveCurrent();
		}
		else
		{
			FGuid KeyGuid;
			ensure(FGuid::Parse(Container->EncryptionKeyGuid, KeyGuid));
			if (const FAES::FAESKey* Key = FEncryptionKeyManager::Get().GetKey(KeyGuid))
			{
				UE_LOG(LogIas, Log, TEXT("Mounting container '%s'"), *Container->Name);
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
		const uint64 RawSize = FMath::Min(OffsetLength.GetLength(), ChunkInfo.Entry->RawSize);
		
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
		FIoChunkDecodingParams Params;
		Params.EncryptionKey = MakeMemoryView(ChunkInfo.Container->EncryptionKey.Key, FAES::FAESKey::KeySize);
		Params.CompressionFormat = ChunkInfo.Container->CompressionFormat;
		Params.BlockSize = ChunkInfo.Container->BlockSize;
		Params.TotalRawSize = ChunkInfo.Entry->RawSize;
		Params.EncodedBlockSize = ChunkInfo.GetBlocks(); 
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

///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoBackend final
	: public FRunnable
	, public IOnDemandIoDispatcherBackend
{
	using FIoRequestQueue = TThreadSafeIntrusiveQueue<FIoRequestImpl>;
	using FChunkRequestQueue = TThreadSafeIntrusiveQueue<FChunkRequest>;

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

	FOnDemandIoBackend(TSharedPtr<IIasCache> Cache);
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
	virtual FIoRequestImpl* GetCompletedRequests() override;
	virtual TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) override;

	// I/O Http backend
	virtual void Mount(const FOnDemandEndpoint& Endpoint) override;
	virtual void SetBulkOptionalEnabled(bool bInEnabled) override;
	virtual void SetEnabled(bool bInEnabled) override;
	virtual void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const override;

#if IS_PROGRAM || WITH_EDITOR
	virtual bool FlushDeferedEndPoints(double TimeOut = 0.0) override;
	virtual TArray<FIoChunkId> GetAllChunkIds() override;
#endif // IS_PROGRAM || WITH_EDITOR

	// Runnable
	virtual bool Init() override { return true; }
	virtual void Stop() override { bStopRequested = true; }
	virtual uint32 Run() override;

private:
	void CompleteRequest(FChunkRequest* ChunkRequest);
	FIoStatus MountDeferredEndpoints(const FString& DistributionUrl, const TConstArrayView<FString>& ServiceUrls);
	static TIoStatusOr<FOnDemandToc> GetToc(const FOnDemandEndpoint& Endpoint);
	FIoStatus AddToc(const FOnDemandEndpoint& Endpoint);
	bool IsHttpEnabled() const { return bHttpEnabled && GIasHttpEnabled; }
	bool IsHttpEnabled(const FIoChunkId& ChunkId) const
	{ 
		return (ChunkId.GetChunkType() != EIoChunkType::OptionalBulkData || (bHttpOptionalBulkDataEnabled && GIasHttpOptionalBulkDataEnabled));
	}
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const;

	TSharedPtr<IIasCache> Cache;
	TUniquePtr<FOnDemandIoStore> IoStore;
	TSharedPtr<const FIoDispatcherBackendContext> BackendContext;
	TUniquePtr<FRunnableThread> BackendThread;
	FEventRef TickBackendEvent;
	FDistributionEndpoints DistributionEndpoints;
	TArray<FOnDemandEndpoint> DeferredEndpoints;
	FChunkRequests ChunkRequests;
	FIoRequestQueue CompletedRequests;
	FChunkRequestQueue HttpRequests;
	TUniquePtr<FOnDemandHttpClient> HttpClient;
	FOnDemandIoBackendStats Stats;
	FRWLock Lock;
	std::atomic_bool bStopRequested{false};
	std::atomic_bool bHttpOptionalBulkDataEnabled{true};
	std::atomic_bool bHttpEnabled{true};
};

///////////////////////////////////////////////////////////////////////////////
FOnDemandIoBackend::FOnDemandIoBackend(TSharedPtr<IIasCache> InCache)
	: Cache(InCache)
{
	IoStore = MakeUnique<FOnDemandIoStore>();
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
	FGenericCrashContext::SetEngineData(TEXT("IAS.Enabled"), bHttpEnabled.load(std::memory_order_relaxed) ? TEXT("true") : TEXT("false"));
	BackendContext = Context;
	DistributionEndpoints.ResolveDeferredEndpoints();
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
	bool bCanCache = Cache.IsValid() && IsHttpEnabled();
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
			const uint64 RawSize = FMath::Min(Request->Options.GetSize(), ChunkRequest->Params.ChunkInfo.Entry->RawSize);
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
			bCanCache = false;
			Request->SetFailed();

			Stats.OnIoRequestError();
			LogIoResult(Request->ChunkId, ChunkRequest->Params.GetUrlHash(), DurationMs,
				0, Request->Options.GetOffset(),
				ChunkRequest->Params.ChunkRange.GetOffset(), ChunkRequest->Priority, ChunkRequest->bCached);
		}

		CompletedRequests.Enqueue(Request);
		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}

	if (bCanCache && !ChunkRequest->bCached && Chunk.GetSize() > 0)
	{
		Cache->Put(ChunkRequest->Params.ChunkKey, Chunk);
	}

	ChunkRequests.Release(ChunkRequest);
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

	if (IsHttpEnabled() == false || IsHttpEnabled(Request->ChunkId) == false)
	{ 
		// Allow reading from the cache only when HTTP streaming is disabled otherwise the chunk may be evicted
		// before trying to read the cache entry.
		if (IsHttpEnabled() || Cache.IsValid() == false || Cache->ContainsChunk(RequestParams.ChunkKey) == false)
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
		//TODO: Pass priority to cache
		ChunkRequest->CacheTask = Cache->Get(ChunkRequest->Params.ChunkKey, FIoReadOptions(), &ChunkRequest->CancellationToken);
	}

	const ETaskPriority TaskPriority = ChunkRequest->Priority > IoDispatcherPriority_Medium ? ETaskPriority::High : ETaskPriority::Normal;
	Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
	{
		LLM_SCOPE_BYTAG(Ias);
		TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::CompleteOrEnqueueHttpRequest);
		if (ChunkRequest->CacheTask.IsValid())
		{
			if (TIoStatusOr<FIoBuffer> Status = ChunkRequest->CacheTask.GetResult(); Status.IsOk())
			{
				ChunkRequest->Chunk = Status.ConsumeValueOrDie();
				ChunkRequest->bCached = true;
				return CompleteRequest(ChunkRequest);
			}
		}

		const bool bHttpDisabled = IsHttpEnabled() == false || IsHttpEnabled(ChunkRequest->GetChunkId()) == false;
		if (bHttpDisabled || ChunkRequest->CancellationToken.IsCancelled())
		{
			UE_CLOG(bHttpDisabled, LogIas, Log, TEXT("Chunk was not found in the cache and HTTP is disabled"));
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

	FIoOffsetAndLength RequestedRange(ChunkRange.GetOffset(), FMath::Min(ChunkInfo.Entry->RawSize, ChunkRange.GetLength()));
	OutAvailable = ChunkInfo.Entry->RawSize;

	if (IsHttpEnabled() == false || IsHttpEnabled(ChunkId) == false)
	{
		if (IsHttpEnabled() || Cache.IsValid() == false)
		{
			return FIoStatus(EIoErrorCode::UnknownChunkID);
		}

		// When HTTP streaming is disabled, no cache entries will be evicted. 
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

FIoStatus FOnDemandIoBackend::MountDeferredEndpoints(const FString& DistributionUrl, const TConstArrayView<FString>& ServiceUrls)
{
	TArray<FOnDemandEndpoint, TInlineAllocator<4>> EndpointsToAdd;
	{
		FWriteScopeLock _(Lock);
		for (auto It = DeferredEndpoints.CreateIterator(); It; ++It)
		{
			if (It->DistributionUrl.Compare(DistributionUrl, ESearchCase::IgnoreCase) == 0)
			{
				EndpointsToAdd.Add(*It);
				It.RemoveCurrent();
			}
		}
	}

	for (const FOnDemandEndpoint& Ep : EndpointsToAdd)
	{
		FIoStatus Status;
		for (const FString& SerivceUrl : ServiceUrls)
		{
			// Currently we don't need use secure sockets to fetch on demand content
			FString UnsecureUrl = SerivceUrl.Replace(TEXT("https"), TEXT("http"));
			Status = AddToc(FOnDemandEndpoint{Ep.EndpointType, DistributionUrl, UnsecureUrl, Ep.TocPath});
			if (Status.IsOk())
			{
				break;
			}
		}

		if (!Status.IsOk())
		{
			return Status;
		}
	}

	return FIoStatus::Ok;
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

	if (EnumHasAnyFlags(Endpoint.EndpointType, EOnDemandEndpointType::CDN))
	{
		if (Endpoint.ServiceUrl.IsEmpty())
		{
			{
				FWriteScopeLock _(Lock);
				DeferredEndpoints.Add(Endpoint);
			}

			DistributionEndpoints.ResolveEndpoints(Endpoint.DistributionUrl, [this](const FString& DistributionUrl, TConstArrayView<FString> SerivceUrls)
			{
				if (FIoStatus Status = MountDeferredEndpoints(DistributionUrl, SerivceUrls); !Status.IsOk())
				{
					UE_LOG(LogIas, Error, TEXT("Failed to add endpoint(s), reason '%s'"), *Status.ToString());
				}
			});
		}
		else if (FIoStatus Status = AddToc(Endpoint); !Status.IsOk())
		{
			UE_LOG(LogIas, Error, TEXT("Failed to add TOC '%s/%s', reason '%s'"),
				*Endpoint.ServiceUrl, *Endpoint.TocPath, *Status.ToString());
		}
	}
	else
	{
		UE_LOG(LogIas, Log, TEXT("Mounting ZEN endpoint, Url='%s'"), *Endpoint.ServiceUrl);
	}
}

void FOnDemandIoBackend::SetBulkOptionalEnabled(bool bInEnabled)
{
	UE_LOG(LogIas, Log, TEXT("HTTP optional bulk data streaming '%s'"), bInEnabled ? TEXT("Enabled") : TEXT("Disabled"));
	bHttpOptionalBulkDataEnabled = bInEnabled;
}

void FOnDemandIoBackend::SetEnabled(bool bInEnabled)
{
	UE_LOG(LogIas, Log, TEXT("HTTP streaming '%s'"), bInEnabled ? TEXT("Enabled") : TEXT("Disabled"));
	bHttpEnabled = bInEnabled;
	FGenericCrashContext::SetEngineData(TEXT("IAS.Enabled"), bHttpEnabled.load(std::memory_order_acquire) ? TEXT("true") : TEXT("false"));
}

void FOnDemandIoBackend::ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	if (!CVar_IoReportAnalytics.GetValueOnAnyThread())
	{
		return;
	}

	if (HttpClient.IsValid())
	{
		AppendAnalyticsEventAttributeArray(OutAnalyticsArray, TEXT("IasCDNBackend"), HttpClient->ServiceUrl());

		Stats.ReportAnalytics(OutAnalyticsArray);
	}
}

#if IS_PROGRAM || WITH_EDITOR
bool FOnDemandIoBackend::FlushDeferedEndPoints(double TimeOut)
{
	return DistributionEndpoints.Flush(TimeOut);
}

TArray<FIoChunkId> FOnDemandIoBackend::GetAllChunkIds()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::GetAllChunkIds);

	if (!IoStore.IsValid() || !IsHttpEnabled())
	{
		return TArray<FIoChunkId>();
	}

	const bool bAllowOptional = GIasHttpOptionalBulkDataEnabled && bHttpOptionalBulkDataEnabled;

	return IoStore->GetAllChunkIds(bAllowOptional);
}

#endif // IS_PROGRAM || WITH_EDITOR

TIoStatusOr<FOnDemandToc> FOnDemandIoBackend::GetToc(const FOnDemandEndpoint& Endpoint)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::GetToc);

	for (int32 Attempt = 0, MaxAttempts = GIasMaxHttpRetryCount; Attempt <= MaxAttempts; ++Attempt)
	{
		TUniquePtr<FOnDemandHttpClient> HttpClient = MakeUnique<FOnDemandHttpClient>(Endpoint.ServiceUrl, GIasMaxHttpConnectionCount);
		TAnsiStringBuilder<256> Url;

		Url << "/" << Endpoint.TocPath;
		UE_LOG(LogIas, Log, TEXT("Fetching TOC '%s/%s' (#%d/%d)"), *HttpClient->ServiceUrl(), *Endpoint.TocPath, Attempt + 1, MaxAttempts);
		
		TIoStatusOr<FOnDemandToc> Toc;
		HttpClient->Get(Url.ToView(), [&Toc](TIoStatusOr<FIoBuffer> Response, uint64 DurationMs)
		{
			if (Response.IsOk())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::SerializeToc);
				FIoBuffer Buffer = Response.ConsumeValueOrDie();
				FOnDemandToc NewToc;	

				FMemoryReaderView Ar(Buffer.GetView());
				Ar << NewToc;
				if (!Ar.IsError())
				{
					Toc = TIoStatusOr<FOnDemandToc>(MoveTemp(NewToc));
				}
				else
				{
					UE_LOG(LogIas, Error, TEXT("Failed loading on demand TOC from compact binary"));
				}
			}
			else
			{
				UE_LOG(LogIas, Error, TEXT("Failed fetching TOC, reason '%s'"), *Response.Status().ToString());
			}
		});

		const bool bBlock = true;
		while (HttpClient->Tick(bBlock));

		if (Toc.IsOk())
		{
			return Toc;
		}
	}

	return TIoStatusOr<FOnDemandToc>(FIoStatus(EIoErrorCode::NotFound));
}

FIoStatus FOnDemandIoBackend::AddToc(const FOnDemandEndpoint& Endpoint)
{
	TIoStatusOr<FOnDemandToc> Toc = GetToc(Endpoint);
	if (!Toc.IsOk())
	{
		return FIoStatus(Toc.Status());
	}

	{
		FWriteScopeLock _(Lock);
		if (!HttpClient.IsValid())
		{
			HttpClient = MakeUnique<FOnDemandHttpClient>(Endpoint.ServiceUrl, GIasMaxHttpConnectionCount);
			BackendThread.Reset(FRunnableThread::Create(this, TEXT("IoStoreOnDemand"), 0, TPri_AboveNormal));
		}
	}

#if !UE_BUILD_SHIPPING
	UE::Tasks::Launch(TEXT("IasLatencyTest"), [ServiceUrl=Endpoint.ServiceUrl, TocPath=Endpoint.TocPath] ()
	{
		LatencyTest(ServiceUrl, TocPath);
	});
#endif // !UE_BUILD_SHIPPING

	IoStore->AddToc(Endpoint, Toc.ConsumeValueOrDie());

	UE_CLOG(Endpoint.ServiceUrl !=  HttpClient->ServiceUrl(),
		LogIas, Fatal, TEXT("Fetching on demand content from multiple endpoints are currently not supported"));

	return FIoStatus::Ok;
}

uint32 FOnDemandIoBackend::Run()
{
	LLM_SCOPE_BYTAG(Ias);

	const int32 MaxConcurrentRequests = HttpClient->MaxConnectionCount();
	FChunkRequest* NextChunkRequest = nullptr;
	int32 NumConcurrentRequests = 0;

	while (!bStopRequested)
	{
		NextChunkRequest = HttpRequests.Dequeue();
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

					TAnsiStringBuilder<256> Url;
					ChunkRequest->Params.GetUrl(Url);

					NumConcurrentRequests++;
					HttpClient->Get(Url.ToView(), ChunkRequest->Params.ChunkRange,
						[this, ChunkRequest, &NumConcurrentRequests](TIoStatusOr<FIoBuffer> Status, uint64 DurationMs)
						{
							NumConcurrentRequests--;

							if (Status.Status().GetErrorCode() == EIoErrorCode::ReadError)
							{
								if (++ChunkRequest->HttpRetryCount <= GIasMaxHttpRetryCount)
								{
									Stats.OnHttpRetry();
									Stats.OnHttpEnqueue();

									// Note there is no need to trigger TickBackendEvent as this callback will occur within FOnDemandIoBackend::Run
									ChunkRequest->Priority = IoDispatcherPriority_High;
									return HttpRequests.EnqueueByPriority(ChunkRequest);
								}
							}

							if (Status.IsOk())
							{
								ChunkRequest->Chunk = Status.ConsumeValueOrDie();
								Stats.OnHttpGet(ChunkRequest->Chunk.DataSize(), DurationMs);
							}
							else
							{
								Stats.OnHttpError();
							}

							UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
							{
								CompleteRequest(ChunkRequest);
							});
						});
				}

				if (NumConcurrentRequests >= MaxConcurrentRequests)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::TickHttpSaturated);
					while (NumConcurrentRequests >= MaxConcurrentRequests)
					{
						HttpClient->Tick(true);
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
				while (HttpClient->Tick(true) && !NextChunkRequest)
				{
					NextChunkRequest = HttpRequests.Dequeue();
				}

				// Tick can cause a new request to be added to HttpRequests, so we should try one last time
				if (NextChunkRequest == nullptr)
				{
					NextChunkRequest = HttpRequests.Dequeue();
				}
			}
		}

		if (!bStopRequested)
		{
			TickBackendEvent->Wait();
		}
	}

	return 0;
}

TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(TSharedPtr<IIasCache> Cache)
{
	return MakeShareable<IOnDemandIoDispatcherBackend>(new FOnDemandIoBackend(Cache));
}

} // namespace UE::IO::IAS
