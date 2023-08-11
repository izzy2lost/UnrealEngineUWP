// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandIoDispatcherBackend.h"
#include "Statistics.h"

#include "CancellationToken.h"
#include "Containers/StringView.h"
#include "CoreHttp/Client.h"
#include "EncryptionKeyManager.h"
#include "FileIoCache.h"
#include "HAL/Event.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/Platform.h"
#include "HAL/PlatformTime.h"
#include "HAL/PreprocessorHelpers.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Http.h"
#include "HttpManager.h"
#include "IO/IoAllocators.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoDispatcher.h"
#include "IO/IoOffsetLength.h"
#include "IO/IoStatus.h"
#include "IO/IoStore.h"
#include "IO/IoStoreOnDemand.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Modules/ModuleManager.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"
#include "Tasks/Task.h"

#include <atomic>

///////////////////////////////////////////////////////////////////////////////
int32 GIoDispatcherMaxHttpConnectionCount = 8;
static FAutoConsoleVariableRef CVar_IoDispatcherMaxHttpConnectionCount (
	TEXT("ias.MaxHttpConnectionCount"),
	GIoDispatcherMaxHttpConnectionCount,
	TEXT("Max number of open HTTP connections to the on demand endpoint(s).")
);

int32 GIoDispatcherMaxHttpRetryCount = 2;
static FAutoConsoleVariableRef CVar_IoDispatcherMaxHttpRetryCount (
	TEXT("ias.MaxHttpRetryCount"),
	GIoDispatcherMaxHttpRetryCount,
	TEXT("Max number of HTTP request retries before failing the I/O request.")
);

int32 GIoDispatcherHttpPollTimeoutMs = 0;
static FAutoConsoleVariableRef CVar_IoDispatcherMaxHttpPollTimeoutMs (
	TEXT("ias.HttpPollTimeout"),
	GIoDispatcherHttpPollTimeoutMs,
	TEXT("Tick() poll timeout in milliseconds")
);

bool GIoDispatcherBulkOptionalEnabled = true;
static FAutoConsoleVariableRef CVar_IoDispatcherBulkOptionalEnabled(
	TEXT("ias.BulkOptionalEnabled"),
	GIoDispatcherBulkOptionalEnabled,
	TEXT("Enables bulk optional requests.")
);

namespace UE::IO::Private
{

///////////////////////////////////////////////////////////////////////////////
static void LogHttpResult(const TCHAR* Url, uint32 StatusCode, uint32 Duration, uint32 Size, uint32 Offset, const char* Memo="ok")
{
	Size >>= 10;
	UE_LOG(LogIas, VeryVerbose, TEXT("http-%3u: %5ums %5uKiB [%7u] '%S' %s"), StatusCode, Duration, Size, Offset, Memo, Url);
};

using namespace UE::Tasks;

///////////////////////////////////////////////////////////////////////////////
FIoHash GetChunkKey(const FIoHash& ChunkHash, const FIoOffsetAndLength& Range)
{
	FIoHashBuilder HashBuilder;
	HashBuilder.Update(ChunkHash.GetBytes(), sizeof(FIoHash::ByteArray));
	HashBuilder.Update(&Range, sizeof(FIoOffsetAndLength));

	return HashBuilder.Finalize();
}

///////////////////////////////////////////////////////////////////////////////
class FDistributionEndpoints
{
public:
	using FOnEndpointResolved = TFunction<void(const FString&, TConstArrayView<FString>)>;
	
	FDistributionEndpoints() = default;
	~FDistributionEndpoints();

	void ResolveEndpoints(const FString& DistributionUrl, FOnEndpointResolved&& OnResolved);
	void ResolveDeferredEndpoints();

private:
	struct FResolvedEndpoint
	{
		TArray<FString> ServiceUrls;
	};

