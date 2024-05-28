// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandIoStore.h"
#include "OnDemandHttpClient.h"
#include "OnDemandInstallCache.h"
#include "OnDemandPackageStoreBackend.h"

#include "Algo/Copy.h"
#include "Algo/Find.h"
#include "Algo/RemoveIf.h"
#include "Algo/Transform.h"
#include "Async/ManualResetEvent.h"
#include "Async/UniqueLock.h"
#include "Containers/RingBuffer.h"
#include "Containers/StringConv.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "IO/IoContainerHeader.h"
#include "IO/PackageStore.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegatesInternal.h"
#include "Misc/EncryptionKeyManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Serialization/MemoryReader.h"

///////////////////////////////////////////////////////////////////////////////
bool GIoStoreOnDemandInstallCacheEnabled = true;
static FAutoConsoleVariableRef CVar_IoStoreOnDemandInstallCacheEnabled (
	TEXT("iostore.OnDemandInstallCacheEnabled"),
	GIoStoreOnDemandInstallCacheEnabled,
	TEXT("Whether the on-demand install cache is enabled."),
	ECVF_ReadOnly
);

namespace UE::IoStore
{

extern FString GIasOnDemandTocExt;

///////////////////////////////////////////////////////////////////////////////
namespace Private
{

///////////////////////////////////////////////////////////////////////////////
static FString GetInstallCacheDirectory()
{
	return FPaths::ProjectPersistentDownloadDir() / TEXT("IoStore") / TEXT("InstallCache");
}

///////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////
using FPackageStoreEntryMap = TMap<FPackageId, const FFilePackageStoreEntry*>;

struct FContainerInstallData
{
	FPackageStoreEntryMap	PackageStoreEntries;
	TSet<FPackageId>		PackageIds;
	TSet<FIoChunkId>		ResolvedChunks;
	uint64					TotalSize = 0;
};

using FInstallData = TMap<FSharedOnDemandContainer, FContainerInstallData>;

FIoStatus BuildInstallData(
	const TSet<FSharedOnDemandContainer>& Containers,
	const TSet<FPackageId>& PackageIds,
	FInstallData& OutInstallData,
	const TSet<FPackageId>& OutMissing)
{
	OutInstallData.Reserve(Containers.Num());

	// Setup the package information for each container
	for (const FSharedOnDemandContainer& Container : Containers)
	{
		if (!Container->Header.IsValid() || Container->Header->PackageIds.IsEmpty())
		{
			// The container contains no package data
			continue;
		}

		FContainerInstallData& Data = OutInstallData.FindOrAdd(Container);
		const FIoContainerHeader& Header = *Container->Header;
		TConstArrayView<FFilePackageStoreEntry> Entries(
			reinterpret_cast<const FFilePackageStoreEntry*>(Header.StoreEntries.GetData()),
			Header.PackageIds.Num());
		
		Data.PackageStoreEntries.Reserve(Header.PackageIds.Num());

		int32 Idx = 0;
		for (const FFilePackageStoreEntry& Entry : Entries)
		{
			const FPackageId PackageId = Header.PackageIds[Idx++];
			Data.PackageStoreEntries.Add(PackageId, &Entry);
		}
	}

	// Traverse dependencies for each package id
	using FQueue = TRingBuffer<FPackageId>;

	FQueue Queue;
	TSet<FPackageId> Visitied;
	TSet<FPackageId> Missing;

	Visitied.Reserve(PackageIds.Num());
	Queue.Reserve(PackageIds.Num());

	//Algo::Copy(PackageIds, Queue);
	for (const FPackageId& PackageId : PackageIds)
	{
		Queue.Add(PackageId);
	}

	while (!Queue.IsEmpty())
	{
		FPackageId PackageId = Queue.PopFrontValue();

		bool bIsAlreadyInSet = false;
		Visitied.Add(PackageId, &bIsAlreadyInSet);
		if (bIsAlreadyInSet)
		{
			continue;
		}

		bool bFound = false;
		for (TPair<FSharedOnDemandContainer, FContainerInstallData>& Kv : OutInstallData)
		{
			FContainerInstallData& Data = Kv.Value;
			if (const FFilePackageStoreEntry** Entry = Data.PackageStoreEntries.Find(PackageId))
			{
				Data.PackageIds.Add(PackageId);

				// Add all imported packages not already processed
				const FFilePackageStoreEntry& PackageStoreEntry = **Entry;
				for (const FPackageId& ImportedPackageId : PackageStoreEntry.ImportedPackages)
				{
					if (!Visitied.Contains(ImportedPackageId))
					{
						Queue.Add(ImportedPackageId);
					}
				}

				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			Missing.Add(PackageId);
		}
	}

	for (TPair<FSharedOnDemandContainer, FContainerInstallData>& Kv : OutInstallData)
	{
		const FOnDemandContainer& Container = *Kv.Key;
		FContainerInstallData& Data			= Kv.Value;

		if (Data.PackageIds.IsEmpty())
		{
			continue;
		}

		for (const FPackageId& PackageId : Data.PackageIds)
		{
			const FIoChunkId PackageChunkId					= CreatePackageDataChunkId(PackageId);
			const FOnDemandChunkEntry* PackageChunkEntry	= Container.ChunkEntries.Find(PackageChunkId);

			if (PackageChunkEntry == nullptr)
			{
				UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Missing package data"));
				continue;
			}

			Data.ResolvedChunks.Add(PackageChunkId);
			Data.TotalSize += PackageChunkEntry->EncodedSize;

			const EIoChunkType AdditionalPackageChunkTypes[] =
			{
				EIoChunkType::BulkData,
				EIoChunkType::OptionalBulkData,
				EIoChunkType::MemoryMappedBulkData 
			};

			for (EIoChunkType ChunkType : AdditionalPackageChunkTypes)
			{
				const FIoChunkId ChunkId = CreateIoChunkId(PackageId.Value(), 0, ChunkType);
				if (const FOnDemandChunkEntry* ChunkEntry = Container.ChunkEntries.Find(ChunkId); ChunkEntry != nullptr)
				{
					Data.ResolvedChunks.Add(ChunkId);
					Data.TotalSize += ChunkEntry->EncodedSize;
				}
			}
		}

		// For now we always download these chunks
		for (const TPair<FIoChunkId, FOnDemandChunkEntry>& IdEntry : Container.ChunkEntries)
		{
			const FIoChunkId& ChunkId				= IdEntry.Key;
			const FOnDemandChunkEntry& ChunkEntry	= IdEntry.Value;

			switch(ChunkId.GetChunkType())
			{
				case EIoChunkType::ExternalFile:
				case EIoChunkType::ShaderCodeLibrary:
				case EIoChunkType::ShaderCode:
				{
					Data.ResolvedChunks.Add(ChunkId);
					Data.TotalSize += ChunkEntry.EncodedSize;
				}
				default:
					break;
			}
		}
	}

	return EIoErrorCode::Ok;
}

} // namespace UE::IoStore::Private

///////////////////////////////////////////////////////////////////////////////
void LexToString(EOnDemandContainerFlags Flags, FStringBuilderBase& Out)
{
	static const TCHAR* Names[]
	{
		TEXT("None"),
		TEXT("PendingEncryptionKey"),
		TEXT("Mounted"),
		TEXT("Streaming"),
		TEXT("Installed")
	};

	if (Flags == EOnDemandContainerFlags::None)
	{
		Out << TEXT("None");
		return;
	}

	for (int32 Idx = 0, Count = int32(EOnDemandContainerFlags::Count); Idx < Count; ++Idx)
	{
		const EOnDemandContainerFlags FlagToTest = static_cast<EOnDemandContainerFlags>(1 << Idx);
		if (EnumHasAnyFlags(Flags, FlagToTest))
		{
			if (Out.Len())
			{
				Out << TEXT("|");
			}
			Out << Names[Idx + 1];
		}
	}
}

FString LexToString(EOnDemandContainerFlags Flags)
{
	TStringBuilder<128> Sb;
	LexToString(Flags, Sb);
	return FString(Sb.ToString(), Sb.Len());
}

///////////////////////////////////////////////////////////////////////////////
const FOnDemandChunkEntry FOnDemandChunkEntry::Null = {};

///////////////////////////////////////////////////////////////////////////////
static FString OnDemandContainerUniqueName(FStringView MountId, FStringView Name)
{
	return FString::Printf(TEXT("%.*s-%.*s"), MountId.Len(), MountId.GetData(), Name.Len(), Name.GetData());
}

FString FOnDemandContainer::UniqueName() const
{
	return OnDemandContainerUniqueName(MountId, Name);
}

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
	bool bUseInstallCache = GIoStoreOnDemandInstallCacheEnabled;
#if !UE_BUILD_SHIPPING
	bUseInstallCache = FParse::Param(FCommandLine::Get(), TEXT("NoIAD")) == false;
#endif
	if (bUseInstallCache)
	{
		FOnDemandInstallCacheConfig CacheConfig;
		CacheConfig.RootDirectory = Private::GetInstallCacheDirectory(); 
#if !UE_BUILD_SHIPPING
		CacheConfig.bDropCache = FParse::Param(FCommandLine::Get(), TEXT("Iad.DropCache"));
#endif
		InstallCache = MakeOnDemandInstallCache(*this, CacheConfig);
		if (InstallCache.IsValid())
		{
			int32 BackendPriority = -5; // Lower than file (zero) but higher than streaming backend (-10)
#if !UE_BUILD_SHIPPING
			if (FParse::Param(FCommandLine::Get(), TEXT("Iad")))
			{
				// Bump the priority to be higher then the file system backend
				BackendPriority = 5;
			}
#endif
			FIoDispatcher::Get().Mount(InstallCache.ToSharedRef(), BackendPriority);
			PackageStoreBackend = MakeOnDemandPackageStoreBackend();
			FPackageStore::Get().Mount(PackageStoreBackend.ToSharedRef());
		}
		else
		{
			// Only warn until this is properly tested
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Failed to initialize install cache"));
		}
	}

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
		FCurrentlyMountedPaksDelegate& Delegate = FCoreInternalDelegates::GetCurrentlyMountedPaksDelegate();
		if (Delegate.IsBound())
		{
			IPlatformFile& Ipf = IPlatformFile::GetPlatformPhysical();
			TArray<FMountedPakInfo> PakInfo = Delegate.Execute();
			for (const FMountedPakInfo& Info : PakInfo)
			{
				check(Info.PakFile != nullptr);
				const FString OnDemandTocPath = FPathViews::ChangeExtension(Info.PakFile->PakGetPakFilename(), GIasOnDemandTocExt);
				if (Ipf.FileExists(*OnDemandTocPath))
				{
					FSharedMountRequest MountRequest = MakeShared<FMountRequest>();
					MountRequest->MountArgs = FOnDemandMountArgs
					{
						.MountId	= OnDemandTocPath,
						.FilePath	= OnDemandTocPath
					};
					MountRequest->OnCompleted = [](TIoStatusOr<FOnDemandMountResult> Result)
					{
						UE_CLOG(!Result.IsOk(), LogIoStoreOnDemand, Error, TEXT("Failed to mount container, reason '%s'"),
							*Result.Status().ToString());
					};
					MountRequests.Add(OnDemandTocPath, MoveTemp(MountRequest));
				}
			}

			// Process mount requests synchronously at startup
			TickLoop();
		}
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
		
