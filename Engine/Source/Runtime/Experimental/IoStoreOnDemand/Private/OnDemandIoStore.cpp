// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandIoStore.h"
#include "OnDemandHttpClient.h"

#include "Algo/RemoveIf.h"
#include "Algo/Transform.h"
#include "Async/ManualResetEvent.h"
#include "Async/UniqueLock.h"
#include "Containers/StringConv.h"
#include "HAL/FileManager.h"
#include "HAL/FileManagerGeneric.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CoreDelegatesInternal.h"
#include "Misc/EncryptionKeyManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Serialization/MemoryReader.h"

namespace UE::IoStore
{

extern FString GIasOnDemandTocExt;

///////////////////////////////////////////////////////////////////////////////
namespace Private
{

static void SplitHostUrl(const FStringView& Url, FStringView& OutHost, FStringView& OutRemainder)
{
	OutHost = OutRemainder = FStringView();

	if (Url.StartsWith(TEXTVIEW("http")))
	{
		int32 Delim = INDEX_NONE;
		ensure(Url.FindChar(':', Delim));
		const int32 ProtocolDelim = Delim + 3;
		ensure(Url.RightChop(ProtocolDelim).FindChar('/', Delim));
		OutHost = Url.Left(ProtocolDelim + Delim);
	}

	OutRemainder = Url.RightChop(OutHost.Len());
}

/**
 * Utility to create a FArchive capable of reading from disk using the exact same pathing
 * rules as FPlatformMisc::LoadTextFileFromPlatformPackage but without forcing the entire
 * file to be loaded at once.
 */
static TUniquePtr<FArchive> CreateReaderFromPlatformPackage(const FString& RelPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::CreateReaderFromPlatformPackage);

	const FString AbsPath = FPaths::Combine(FGenericPlatformMisc::RootDir(), RelPath);

	IFileHandle* File = IPlatformFile::GetPlatformPhysical().OpenRead(*AbsPath);
	if (File)
	{
#if PLATFORM_ANDROID
		// This is a handle to an asset so we need to call Seek(0) to move the internal
		// offset to the start of the asset file.
		File->Seek(0);
#endif //PLATFORM_ANDROID
		const uint32 ReadBufferSize = 256 * 1024;
		return MakeUnique<FArchiveFileReaderGeneric>(File, *AbsPath, File->Size(), ReadBufferSize);
	}
	else
	{
		return TUniquePtr<FArchive>();
	}	
}

} // namespace UE::IoStore::Private

///////////////////////////////////////////////////////////////////////////////
const FOnDemandChunkEntry FOnDemandChunkEntry::Null = {};

///////////////////////////////////////////////////////////////////////////////
FOnDemandIoStore::FOnDemandIoStore()
{
	FEncryptionKeyManager::Get().OnKeyAdded().AddRaw(this, &FOnDemandIoStore::OnEncryptionKeyAdded);
}

FOnDemandIoStore::~FOnDemandIoStore()
{
	FEncryptionKeyManager::Get().OnKeyAdded().RemoveAll(this);

	if (OnMountPakHandle.IsValid())
	{
		FCoreInternalDelegates::GetOnPakMountOperation().Remove(OnMountPakHandle);
	}

	if (TickFuture.IsValid())
	{
		TickFuture.Wait();
	}
}

