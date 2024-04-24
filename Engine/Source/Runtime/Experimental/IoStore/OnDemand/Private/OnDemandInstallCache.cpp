// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandInstallCache.h"
#include "OnDemandHttpClient.h"
#include "OnDemandIoStore.h"

#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "IO/IoContainerHeader.h"
#include "IO/IoChunkEncoding.h"
#include "Misc/PathViews.h"
#include "Serialization/MemoryReader.h"

namespace UE::IoStore
{

///////////////////////////////////////////////////////////////////////////////
class FOnDemandInstallCache final
	: public IOnDemandInstallCache 
{
	using FSharedBackendContextRef	= TSharedRef<const FIoDispatcherBackendContext>;
	using FSharedBackendContext		= TSharedPtr<const FIoDispatcherBackendContext>;

	struct FChunkRequest
	{
		explicit FChunkRequest(
			FIoRequestImpl* Request,
			FOnDemandChunkInfo&& Info,
			FIoOffsetAndLength Range,
			uint64 RequestedRawSize)
				: DispatcherRequest(Request)
				, ChunkInfo(MoveTemp(Info))
				, ChunkRange(Range)
				, EncodedChunk(ChunkRange.GetLength())
				, RawSize(RequestedRawSize)
		{
			check(DispatcherRequest != nullptr);
			check(ChunkInfo.IsValid());
			check(Request->NextRequest == nullptr);
			check(Request->BackendData == nullptr);

			DispatcherRequest->BackendData = this;
		}

		FIoRequestImpl*		DispatcherRequest;
		FOnDemandChunkInfo	ChunkInfo;
		FIoOffsetAndLength 	ChunkRange;
		FIoBuffer			EncodedChunk;
		uint64				RawSize;
	};

public:
	FOnDemandInstallCache(const FOnDemandInstallCacheConfig& Config, FOnDemandIoStore& IoStore);
	virtual ~FOnDemandInstallCache();

	// IIoDispatcherBackend
	virtual void				Initialize(FSharedBackendContextRef Context) override;
	virtual void				Shutdown() override;
	virtual void				ResolveIoRequests(FIoRequestList Requests, FIoRequestList& OutUnresolved) override;
	virtual FIoRequestImpl*		GetCompletedIoRequests() override;
	virtual void				CancelIoRequest(FIoRequestImpl* Request) override;
	virtual void				UpdatePriorityForIoRequest(FIoRequestImpl* Request) override;
	virtual bool				DoesChunkExist(const FIoChunkId& ChunkId) const override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId) const override;
	virtual TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) override;

	// IOnDemandInstallCache
	virtual FIoStatus Put(const FIoChunkId& ChunkId, FIoBuffer&& Chunk, const FIoHash& Hash) override;

private:
	bool						Resolve(FIoRequestImpl* Request);
	void						CompleteRequest(TUniquePtr<FChunkRequest>&& ChunkRequest);
	FIoStatus					WriteChunk(FIoBuffer Chunk, const FIoHash& ExpectedHash);
	void						GetChunkFilename(const FIoHash& Hash, FStringBuilderBase& OutPath);

	FOnDemandIoStore&		IoStore;
	FSharedBackendContext	BackendContext;
	FIoRequestList			CompletedRequests;
	UE::FMutex				Mutex;
	FString					CacheDirectory;
};

///////////////////////////////////////////////////////////////////////////////
FOnDemandInstallCache::FOnDemandInstallCache(const FOnDemandInstallCacheConfig& Config, FOnDemandIoStore& InIoStore)
	: IoStore(InIoStore)
	, CacheDirectory(Config.RootDirectory)
{
}

FOnDemandInstallCache::~FOnDemandInstallCache()
{
}

void FOnDemandInstallCache::Initialize(FSharedBackendContextRef Context)
{
	BackendContext = Context;
}

void FOnDemandInstallCache::Shutdown()
{
}

void FOnDemandInstallCache::ResolveIoRequests(FIoRequestList Requests, FIoRequestList& OutUnresolved)
{
	while (FIoRequestImpl* Request = Requests.PopHead())
	{
		if (Resolve(Request) == false)
		{
			OutUnresolved.AddTail(Request);
		}
	}
}