		MountRequest = MakeShared<FMountRequest>();
		MountRequest->MountArgs		= MoveTemp(Args);
		MountRequest->OnCompleted	= MoveTemp(OnCompleted);
	}

	TryEnterTickLoop();
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

		Containers.SetNum(Algo::RemoveIf(Containers, [this, &MountId](const FSharedOnDemandContainer& Container)
		{
			if (Container->MountId == MountId)
			{
				UE_LOG(LogIoStoreOnDemand, Log, TEXT("Unmounting container, ContainerName='%s', MountId='%s'"),
					*WriteToString<128>(Container->Name), *WriteToString<128>(Container->MountId));

				if (PackageStoreBackend.IsValid())
				{
					PackageStoreBackend->Unmount(Container->UniqueName());
				}

				return true;
			}

			return false;
		}));
	}

	return EIoErrorCode::Ok;
}

FOnDemandChunkInfo FOnDemandIoStore::GetStreamingChunkInfo(const FIoChunkId& ChunkId)
{
	return GetChunkInfo(ChunkId, EOnDemandContainerFlags::Mounted | EOnDemandContainerFlags::Streaming);
}

FOnDemandChunkInfo FOnDemandIoStore::GetInstalledChunkInfo(const FIoChunkId& ChunkId)
{
	return GetChunkInfo(ChunkId, EOnDemandContainerFlags::Mounted | EOnDemandContainerFlags::Installed);
}