FIoStatus FOnDemandIoStore::Initialize()
{
	OnMountPakHandle = FCoreInternalDelegates::GetOnPakMountOperation().AddLambda(
		[this](EMountOperation Operation, const TCHAR* ContainerPath, int32 Order) -> void
		{
			IPlatformFile& Ipf = IPlatformFile::GetPlatformPhysical();
			const FString OnDemandTocPath = FPathViews::ChangeExtension(ContainerPath, GIasOnDemandTocExt);

			if (Ipf.FileExists(*OnDemandTocPath) == false)
			{
				return;
			}

			switch (Operation)
			{
			case EMountOperation::Mount:
			{
				UE::FManualResetEvent DoneEvent;
				Mount(FOnDemandMountArgs
				{
					.MountId = OnDemandTocPath,
					.FilePath = OnDemandTocPath
				},
				[&DoneEvent](TIoStatusOr<FOnDemandMountResult> Result)
				{
					UE_CLOG(!Result.IsOk(), LogIoStoreOnDemand, Error,
						TEXT("Failed to mount container, reason '%s'"), *Result.Status().ToString());
					DoneEvent.Notify();
				});
				DoneEvent.Wait();
				break;
			}
			case EMountOperation::Unmount:
			{
				const FIoStatus Status = Unmount(OnDemandTocPath);
				UE_CLOG(!Status.IsOk(), LogIoStoreOnDemand, Error,
					TEXT("Failed to unmount container, reason '%s'"), *Status.ToString());
				break;
			}
			default:
				checkNoEntry();
			}
		}
	);

	{
		IPlatformFile& Ipf = IPlatformFile::GetPlatformPhysical();
		FCurrentlyMountedPaksDelegate& Delegate = FCoreInternalDelegates::GetCurrentlyMountedPaksDelegate();
		if (Delegate.IsBound())
		{
			TArray<FMountedPakInfo> PakInfo = Delegate.Execute();
			for (const FMountedPakInfo& Info : PakInfo)
			{
				check(Info.PakFile != nullptr);
				const FString OnDemandTocPath = FPathViews::ChangeExtension(Info.PakFile->PakGetPakFilename(), GIasOnDemandTocExt);
				if (Ipf.FileExists(*OnDemandTocPath))
				{
					MountRequests.Add(OnDemandTocPath, MakeShared<FMountRequest>(FMountRequest
					{
						.MountArgs = FOnDemandMountArgs
						{
							.MountId = OnDemandTocPath,
							.FilePath = OnDemandTocPath
						},
						.OnCompleted = [](TIoStatusOr<FOnDemandMountResult> Result)
						{
							UE_CLOG(!Result.IsOk(), LogIoStoreOnDemand, Error, TEXT("Failed to mount container, reason '%s'"),
								*Result.Status().ToString());
						}
					}));
				}
			}
		}

		// Process mount requests synchronously at startup
		while (Tick(MAX_int64));
	}

	return EIoErrorCode::Ok;
}

void FOnDemandIoStore::Mount(FOnDemandMountArgs&& Args, FOnDemandMountCompleted&& OnCompleted)
{
	if (Args.MountId.IsEmpty())
	{
		return OnCompleted(FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid Mount ID")));
	}

	{
		UE::TUniqueLock Lock(MountRequestMutex);
		FSharedMountRequest& MountRequest = MountRequests.FindOrAdd(Args.MountId);

		if (MountRequest.IsValid())
		{
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Mount request '%s' is already mounting"), *Args.MountId);
			return OnCompleted(FIoStatus(EIoErrorCode::InvalidParameter));
		}

		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Enqueing mount request, MountId='%s'"), *Args.MountId);
		MountRequest = MakeShared<FMountRequest>(FMountRequest
		{ 
			.MountArgs		= MoveTemp(Args),
			.OnCompleted	= MoveTemp(OnCompleted),
		});
	}

	ConditionallyStartTicking();
}

void FOnDemandIoStore::ConditionallyStartTicking()
{
	check(FPlatformProcess::SupportsMultithreading());

	if (FPlatformProcess::SupportsMultithreading() && GIOThreadPool != nullptr)
	{
		bool bExpected = false;
		if (bTicking.compare_exchange_strong(bExpected, true))
		{
			TickFuture = AsyncPool(
				*GIOThreadPool,
				[this]
				{
					while (Tick(MAX_int64));
				},
				nullptr,
				EQueuedWorkPriority::Low);
		}
	}
	else
	{
		while (Tick(MAX_int64));
	}
}

FIoStatus FOnDemandIoStore::Unmount(FStringView MountId)
{
	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Unmounting '%s'"), *WriteToString<256>(MountId));

	{
		UE::TUniqueLock Lock(MountRequestMutex);
		if (FSharedMountRequest* MountRequest = MountRequests.Find(FString(MountId)))
		{
			(*MountRequest)->bCancelled = true;
		}
	}

	{
		TUniqueLock Lock(ContainerMutex);

		Containers.SetNum(Algo::RemoveIf(Containers, [&MountId](const FSharedOnDemandContainer& Container)
		{
			if (Container->MountId == MountId)
			{
				UE_LOG(LogIoStoreOnDemand, Log, TEXT("Unmounting container, ContainerName='%s', MountId='%s'"),
					*WriteToString<128>(Container->Name), *WriteToString<128>(Container->MountId));
				return true;
			}

			return false;
		}));
	}

	return EIoErrorCode::Ok;
}

FOnDemandChunkInfo FOnDemandIoStore::GetChunkInfo(const FIoChunkId& ChunkId)
{
	TUniqueLock Lock(ContainerMutex);

	for (FSharedOnDemandContainer& Container : Containers)
	{
		if (EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::Mounted))
		{
			if (const FOnDemandChunkEntry* Entry = Container->ChunkEntries.Find(ChunkId))
			{
				return FOnDemandChunkInfo(Container, *Entry);
			}
		}
	}

	return FOnDemandChunkInfo();
}