FIoRequestImpl* FOnDemandInstallCache::GetCompletedIoRequests()
{
	FIoRequestImpl* FirstCompleted = nullptr;
	{
		UE::TUniqueLock Lock(Mutex);
		FirstCompleted = CompletedRequests.GetHead();
		CompletedRequests = FIoRequestList();
	}

	return FirstCompleted;
}

void FOnDemandInstallCache::CancelIoRequest(FIoRequestImpl* Request)
{
}

void FOnDemandInstallCache::UpdatePriorityForIoRequest(FIoRequestImpl* Request)
{
}

bool FOnDemandInstallCache::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	const TIoStatusOr<uint64> Status = GetSizeForChunk(ChunkId);
	return Status.IsOk();
}

TIoStatusOr<uint64> FOnDemandInstallCache::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	if (FOnDemandChunkInfo ChunkInfo = IoStore.GetInstalledChunkInfo(ChunkId))
	{
		return ChunkInfo.RawSize();
	}

	return FIoStatus(EIoErrorCode::UnknownChunkID);
}

TIoStatusOr<FIoMappedRegion> FOnDemandInstallCache::OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options)
{
	return FIoStatus(EIoErrorCode::FileOpenFailed);
}

bool FOnDemandInstallCache::Resolve(FIoRequestImpl* Request)
{
	FOnDemandChunkInfo ChunkInfo = IoStore.GetInstalledChunkInfo(Request->ChunkId);
	if (ChunkInfo.IsValid() == false)
	{
		return false;
	}

	const uint64 RequestSize = FMath::Min<uint64>(
		Request->Options.GetSize(),
		ChunkInfo.RawSize() - Request->Options.GetOffset());

	TIoStatusOr<FIoOffsetAndLength> ChunkRange = FIoChunkEncoding::GetChunkRange(
		ChunkInfo.RawSize(),
		ChunkInfo.BlockSize(),
		ChunkInfo.Blocks(),
		Request->Options.GetOffset(),
		RequestSize);

	if (ChunkRange.IsOk() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to get chunk range"));
		return false;
	}

	TStringBuilder<256> Filename;
	GetChunkFilename(ChunkInfo.Hash(), Filename);

	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	TUniquePtr<IFileHandle> FileHandle(Ipf.OpenRead(Filename.ToString()));
	if (FileHandle.IsValid() == false)
	{
		return false;
	}

	const FIoOffsetAndLength Range = ChunkRange.ConsumeValueOrDie();
	if (!FileHandle->Seek(IntCastChecked<int64>(Range.GetOffset())))
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Seek file failed"));
		return false;
	}

	TUniquePtr<FChunkRequest> ChunkRequest = MakeUnique<FChunkRequest>(
		Request,
		MoveTemp(ChunkInfo),
		Range,
		RequestSize);

	FIoBuffer& EncodedChunk = ChunkRequest->EncodedChunk;
	if (!FileHandle->Read(reinterpret_cast<uint8*>(EncodedChunk.GetData()), IntCastChecked<int64>(EncodedChunk.GetSize())))
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to read '%s'"), Filename.ToString());
		return false;
	}

	UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, ChunkRequest = MoveTemp(ChunkRequest)]() mutable
	{
		CompleteRequest(MoveTemp(ChunkRequest));
	});

	return true;
}

FIoStatus FOnDemandInstallCache::Put(const FIoChunkId& ChunkId, FIoBuffer&& Chunk, const FIoHash& Hash)
{
	return WriteChunk(Chunk, Hash);
}