FOnDemandChunkInfo FOnDemandIoStore::GetChunkInfo(const FIoChunkId& ChunkId, EOnDemandContainerFlags ContainerFlags)
{
	TUniqueLock Lock(ContainerMutex);

	for (FSharedOnDemandContainer& Container : Containers)
	{
		if (EnumHasAllFlags(Container->Flags, ContainerFlags))
		{
			if (const FOnDemandChunkEntry* Entry = Container->ChunkEntries.Find(ChunkId))
			{
				return FOnDemandChunkInfo(Container, *Entry);
			}
		}
	}

	return FOnDemandChunkInfo();
}

void FOnDemandIoStore::TryEnterTickLoop()
{
	bool bEnterTickLoop = false;
	{
		UE::TUniqueLock Lock(MountRequestMutex);
		bTickRequested = true;
		if (bTicking == false)
		{
			bTicking = bEnterTickLoop = true;
		}
	}

	if (bEnterTickLoop == false)
	{
		UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("I/O store already ticking"));
		return;
	}

	if (FPlatformProcess::SupportsMultithreading() && GIOThreadPool != nullptr)
	{
		TickFuture = AsyncPool(*GIOThreadPool, [this] { TickLoop(); }, nullptr, EQueuedWorkPriority::Low);
	}
	else
	{
		TickLoop();
	}
}