bool FOnDemandIoStore::Tick(int64 MaxCycles)
{
	check(!bTicking);
	bTicking = true;
	ON_SCOPE_EXIT{ bTicking = false; };

	TArray<FSharedMountRequest> Requests;
	{
		UE::TUniqueLock Lock(MountRequestMutex);
		for (TPair<FString, FSharedMountRequest>& Kv : MountRequests)
		{
			Requests.Add(Kv.Value);
		}
	}

	if (Requests.IsEmpty())
	{
		return false;
	}

	for (FSharedMountRequest& Request : Requests)
	{
		FIoStatus Status = ProcessMountRequest(*Request);

		{
			UE::TUniqueLock Lock(MountRequestMutex);
			MountRequests.Remove(Request->MountArgs.MountId);
			if (Request->bCancelled)
			{
				Status = EIoErrorCode::Cancelled;
			}
		}

		TIoStatusOr<FOnDemandMountResult> MountStatus;
		if (Status.IsOk())
		{
			UE::TUniqueLock Lock(ContainerMutex);
			for (const FSharedOnDemandContainer& Container : Request->Containers)
			{
				Containers.Add(Container);
				if (Container->EncryptionKeyGuid.IsEmpty() == false)
				{
					FGuid KeyGuid;
					ensure(FGuid::Parse(Container->EncryptionKeyGuid, KeyGuid));
					if (FEncryptionKeyManager::Get().TryGetKey(KeyGuid, Container->EncryptionKey) == false)
					{
						UE_LOG(LogIoStoreOnDemand, Log, TEXT("Deferring container '%s' until encryption key '%s' becomes available"),
							*Container->Name, *Container->EncryptionKeyGuid);
						EnumAddFlags(Container->Flags, EOnDemandContainerFlags::PendingEncryptionKey);
						continue;
					}
				}

				UE_LOG(LogIoStoreOnDemand, Log, TEXT("Mounting container '%s' (%d entries)"),
					*Container->Name, Container->ChunkEntries.Num());
				EnumAddFlags(Container->Flags, EOnDemandContainerFlags::Mounted);
			}

			MountStatus = TIoStatusOr<FOnDemandMountResult>(FOnDemandMountResult
			{
				.MountId = Request->MountArgs.MountId
			});
		}
		else
		{
			MountStatus = TIoStatusOr<FOnDemandMountResult>(Status);	
		}

		FOnDemandMountCompleted OnCompleted = MoveTemp(Request->OnCompleted);
		OnCompleted(MountStatus);
	}

	return true;
}