	struct FResolveRequest
	{
		FString DistributionUrl;
		FHttpRequestPtr HttpRequest;
		TArray<FOnEndpointResolved> Callbacks;
		int32 RetryCount = 0;
	};

	void IssueEndpointRequests();
	void CancelEndpointRequests();
	void CompleteEndpointRequest(FResolveRequest& ResolveRequest, FHttpResponsePtr HttpResponse);

	TMap<FString, TUniquePtr<FResolvedEndpoint>> ResolvedEndpoints;
	TMap<FString, TUniquePtr<FResolveRequest>> PendingRequests;
	FRWLock Lock;
	bool bInitialized = false;
};

FDistributionEndpoints::~FDistributionEndpoints()
{
	CancelEndpointRequests();
}

void FDistributionEndpoints::ResolveEndpoints(const FString& DistributionUrl, FOnEndpointResolved&& OnResolved)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::ResolveEndpoints);
	const FResolvedEndpoint* Ep = nullptr;
	{
		FReadScopeLock _(Lock);
		if (TUniquePtr<FResolvedEndpoint>* Entry = ResolvedEndpoints.Find(DistributionUrl))
		{
			Ep = Entry->Get();
		}
	}

	if (Ep)
	{
		return OnResolved(DistributionUrl, Ep->ServiceUrls);
	}

	bool bIssueRequest = false;
	{
		FWriteScopeLock _(Lock);
		TUniquePtr<FResolveRequest>& Request = PendingRequests.FindOrAdd(DistributionUrl);
		if (!Request.IsValid())
		{
			Request.Reset(new FResolveRequest{DistributionUrl});
			bIssueRequest = bInitialized;
		}
		Request->Callbacks.Add(MoveTemp(OnResolved));
	}

	if (bIssueRequest)
	{
		IssueEndpointRequests();
	}
}

void FDistributionEndpoints::ResolveDeferredEndpoints()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::ResolveDeferredEndpoints);
	{
		FWriteScopeLock _(Lock);
		bInitialized = true;
	}

	IssueEndpointRequests();
}

void FDistributionEndpoints::IssueEndpointRequests()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::IssueEndpointRequests);
	// Currently we need to use the HTTP module in order to resolve service endpoints due to HTTPS
	FHttpModule& HttpModule = FModuleManager::LoadModuleChecked<FHttpModule>("HTTP");
	const int32 MaxAttempts = GIoDispatcherMaxHttpRetryCount;

	TArray<FHttpRequestPtr, TInlineAllocator<2>> HttpRequests;
	{
		FWriteScopeLock _(Lock);
		check(bInitialized);

		for (auto& Kv : PendingRequests)
		{
			if (Kv.Value->HttpRequest.IsValid())
			{
				continue;
			}

			FResolveRequest& ResolveRequest = *Kv.Value.Get();
			UE_LOG(LogIas, Log, TEXT("Resolving '%s' (#%d/%d)"), *ResolveRequest.DistributionUrl, ResolveRequest.RetryCount + 1, MaxAttempts);

			FHttpRequestPtr HttpRequest = HttpModule.Get().CreateRequest();
			HttpRequest->SetTimeout(3.0f);
			HttpRequest->SetURL(Kv.Key);
			HttpRequest->SetVerb(TEXT("GET"));
			HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
			HttpRequest->OnProcessRequestComplete().BindLambda(
				[this, &ResolveRequest, MaxAttempts]
				(FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
				{
					LLM_SCOPE(ELLMTag::FileSystem);
					FHttpRequestPtr Request = MoveTemp(ResolveRequest.HttpRequest);
					if (Response->GetResponseCode() != 200)
					{
						if (++ResolveRequest.RetryCount <= MaxAttempts)
						{
							Request->OnProcessRequestComplete().Unbind();
							return IssueEndpointRequests();
						}
					}

					CompleteEndpointRequest(ResolveRequest, Response);
				});

			ResolveRequest.HttpRequest = HttpRequest;
			HttpRequests.Add(HttpRequest);
		}
	}

	for (FHttpRequestPtr& Request : HttpRequests)
	{
		Request->ProcessRequest();
	}
}