void FOnDemandIoStore::TickLoop()
{
	ON_SCOPE_EXIT { UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Exiting I/O store tick loop")); };

	UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Entering I/O store tick loop"));
	for (;;)
	{
		const bool bTicked = Tick();
		if (bTicked == false)
		{
			UE::TUniqueLock Lock(MountRequestMutex);
			if (bTickRequested == false)
			{
				bTicking = false;
				break;
			}
			bTickRequested = false;
		}
	}
}

bool FOnDemandIoStore::Tick()
{
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
		// Tick mount
		if (FIoStatus Status = TickMountRequest(*Request); !Status.IsOk())
		{
			{
				UE::TUniqueLock Lock(MountRequestMutex);
				MountRequests.Remove(Request->MountArgs.MountId);
			}
			FOnDemandMountCompleted OnCompleted = MoveTemp(Request->OnCompleted);
			OnCompleted(Status);
			continue;
		}

		// Tick install
		if (EnumHasAnyFlags(Request->MountArgs.Options, EOnDemandMountOptions::Install))
		{
			if (FIoStatus Status = TickInstallRequest(*Request); !Status.IsOk())
			{
				{
					UE::TUniqueLock Lock(MountRequestMutex);
					MountRequests.Remove(Request->MountArgs.MountId);
				}
				FOnDemandMountCompleted OnCompleted = MoveTemp(Request->OnCompleted);
				OnCompleted(Status);
				continue;;
			}
		}

		{
			{
				UE::TUniqueLock Lock(MountRequestMutex);
				MountRequests.Remove(Request->MountArgs.MountId);
			}

			TStringBuilder<128> Sb;
			UE::TUniqueLock Lock(ContainerMutex);
			for (const FSharedOnDemandContainer& Container : Request->Containers)
			{
				if (EnumHasAnyFlags(Request->MountArgs.Options, EOnDemandMountOptions::StreamOnDemand))
				{
					EnumAddFlags(Container->Flags, EOnDemandContainerFlags::Streaming);
				}

				if (EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::Mounted))
				{
					continue;
				}

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

				Sb.Reset();
				LexToString(Container->Flags, Sb);
				UE_LOG(LogIoStoreOnDemand, Log, TEXT("Mounting container '%s', Entries=%d, Flags='%s'"),
					*Container->Name, Container->ChunkEntries.Num(), Sb.ToString());

				EnumAddFlags(Container->Flags, EOnDemandContainerFlags::Mounted);
			}

			FOnDemandMountCompleted OnCompleted = MoveTemp(Request->OnCompleted);
			OnCompleted(FOnDemandMountResult
			{
				.MountId = Request->MountArgs.MountId
			});
		}
	}

	return true;
}