FIoStatus FOnDemandIoStore::ProcessMountRequest(FMountRequest& MountRequest)
{
	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Processing mount request, MountId='%s'"), *MountRequest.MountArgs.MountId);

	FOnDemandMountArgs& Args = MountRequest.MountArgs;
	FStringView Host, TocRelUrl;
	Private::SplitHostUrl(Args.Url, Host, TocRelUrl);
	const FStringView TocPath = FPathViews::GetPath(TocRelUrl);
	
	if (Args.Toc.IsSet())
	{
		CreateContainersFromToc(Args.MountId, TocPath, Args.Toc.GetValue(), MountRequest.Containers);
	}
	else if (Args.FilePath.IsEmpty() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Loading TOC from file '%s'"), *Args.FilePath);

		TUniquePtr<FArchive> Ar;
		if (FPlatformMisc::FileExistsInPlatformPackage(Args.FilePath))
		{
			Ar = Private::CreateReaderFromPlatformPackage(Args.FilePath);
		}
		else
		{
			Ar.Reset(IFileManager::Get().CreateFileReader(*Args.FilePath));
		}

		if (Ar.IsValid() == false)
		{
			return FIoStatusBuilder(EIoErrorCode::FileNotOpen)
				<< TEXT("Failed to open '")
				<< Args.FilePath
				<< TEXT("'");
		}

		// TODO: Add sentinel to all serialization paths for TOC files
		const bool bIncludeDot = true;	
		const bool bValidate = FPathViews::GetExtension(Args.FilePath, bIncludeDot) == GIasOnDemandTocExt;
		if (bValidate)
		{
			const int64 SentinelPos = Ar->TotalSize() - FOnDemandTocSentinel::SentinelSize;

			if (SentinelPos < 0)
			{
				UE_LOG(LogIoStoreOnDemand, Error, TEXT("The file '%s' is smaller than expected and quite possible corrupted"), *Args.FilePath);
				return EIoErrorCode::CorruptToc;
			}

			Ar->Seek(SentinelPos);

			FOnDemandTocSentinel Sentinel;
			(*Ar) << Sentinel;

			if (!Sentinel.IsValid())
			{
				UE_LOG(LogIas, Error, TEXT("File corruption detected when serializing '%s'"), *Args.FilePath);
				return EIoErrorCode::CorruptToc;
			}

			Ar->Seek(0);
		}

		FOnDemandToc Toc;
		*Ar << Toc;

		if (Ar->IsError() || Ar->IsCriticalError())
		{
			return FIoStatusBuilder(EIoErrorCode::FileNotOpen) << TEXT("Failed to serialize TOC file");
		}

		CreateContainersFromToc(Args.MountId, TocPath, Toc, MountRequest.Containers);
	}
	else if (Args.Url.IsEmpty() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Loading TOC from URL '%s'"), *Args.Url);

		if (Host.IsEmpty())
		{
			return FIoStatusBuilder(EIoErrorCode::InvalidParameter) << TEXT("No valid host URL");
		}

		FHttpClientConfig HttpCfg;
		HttpCfg.Endpoints.Add(FString(Host));
		HttpCfg.PrimaryEndpoint = 0; 
		HttpCfg.MaxRetryCount = 2;
		TUniquePtr<FHttpClient> Client = FHttpClient::Create(MoveTemp(HttpCfg));

		TAnsiStringBuilder<256> Url;
		if (!TocRelUrl.StartsWith(TCHAR('/')))
		{
			Url << "/";
		}
		Url << TocRelUrl;

		FIoBuffer Body;
		FIoStatus HttpStatus;
		Client->Get(Url.ToView(), [&Body, &HttpStatus](TIoStatusOr<FIoBuffer> Response, uint64)
			{
				if (Response.IsOk())
				{
					Body = Response.ConsumeValueOrDie();
				}
				else
				{
					HttpStatus = Response.Status();
				}
			});

		while (Client->Tick());

		if (HttpStatus.IsOk() == false)
		{
			return HttpStatus;
		}

		FOnDemandToc Toc;
		FMemoryReaderView Ar(Body.GetView());
		Ar << Toc;

		if (Ar.IsError() || Ar.IsCriticalError())
		{
			return FIoStatusBuilder(EIoErrorCode::FileNotOpen) << TEXT("Failed to serialize TOC from HTTP response");
		}

		CreateContainersFromToc(Args.MountId, TocPath, Toc, MountRequest.Containers);
	}
	else
	{
		return FIoStatusBuilder(EIoErrorCode::InvalidParameter) << TEXT("Invalid mount arguments");
	}

	return EIoErrorCode::Ok;
}