void FDistributionEndpoints::CancelEndpointRequests()
{
	TArray<FHttpRequestPtr, TInlineAllocator<2>> HttpRequests;
	{
		FWriteScopeLock _(Lock);
		for (auto& Kv : PendingRequests)
		{
			if (Kv.Value->HttpRequest.IsValid())
			{
				HttpRequests.Add(Kv.Value->HttpRequest);
			}
		}
	}

	if (!HttpRequests.IsEmpty())
	{
		FHttpModule& HttpModule = FModuleManager::LoadModuleChecked<FHttpModule>("HTTP");
		for (FHttpRequestPtr& Request : HttpRequests)
		{
			HttpModule.GetHttpManager().RemoveRequest(Request.ToSharedRef());
			//TODO: Flush?
		}
	}
}

void FDistributionEndpoints::CompleteEndpointRequest(FResolveRequest& ResolveRequest, FHttpResponsePtr HttpResponse)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::CompleteEndpointRequest);

	using FJsonValuePtr = TSharedPtr<FJsonValue>;
	using FJsonObjPtr = TSharedPtr<FJsonObject>;
	using FJsonReader = TJsonReader<TCHAR>;
	using FJsonReaderPtr = TSharedRef<FJsonReader>;

	TArray<FString> ServiceUrls;
	if (HttpResponse->GetResponseCode() == 200)
	{
		FString Json = HttpResponse->GetContentAsString();
		FJsonReaderPtr JsonReader = TJsonReaderFactory<TCHAR>::Create(Json);

		FJsonObjPtr JsonObj;
		if (FJsonSerializer::Deserialize(JsonReader, JsonObj))
		{
			TArray<FJsonValuePtr> JsonValues = JsonObj->GetArrayField(TEXT("distributions"));
			for (const FJsonValuePtr& JsonValue : JsonValues)
			{
				FString ServiceUrl = JsonValue->AsString();
				if (ServiceUrl.EndsWith(TEXT("/")))
				{
					ServiceUrl.LeftInline(ServiceUrl.Len() - 1);
				}
				ServiceUrls.Add(MoveTemp(ServiceUrl));
			}
		}
	}

	const FResolvedEndpoint* ResolvedEndpoint = nullptr;
	FString DistributionUrl;
	TArray<FOnEndpointResolved> Callbacks;

	{
		FWriteScopeLock _(Lock);
		if (!ServiceUrls.IsEmpty())
		{
			ResolvedEndpoint = ResolvedEndpoints.Emplace(
				ResolveRequest.DistributionUrl,
				new FResolvedEndpoint{MoveTemp(ServiceUrls)})
			.Get();
		}

		Callbacks = MoveTemp(ResolveRequest.Callbacks);
		DistributionUrl = MoveTemp(ResolveRequest.DistributionUrl);
		PendingRequests.Remove(DistributionUrl);
	}

	TConstArrayView<FString> Urls = ResolvedEndpoint ? ResolvedEndpoint->ServiceUrls : TConstArrayView<FString>();
	for (FOnEndpointResolved& Callback : Callbacks)
	{
		Callback(DistributionUrl, Urls);
	}
}

///////////////////////////////////////////////////////////////////////////////
class FHttpClient
{
public:
	FHttpClient(const FString& ServiceUrl, int32 MaxConnectionCount = 8);
	~FHttpClient() = default;

	const FString& ServiceUrl() const { return SvcsUrl; }
	int32 MaxConnectionCount() const { return MaxConnections;}
	void Get(FAnsiStringView Url, FIoReadCallback&& Callback);
	void Get(FAnsiStringView Url, const FIoOffsetAndLength& Range, FIoReadCallback&& Callback);