FIoStatus FOnDemandIoStore::TickMountRequest(FMountRequest& MountRequest)
{
	UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Ticking mount request, MountId='%s'"), *MountRequest.MountArgs.MountId);

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

		// TODO: Enable validation when the sentinal is included in all serialization paths
		const bool bValidate = false;
		TIoStatusOr<FOnDemandToc> TocStatus = FOnDemandToc::LoadFromFile(Args.FilePath, bValidate);
		if (!TocStatus.IsOk())
		{
			return TocStatus.Status();
		}

		FOnDemandToc Toc = TocStatus.ConsumeValueOrDie();
		Args.Toc.Emplace(MoveTemp(Toc));

		CreateContainersFromToc(Args.MountId, TocPath, *Args.Toc, MountRequest.Containers);
	}
	else if (Args.Url.IsEmpty() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Loading TOC from URL '%s'"), *Args.Url);

		const uint32 RetryCount				= 2;
		const bool bFollowRedirects			= true;
		TIoStatusOr<FOnDemandToc> TocStatus	= FOnDemandToc::LoadFromUrl(Args.Url, 2, bFollowRedirects);

		if (!TocStatus.IsOk())
		{
			return TocStatus.Status();
		}

		FOnDemandToc Toc = TocStatus.ConsumeValueOrDie();
		Args.Toc.Emplace(MoveTemp(Toc));

		CreateContainersFromToc(Args.MountId, TocPath, *Args.Toc, MountRequest.Containers);
	}

	// Remove already mounted containers
	// TODO: Treat this as an error?
	{
		MountRequest.Containers.SetNum(
			Algo::RemoveIf(
				MountRequest.Containers,
				[MountedContainers = GetMountedContainers()](const FSharedOnDemandContainer& Container)
				{
					const FSharedOnDemandContainer* Existing =
						Algo::FindBy(
							MountedContainers,
							Container->UniqueName(),
							[](const FSharedOnDemandContainer& C) { return C->UniqueName(); });

					if (Existing != nullptr)
					{
						UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Container already mounted, ContainerName='%s', MountId='%s'"),
							*WriteToString<128>(Container->Name), *WriteToString<128>(Container->MountId));
						return true;
					}
					return false;
				}));
	}

	// Find containers matching the mount ID if this is a request to install/download already mounted containers 
	if (MountRequest.Containers.IsEmpty())
	{
		UE::TUniqueLock Lock(ContainerMutex);
		for (FSharedOnDemandContainer& Container : Containers)
		{
			if (Container->MountId == Args.MountId)
			{
				MountRequest.Containers.Add(Container);
			}
		}
	}

	return EIoErrorCode::Ok;
}