FIoStatus FOnDemandInstallCache::WriteChunk(FIoBuffer Chunk, const FIoHash& ExpectedHash)
{
	const FIoHash ChunkHash = FIoHash::HashBuffer(Chunk.GetView());

	if (ChunkHash != ExpectedHash)
	{
		return FIoStatus(EIoErrorCode::ReadError, TEXTVIEW("Hash mismatch"));
	}

	TStringBuilder<256> Filename;
	GetChunkFilename(ChunkHash, Filename);
	const FString Directory = FString(FPathViews::GetPath(Filename));

	const bool bTree = true;
	if (IFileManager& Ifm = IFileManager::Get(); !Ifm.MakeDirectory(*Directory, bTree))
	{
		return FIoStatusBuilder(EIoErrorCode::WriteError)
			<< TEXT("Failed to create directory '")
			<< Directory
			<< TEXT("'");
	}

	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	TUniquePtr<IFileHandle> FileHandle(Ipf.OpenWrite(Filename.ToString()));

	if (FileHandle.IsValid() == false)
	{
		return EIoErrorCode::FileOpenFailed;
	}

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Writing '%s' %.2lf KiB"), Filename.ToString(), double(Chunk.GetSize()) / 1024.0);
	if (!FileHandle->Write(reinterpret_cast<const uint8*>(Chunk.GetData()), Chunk.GetSize()))
	{
		return FIoStatusBuilder(EIoErrorCode::WriteError)
			<< TEXT("Failed to write file '")
			<< Filename.ToString()
			<< TEXT("'");
	}

	return EIoErrorCode::Ok;
}

void FOnDemandInstallCache::GetChunkFilename(const FIoHash& Hash, FStringBuilderBase& OutPath)
{
	const FString HashString = LexToString(Hash);

	FPathViews::Append(OutPath, CacheDirectory);
	FPathViews::Append(OutPath, TEXTVIEW("chunks"));
	FPathViews::Append(OutPath, HashString.Left(2));
	FPathViews::Append(OutPath, HashString);
	OutPath << TEXT(".iochunk");
}

void FOnDemandInstallCache::CompleteRequest(TUniquePtr<FChunkRequest>&& ChunkRequest)
{
	FIoRequestImpl* Request				= ChunkRequest->DispatcherRequest;
	const FOnDemandChunkInfo& ChunkInfo = ChunkRequest->ChunkInfo;

	FIoChunkDecodingParams Params;
	Params.CompressionFormat	= ChunkInfo.CompressionFormat();
	Params.EncryptionKey		= ChunkInfo.EncryptionKey();
	Params.BlockSize			= ChunkInfo.BlockSize();
	Params.TotalRawSize			= ChunkInfo.RawSize();
	Params.RawOffset			= Request->Options.GetOffset();
	Params.EncodedOffset		= ChunkRequest->ChunkRange.GetOffset();
	Params.EncodedBlockSize		= ChunkInfo.Blocks();
	Params.BlockHash			= ChunkInfo.BlockHashes();

	Request->CreateBuffer(ChunkRequest->RawSize);

	FMutableMemoryView RawChunk = Request->GetBuffer().GetMutableView();
	FMemoryView EncodedChunk	= ChunkRequest->EncodedChunk.GetView();

	if (FIoChunkEncoding::Decode(Params, EncodedChunk, RawChunk) == false)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to read chunk, ChunkId='%s'"), *LexToString(Request->ChunkId));
		Request->SetResult(FIoBuffer());
		Request->SetFailed();
	}

	{
		UE::TUniqueLock Lock(Mutex);
		Request->BackendData = nullptr;
		CompletedRequests.AddTail(Request);
	}

	BackendContext->WakeUpDispatcherThreadDelegate.Execute();
}

///////////////////////////////////////////////////////////////////////////////
TSharedPtr<IOnDemandInstallCache> MakeOnDemandInstallCache(
	FOnDemandIoStore& IoStore,
	const FOnDemandInstallCacheConfig& Config)
{
	IFileManager& Ifm = IFileManager::Get();
	if (Config.bDropCache)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Deleting install cache directory '%s'"), *Config.RootDirectory);
		Ifm.DeleteDirectory(*Config.RootDirectory, false, true);
	}

	const bool bTree = true;
	if (!Ifm.MakeDirectory(*Config.RootDirectory, bTree))
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to create directory '%s'"), *Config.RootDirectory);
		return TSharedPtr<IOnDemandInstallCache>();
	}

	return MakeShareable<IOnDemandInstallCache>(new FOnDemandInstallCache(Config, IoStore));
}

} // namespace UE::IoStore