	/** @return True if the client has pending work otherwise false. */
	bool Tick();

private:
	void Issue(FAnsiStringView Url, FIoReadCallback&& Callback, FIoOffsetAndLength Range = FIoOffsetAndLength());

	FString SvcsUrl;
	int32 MaxConnections;
	HTTP::FEventLoop EventLoop;
	TUniquePtr<HTTP::FConnectionPool> ConnectionPool;
};

FHttpClient::FHttpClient(const FString& ServiceUrl, int32 MaxConnectionCount)
	: SvcsUrl(ServiceUrl)
	, MaxConnections(MaxConnectionCount)
{
	HTTP::FConnectionPool::FParams Params;
	Params.SetHostFromUrl(StringCast<ANSICHAR>(*ServiceUrl).Get());
	Params.ConnectionCount = MaxConnectionCount;
	ConnectionPool = MakeUnique<HTTP::FConnectionPool>(Params);
}

void FHttpClient::Get(FAnsiStringView Url, const FIoOffsetAndLength& Range, FIoReadCallback&& Callback)
{
	Issue(Url, MoveTemp(Callback), Range);
}

void FHttpClient::Get(FAnsiStringView Url, FIoReadCallback&& Callback)
{
	Issue(Url, MoveTemp(Callback));
}

void FHttpClient::Issue(FAnsiStringView Url, FIoReadCallback&& Callback, FIoOffsetAndLength Range)
{
	using namespace UE::HTTP;

	auto Sink = [
		Buffer = FIoBuffer(),
		Callback = MoveTemp(Callback),
		Url = FString(Url),
		Offset = Range.GetOffset(),
		StartTime = FPlatformTime::Cycles64(),
		StatusCode = uint32(0)]
		(const FTicketStatus& Status) mutable
		{ 
			if (FTicketStatus::EId::Response == Status.GetId())
			{
				FResponse& Response = Status.GetResponse();
				StatusCode = Response.GetStatusCode();
				Response.SetDestination(&Buffer);
			}
			else if (FTicketStatus::EId::Content == Status.GetId())
			{
				const uint64 Duration = (uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime);
				const FIoBuffer& Content = Status.GetContent(); 

				LogHttpResult(*Url, StatusCode, Duration, Content.GetSize(), Offset);

				const bool bSuccessful = StatusCode > 199 && StatusCode < 300;
				if (bSuccessful && Content.GetSize() > 0)
				{
					Callback(Content);
				}
				else
				{
					Callback(FIoStatus(EIoErrorCode::NotFound, TEXTVIEW("Invalid Content")));
				}
			}
			else if (FTicketStatus::EId::Error == Status.GetId())
			{
				const uint64 Duration = (uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime);
				LogHttpResult(*Url, StatusCode, Duration, 0, Offset, Status.GetErrorReason());
				Callback(FIoStatus(EIoErrorCode::ReadError, FString(Status.GetErrorReason())));
			}
		};

	UE::HTTP::FRequest Request = EventLoop.Get(Url, *ConnectionPool);
	const uint64 RangeStart = Range.GetOffset();
	const uint64 RangeEnd = Range.GetOffset() + Range.GetLength();
	if (RangeStart > 0 || RangeEnd > 0)
	{
		Request.Header(ANSITEXTVIEW("Range"), WriteToAnsiString<64>(ANSITEXTVIEW("bytes="), RangeStart, ANSITEXTVIEW("-"), RangeEnd));
	}
	
	EventLoop.Send(MoveTemp(Request), MoveTemp(Sink));
}