FIoStatus FOnDemandIoStore::TickInstallRequest(FMountRequest& MountRequest)
{
	if (InstallCache.IsValid() == false || PackageStoreBackend.IsValid() == false)
	{
		return FIoStatusBuilder(EIoErrorCode::InvalidCode) << TEXT("Install cache not configured");
	}

	if (MountRequest.bCancelled)
	{
		return EIoErrorCode::Cancelled;
	}

	TAnsiStringBuilder<512> ChunkUrl;
	FStringView Host, TocRelUrl;
	Private::SplitHostUrl(MountRequest.MountArgs.Url, Host, TocRelUrl);

	auto GetChunkUrl = [](
		const FStringView& Host,
		const FOnDemandContainer& Container,
		const FOnDemandChunkEntry& Entry,
		FAnsiStringBuilderBase& OutUrl) -> FAnsiStringBuilderBase&
	{
		OutUrl.Reset();
		if (Host.IsEmpty() == false)
		{
			OutUrl << Host;
		}

		if (!Container.ChunksDirectory.IsEmpty())
		{
			OutUrl << "/" << Container.ChunksDirectory;
		}

		const FString HashString = LexToString(Entry.Hash);
		OutUrl << "/" << HashString.Left(2) << "/" << HashString << ".iochunk";

		return OutUrl;
	};

	auto FetchContainerHeader = [&Host, &GetChunkUrl, &ChunkUrl](
		const FOnDemandContainer& Container,
		FIoContainerHeader& Header) -> FIoStatus 
	{
		const FIoChunkId			ChunkId = CreateContainerHeaderChunkId(Container.ContainerId);
		const FOnDemandChunkEntry*	Entry = Container.ChunkEntries.Find(ChunkId);

		if (Entry == nullptr)
		{
			return EIoErrorCode::Ok;
		}

		UE_LOG(LogIoStoreOnDemand, VeryVerbose, TEXT("Fetching container header, ContainerName='%s'"), *Container.Name);
		TIoStatusOr<FIoBuffer> Response = FHttpClient::Get(GetChunkUrl(Host, Container, *Entry, ChunkUrl).ToView(), 2, EHttpRedirects::Follow);
		if (Response.IsOk() == false)
		{
			FIoStatus Status = FIoStatusBuilder(EIoErrorCode::InvalidCode) << TEXT("Failed to fetch container header chunk from URL");
			return Status;
		}

		FMemoryReaderView Ar(Response.ValueOrDie().GetView());
		Ar << Header;
		Ar.Close();

		if (Ar.IsError() || Ar.IsCriticalError())
		{
			FIoStatus Status = FIoStatusBuilder(EIoErrorCode::FileNotOpen) << TEXT("Failed to serialize container header");
			return Status;
		}

		return EIoErrorCode::Ok;
	};

	TArray<FSharedOnDemandContainer> MountedContainers = GetMountedContainers();
	TSet<FSharedOnDemandContainer> AllContainers;

	Algo::Copy(MountedContainers, AllContainers);
	Algo::Copy(MountRequest.Containers, AllContainers);

	// Fetch all container headers
	for (const FSharedOnDemandContainer& Container : AllContainers)
	{
		if (!Container->Header.IsValid())
		{
			FSharedContainerHeader Header = MakeShared<FIoContainerHeader>();
			if (FIoStatus Status = FetchContainerHeader(*Container, *Header); !Status.IsOk())
			{
				return Status; 
			}
			Container->Header = MoveTemp(Header);
		}
	}

	// Parse the tag sets
	if (MountRequest.MountArgs.TagSets.IsEmpty() == false && MountRequest.TagSets.IsEmpty())
	{
		const FOnDemandToc& Toc = MountRequest.MountArgs.Toc.GetValue();
		for (const FString& Tag : MountRequest.MountArgs.TagSets)
		{
			const FOnDemandTocTagSet* TagSet = Algo::FindBy(Toc.TagSets, Tag, &FOnDemandTocTagSet::Tag);
			if (TagSet == nullptr)
			{
				UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Tag set '%s' doesn't exists"), *Tag);
				continue;
			}

			FTagSet NewTagSet; 
			NewTagSet.Tag = TagSet->Tag;
			for (const FOnDemandTocTagSetPackageList& PackageIndices : TagSet->Packages)
			{
				if (MountRequest.Containers.IsEmpty() || int32(PackageIndices.ContainerIndex) > (MountRequest.Containers.Num() - 1))
				{
					UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Invalid container index in tag set '%s'"), *Tag);
					continue;
				}

				FOnDemandContainer& Container = *MountRequest.Containers[PackageIndices.ContainerIndex];
				if (Container.Header.IsValid() == false)
				{
					UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Tag set specified for invalid container header, Tag='%s', ContainerName='%s'"),
						*Tag, *Container.UniqueName());
					continue;
				}

				NewTagSet.PackageIds.Reserve(PackageIndices.PackageIndicies.Num());
				for (int32 IdIdx : PackageIndices.PackageIndicies)
				{
					NewTagSet.PackageIds.Add(Container.Header->PackageIds[IdIdx]);
				}
			}

			MountRequest.TagSets.Emplace(MoveTemp(NewTagSet));
		}
	}

	TSet<FPackageId> PackageIdsToInstall;
	// Install all packages if no tag set was specified
	if (MountRequest.MountArgs.TagSets.IsEmpty())
	{
		for (const FSharedOnDemandContainer& Container : MountRequest.Containers)
		{
			if (Container->Header.IsValid())
			{
				PackageIdsToInstall.Reserve(Container->Header->PackageIds.Num());
				for (const FPackageId& PackageId : Container->Header->PackageIds)
				{
					PackageIdsToInstall.Add(PackageId);
				}
			}
		}
	}
	else
	{
		for (const FString& Tag : MountRequest.MountArgs.TagSets)
		{
			const FTagSet* TagSet = nullptr;
			for (const FTagSet& Set : MountRequest.TagSets)
			{
				if (Set.Tag == Tag)
				{
					PackageIdsToInstall.Reserve(Set.PackageIds.Num());
					for (const FPackageId& PackageId : Set.PackageIds)
					{
						PackageIdsToInstall.Add(PackageId);
					}
				}
			}
		}
	}

	// Find all I/O chunks fo the specified list of packages
	Private::FInstallData InstallData;
	TSet<FPackageId> Missing;

	FIoStatus Status = BuildInstallData(AllContainers, PackageIdsToInstall, InstallData, Missing);
	if (Status.IsOk() == false)
	{
		return Status;
	}

	// Check the other I/O backends for missing packge chunks
	for (const FPackageId& PackageId : Missing)
	{
		const FIoChunkId ChunkId = CreatePackageDataChunkId(PackageId);
		if (FIoDispatcher::Get().DoesChunkExist(ChunkId) == false)
		{
			// TODO: Fail the install request or just try to continue
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Missing package chunk '%s'"), *LexToString(ChunkId));
		}
	}

	// Download all chunks
	const int32 MaxConcurrentRequests = 32;
	int32 ConcurrentRequests = 0;

	FHttpClientConfig HttpConfig;
	HttpConfig.MaxConnectionCount = 8;
	HttpConfig.MaxRetryCount = 2;
	HttpConfig.Endpoints.Add(FString(Host));

	TUniquePtr<FHttpClient> HttpClient = FHttpClient::Create(MoveTemp(HttpConfig));
	if (HttpClient.IsValid() == false)
	{
		return FIoStatusBuilder(EIoErrorCode::InvalidCode) << TEXT("Failed to initialize HTTP client");
	}

	uint64 TotalChunkCount		= 0;
	uint64 TotalBytes			= 0;
	uint64 DownloadedChunkCount	= 0;
	uint64 DownloadedBytes		= 0;

	for (const auto& Kv : InstallData)
	{
		FOnDemandContainer& Container				= *Kv.Key;
		const Private::FContainerInstallData& Data	= Kv.Value;

		for (const FIoChunkId& ChunkId : Data.ResolvedChunks)
		{
			const FOnDemandChunkEntry& Entry = Container.ChunkEntries.FindRef(ChunkId);
			++TotalChunkCount;
			TotalBytes += Entry.EncodedSize;

			if (InstallCache->ContainsChunk(Entry.Hash))
			{
				continue;
			}

			++DownloadedChunkCount;
			DownloadedBytes += Entry.EncodedSize;

			FOnDemandChunkEntry* ChunkEntry = Container.ChunkEntries.Find(ChunkId);
			ConcurrentRequests++;
			HttpClient->Get(
				GetChunkUrl(FStringView(), Container, *ChunkEntry, ChunkUrl).ToView(),
				[this, &Status, ChunkId, ChunkEntry, &ConcurrentRequests]
				(TIoStatusOr<FIoBuffer> ChunkStatus, uint64 DurationMs) mutable
				{
					ConcurrentRequests--;
					if (!ChunkStatus.IsOk())
					{
						Status = ChunkStatus.Status();
						return;
					}

					Status = InstallCache->PutChunk(ChunkStatus.ConsumeValueOrDie(), ChunkEntry->Hash);
				});

			while (ConcurrentRequests >= MaxConcurrentRequests)
			{
				HttpClient->Tick();
			}

			if (Status.IsOk() == false)
			{
				return Status;
			}
		}
	}

	while (HttpClient->Tick())
		;

	// TODO: Only mount what has been installed
	for (const auto& Kv : InstallData)
	{
		FOnDemandContainer& Container				= *Kv.Key;
		const Private::FContainerInstallData& Data	= Kv.Value;
		if (!Data.PackageIds.IsEmpty())
		{
			check(Container.Header.IsValid());
			const FIoStatus MountStatus = PackageStoreBackend->Mount(Container.UniqueName(), Container.Header);
			check(MountStatus.IsOk());
		}

		EnumAddFlags(Container.Flags, EOnDemandContainerFlags::Installed);
	}

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Downloaded %llu (%.2lf MiB) of total %llu (%.2lf MiB) requested chunk(s)'"),
		DownloadedChunkCount, double(DownloadedBytes) / 1024.0 / 1024.0, TotalChunkCount, double(TotalBytes) / 1024.0 / 1024.0);

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
				UE_LOG(LogIoStoreOnDemand, Log, TEXT("Mounting container '%s', Entries=%d, Flags='%s'"),
					*Container->Name, Container->ChunkEntries.Num(), *LexToString(Container->Flags));

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

	TStringBuilder<128> Sb;
	FStringView ChunksDirectory;
	{
		if (TocPath.IsEmpty() == false)
		{
			//Algo::Transform(TocPath, AppendChars(Sb), FChar::ToLower);
			FPathViews::Append(Sb, TocPath);
		}
		else
		{
			// Algo::Transform(Toc.Header.ChunksDirectory, AppendChars(Sb), FChar::ToLower);
			FPathViews::Append(Sb, Toc.Header.ChunksDirectory);
		}
		FPathViews::Append(Sb, TEXT("chunks"));

		ChunksDirectory = Sb;
		if (ChunksDirectory.StartsWith('/'))
		{
			ChunksDirectory.RemovePrefix(1);
		}
		if (ChunksDirectory.EndsWith('/'))
		{
			ChunksDirectory.RemoveSuffix(1);
		}
	}

	for (FOnDemandTocContainerEntry& ContainerEntry : Toc.Containers)
	{
		FSharedOnDemandContainer Container = MakeShared<FOnDemandContainer>();
		Container->Name					= MoveTemp(ContainerEntry.ContainerName);
		Container->MountId				= MountId;
		Container->ChunksDirectory		= StringCast<ANSICHAR>(ChunksDirectory.GetData(), ChunksDirectory.Len());
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

TArray<FSharedOnDemandContainer> FOnDemandIoStore::GetMountedContainers()
{
	UE::TUniqueLock Lock(ContainerMutex);
	return Containers;
}

} // namespace UE::IoStore