void FOnDemandIoStore::OnEncryptionKeyAdded(const FGuid& Id, const FAES::FAESKey& Key)
{
	TUniqueLock Lock(ContainerMutex);

	for (FSharedOnDemandContainer& Container : Containers)
	{
		if (EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::PendingEncryptionKey))
		{
			FGuid KeyGuid;
			ensure(FGuid::Parse(FString(StringCast<TCHAR>(*Container->EncryptionKeyGuid)), KeyGuid));

			if (FEncryptionKeyManager::Get().TryGetKey(KeyGuid, Container->EncryptionKey))
			{
				UE_LOG(LogIoStoreOnDemand, Log, TEXT("Mounting container '%s' (%d entries)"),
					*WriteToString<128>(Container->Name), Container->ChunkEntries.Num());

				EnumRemoveFlags(Container->Flags, EOnDemandContainerFlags::PendingEncryptionKey);
				EnumAddFlags(Container->Flags, EOnDemandContainerFlags::Mounted);
			}
		}
	}
}

void FOnDemandIoStore::CreateContainersFromToc(
	FStringView MountId,
	FStringView TocPath,
	FOnDemandToc& Toc,
	TArray<FSharedOnDemandContainer>& Out)
{
	const FOnDemandTocHeader& Header = Toc.Header;
	const FName CompressionFormat(Header.CompressionFormat);

	TStringBuilder<128> ChunksDirectory;

	{
		if (TocPath.IsEmpty() == false)
		{
			Algo::Transform(TocPath, AppendChars(ChunksDirectory), FChar::ToLower);
		}
		else
		{
			Algo::Transform(Toc.Header.ChunksDirectory, AppendChars(ChunksDirectory), FChar::ToLower);
		}

		FPathViews::Append(ChunksDirectory, TEXT("chunks"));
	}

	for (FOnDemandTocContainerEntry& ContainerEntry : Toc.Containers)
	{
		FSharedOnDemandContainer Container = MakeShared<FOnDemandContainer>();
		Container->Name					= MoveTemp(ContainerEntry.ContainerName);
		Container->MountId				= MountId;
		Container->ChunksDirectory		= StringCast<ANSICHAR>(ChunksDirectory.ToString());
		Container->EncryptionKeyGuid	= MoveTemp(ContainerEntry.EncryptionKeyGuid);
		Container->BlockSize			= Header.BlockSize;
		Container->BlockSizes			= MoveTemp(ContainerEntry.BlockSizes);
		Container->BlockHashes			= MoveTemp(ContainerEntry.BlockHashes);
		Container->ContainerId			= ContainerEntry.ContainerId;
		Container->CompressionFormats.Add(CompressionFormat);

		Container->ChunkEntries.Reserve(ContainerEntry.Entries.Num());
		for (const FOnDemandTocEntry& TocEntry : ContainerEntry.Entries)
		{
			check(TocEntry.RawSize <= 0xffff'ffffull);
			check(TocEntry.EncodedSize <= 0xffff'ffffull);
			Container->ChunkEntries.Add(TocEntry.ChunkId, FOnDemandChunkEntry
			{
				.Hash					= TocEntry.Hash,
				.RawSize				= uint32(TocEntry.RawSize),
				.EncodedSize			= uint32(TocEntry.EncodedSize),
				.BlockOffset			= TocEntry.BlockOffset,
				.BlockCount				= TocEntry.BlockCount,
				.CompressionFormatIndex	= 0
			});
		}

		Out.Add(MoveTemp(Container));
	}
}

} // namespace UE::IoStore