bool FHttpClient::Tick()
{
	return EventLoop.Tick(GIoDispatcherHttpPollTimeoutMs) != 0;
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
	LLM_SCOPE(ELLMTag::FileSystem);
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
	static FChunkRequestParams Create(FIoRequestImpl* Request, FOnDemandIoStore::FChunkInfo ChunkInfo)
	{
		check(Request);
		check(Request->NextRequest == nullptr);
		const uint64 RawSize = FMath::Min(Request->Options.GetSize(), ChunkInfo.Entry->RawSize);
		
		const FIoOffsetAndLength ChunkRange = FIoChunkEncoding::GetChunkRange(
			ChunkInfo.Entry->RawSize,
			ChunkInfo.Container->BlockSize,
			ChunkInfo.GetBlocks(),
			Request->Options.GetOffset(),
			RawSize).ConsumeValueOrDie();

		return FChunkRequestParams{GetChunkKey(ChunkInfo.Entry->Hash, ChunkRange), ChunkRange, ChunkInfo};
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

	double DurationInSeconds() const
	{
		return FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartTime);
	}

	FChunkRequest* NextRequest;
	FChunkRequestParams Params;
	FIoRequestImpl* RequestHead;
	FIoRequestImpl* RequestTail;
	FIoBuffer Chunk;
	TTask<TIoStatusOr<FIoBuffer>> CacheTask;
	FTask DecodeTask;
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
	uint32 Duration,
	uint32 UncompressedSize,
	uint32 UncompressedOffset,
	uint32 CompressedOffset,
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
	UE_LOG(LogIas, VeryVerbose, TEXT("%s: %5ums %5uKiB [%7u] %s:%s|%u (%d)"),
		Prefix,
		Duration,
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
	, public UE::IOnDemandIoDispatcherBackend
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

	FOnDemandIoBackend(TSharedPtr<IIoCache> Cache);
	virtual ~FOnDemandIoBackend();

	// I/O dispatcher backend
	virtual void Initialize(TSharedRef<const FIoDispatcherBackendContext> Context) override;
	virtual void Shutdown() override;
	virtual bool Resolve(FIoRequestImpl* Request) override;
	virtual void CancelIoRequest(FIoRequestImpl* Request) override;
	virtual void UpdatePriorityForIoRequest(FIoRequestImpl* Request) override;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId) const override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId) const override;
	virtual FIoRequestImpl* GetCompletedRequests() override;
	virtual TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) override;

	// I/O Http backend
	virtual void Mount(const FOnDemandEndpoint& Endpoint) override;
	virtual void SetBulkOptionalEnabled(bool bInEnabled) override;
	virtual void SetEnabled(bool bInEnabled) override;

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
	static TIoStatusOr<FOnDemandToc> GetToc(FHttpClient& HttpClient, const FString& TocPath);
	FIoStatus AddToc(const FOnDemandEndpoint& Endpoint);

	TSharedPtr<IIoCache> Cache;
	TUniquePtr<FOnDemandIoStore> IoStore;
	TSharedPtr<const FIoDispatcherBackendContext> BackendContext;
	TUniquePtr<FRunnableThread> BackendThread;
	FEventRef TickBackendEvent;
	FDistributionEndpoints DistributionEndpoints;
	TArray<FOnDemandEndpoint> DeferredEndpoints;
	FChunkRequests ChunkRequests;
	FIoRequestQueue CompletedRequests;
	FChunkRequestQueue HttpRequests;
	TUniquePtr<FHttpClient> HttpClient;
	FOnDemandIoBackendStats Stats;
	FRWLock Lock;
	std::atomic_bool bStopRequested{false};
	std::atomic_bool bEnableBulkOptional{true};
	std::atomic_bool bEnabled{true};
};

///////////////////////////////////////////////////////////////////////////////
FOnDemandIoBackend::FOnDemandIoBackend(TSharedPtr<IIoCache> InCache)
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
	LLM_SCOPE(ELLMTag::FileSystem);
	UE_LOG(LogIas, Log, TEXT("Initializing on demand I/O dispatcher backend"));
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
	LLM_SCOPE(ELLMTag::FileSystem);
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

	bool bCanCache = Cache.IsValid();
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
		
		const uint64 Duration = Request->GetStartTime() > 0 ?
			(uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Request->GetStartTime()) : 0;

		if (bDecoded)
		{
			Stats.OnIoRequestComplete(Request->GetBuffer().GetSize());
			LogIoResult(Request->ChunkId, ChunkRequest->Params.GetUrlHash(), Duration,
				Request->GetBuffer().DataSize(), Request->Options.GetOffset(),
				ChunkRequest->Params.ChunkRange.GetOffset(), ChunkRequest->Priority, ChunkRequest->bCached);
				
		}
		else
		{
			bCanCache = false;
			Request->SetFailed();

			Stats.OnIoRequestFail();
			LogIoResult(Request->ChunkId, ChunkRequest->Params.GetUrlHash(), Duration,
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

	Stats.OnChunkRequestRelease();
	ChunkRequests.Release(ChunkRequest);
}

bool FOnDemandIoBackend::Resolve(FIoRequestImpl* Request)
{
	using namespace UE::Tasks;

	if (!bEnabled)
	{
		return false;
	}
	
	FOnDemandIoStore::FChunkInfo ChunkInfo = IoStore->GetChunkInfo(Request->ChunkId);
	if (!ChunkInfo.IsValid())
	{
		return false;
	}

	if ((!GIoDispatcherBulkOptionalEnabled || !bEnableBulkOptional) && Request->ChunkId.GetChunkType() == EIoChunkType::OptionalBulkData)
	{
		return false;
	}

	Stats.OnIoRequestEnqueue();
	FChunkRequestParams RequestParams = FChunkRequestParams::Create(Request, ChunkInfo);

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

	Stats.OnChunkRequestCreate();
		
	if (Cache.IsValid())
	{
		//TODO: Pass priority to cache
		ChunkRequest->CacheTask = Cache->Get(ChunkRequest->Params.ChunkKey, FIoReadOptions(), &ChunkRequest->CancellationToken);
	}

	const ETaskPriority TaskPriority = ChunkRequest->Priority > IoDispatcherPriority_Medium ? ETaskPriority::High : ETaskPriority::Normal;
	Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
	{
		LLM_SCOPE(ELLMTag::FileSystem);
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

		if (ChunkRequest->CancellationToken.IsCancelled())
		{
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
	TIoStatusOr<uint64> ChunkSize = GetSizeForChunk(ChunkId);
	return ChunkSize.IsOk();
}

TIoStatusOr<uint64> FOnDemandIoBackend::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	if (IoStore.IsValid())
	{
		if (!bEnabled)
		{
			return FIoStatus(EIoErrorCode::UnknownChunkID);
		}

		if ((!GIoDispatcherBulkOptionalEnabled || !bEnableBulkOptional) && ChunkId.GetChunkType() == EIoChunkType::OptionalBulkData)
		{
			return FIoStatus(EIoErrorCode::UnknownChunkID);
		}

		return IoStore->GetChunkSize(ChunkId);
	}

	return FIoStatus(EIoErrorCode::UnknownChunkID);
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
			// Currenlty we don't need use secure sockets to fetch on demand content
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
	LLM_SCOPE(ELLMTag::FileSystem);
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
	bEnableBulkOptional = bInEnabled;
}

void FOnDemandIoBackend::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
}

#if IS_PROGRAM || WITH_EDITOR
bool FOnDemandIoBackend::FlushDeferedEndPoints(double TimeOut)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::FlushDeferedEndPoints);

	FHttpManager& HttpManager = FHttpModule::Get().GetHttpManager();

	const double StartTime = FPlatformTime::Seconds();

	while (!DeferredEndpoints.IsEmpty())
	{
		HttpManager.Tick(0.0);
		FPlatformProcess::SleepNoStats(0.0f);

		if (TimeOut > 0.0 && (FPlatformTime::Seconds() - StartTime) > TimeOut)
		{
			return false;
		}
	}

	return true;
}

TArray<FIoChunkId> FOnDemandIoBackend::GetAllChunkIds()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::GetAllChunkIds);

	if (!IoStore.IsValid() || !bEnabled)
	{
		return TArray<FIoChunkId>();
	}

	const bool bAllowOptional = GIoDispatcherBulkOptionalEnabled && bEnableBulkOptional;

	return IoStore->GetAllChunkIds(bAllowOptional);
}

#endif // IS_PROGRAM || WITH_EDITOR

TIoStatusOr<FOnDemandToc> FOnDemandIoBackend::GetToc(FHttpClient& HttpClient, const FString& TocPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::GetToc);

	TAnsiStringBuilder<256> Url;
	Url << "/" << TocPath;

	for (int32 Attempt = 0, MaxAttempts = GIoDispatcherMaxHttpRetryCount; Attempt <= MaxAttempts; ++Attempt)
	{
		UE_LOG(LogIas, Log, TEXT("Fetching TOC '%s/%s' (#%d/%d)"), *HttpClient.ServiceUrl(), *TocPath, Attempt + 1, MaxAttempts);
		
		TIoStatusOr<FOnDemandToc> Toc;
		HttpClient.Get(Url.ToView(), [&Toc](TIoStatusOr<FIoBuffer> Response)
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

		while (HttpClient.Tick());

		if (Toc.IsOk())
		{
			return Toc;
		}
	}

	return TIoStatusOr<FOnDemandToc>(FIoStatus(EIoErrorCode::NotFound));
}

FIoStatus FOnDemandIoBackend::AddToc(const FOnDemandEndpoint& Endpoint)
{
	TUniquePtr<FHttpClient> Client = MakeUnique<FHttpClient>(Endpoint.ServiceUrl, GIoDispatcherMaxHttpConnectionCount);

	TIoStatusOr<FOnDemandToc> Toc = GetToc(*Client, Endpoint.TocPath);
	if (!Toc.IsOk())
	{
		return FIoStatus(Toc.Status());
	}

	{
		FWriteScopeLock _(Lock);
		if (!HttpClient.IsValid())
		{
			HttpClient = MoveTemp(Client);
			BackendThread.Reset(FRunnableThread::Create(this, TEXT("IoStoreOnDemand"), 0, TPri_AboveNormal));
		}
	}

	IoStore->AddToc(Endpoint, Toc.ConsumeValueOrDie());

	UE_CLOG(Endpoint.ServiceUrl !=  HttpClient->ServiceUrl(),
		LogIas, Fatal, TEXT("Fetching on demand content from multiple endpoints are currently not supported"));

	return FIoStatus::Ok;
}

uint32 FOnDemandIoBackend::Run()
{
	LLM_SCOPE(ELLMTag::FileSystem);

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
						[this, ChunkRequest, &NumConcurrentRequests](TIoStatusOr<FIoBuffer> Status)
						{
							NumConcurrentRequests--;

							if (Status.Status().GetErrorCode() == EIoErrorCode::ReadError)
							{
								if (++ChunkRequest->HttpRetryCount <= GIoDispatcherMaxHttpRetryCount)
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
								Stats.OnHttpGet(ChunkRequest->Chunk.DataSize());
							}
							else
							{
								TAnsiStringBuilder<256> Url;
								ChunkRequest->Params.GetUrl(Url);
								LogHttpResult(StringCast<TCHAR>(*Url).Get(), -1, 0, 0, ChunkRequest->Params.ChunkRange.GetOffset(), "HTTP FAILED");
								Stats.OnHttpError();
							}

							Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
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
						HttpClient->Tick();
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
				while (HttpClient->Tick() && !NextChunkRequest)
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

} // namespace UE::IO::Private

namespace UE
{

TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(TSharedPtr<IIoCache> Cache)
{
	return MakeShareable<IOnDemandIoDispatcherBackend>(new UE::IO::Private::FOnDemandIoBackend(Cache));
}

} // namespace UE
