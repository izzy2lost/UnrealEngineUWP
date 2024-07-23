// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandIoStore.h"
#include "OnDemandHttpClient.h"
#include "OnDemandInstallCache.h"
#include "OnDemandPackageStoreBackend.h"

#include "Algo/Accumulate.h"
#include "Algo/Copy.h"
#include "Algo/RemoveIf.h"
#include "Algo/Transform.h"
#include "Async/ManualResetEvent.h"
#include "Async/UniqueLock.h"
#include "Containers/RingBuffer.h"
#include "Containers/StringConv.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoContainerHeader.h"
#include "IO/PackageStore.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CoreDelegatesInternal.h"
#include "Misc/CoreMisc.h"
#include "Misc/EncryptionKeyManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Serialization/MemoryReader.h"
#if !(UE_BUILD_SHIPPING|UE_BUILD_TEST)
#include "String/LexFromString.h"
#endif

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
	FString DirName;

	if (IsRunningDedicatedServer())
	{
		if (!FForkProcessHelper::IsForkRequested())
		{
			DirName = TEXT("InstallCacheServer");
		}
		else
		{
			if (!FForkProcessHelper::IsForkedChildProcess())
			{
				UE_LOG(LogIoStoreOnDemand, Fatal, TEXT("Attempting to create IOStore cache before forking!"));
			}

			FString CommandLineDir;
			bool bUsePathFromCommandLine = FParse::Value(FCommandLine::Get(), TEXT("ServerIOInstallCacheDir="), CommandLineDir);
			if (bUsePathFromCommandLine)
			{
				if (!FPaths::ValidatePath(CommandLineDir))
				{
					bUsePathFromCommandLine = false;
					UE_LOG(LogIoStoreOnDemand, Error, TEXT("Invalid ServerIOInstallCacheDir from command line: %s"), *CommandLineDir);
				}
				else if (!FPaths::IsRelative(CommandLineDir))
				{
					bUsePathFromCommandLine = false;
					UE_LOG(LogIoStoreOnDemand, Error, TEXT("ServerIOInstallCacheDir from command line is not relative: %s"), *CommandLineDir);
				}

				if (bUsePathFromCommandLine)
				{
					return FPaths::ProjectPersistentDownloadDir() / CommandLineDir;
				}
			}

			DirName = FString::Printf(TEXT("InstallCacheServer-%u"), FPlatformProcess::GetCurrentProcessId());
		}
	}
#if WITH_EDITOR
	else if (GIsEditor)
	{
		DirName = TEXT("InstallCacheEditor");
	}
#endif //if WITH_EDITOR
	else
	{
		DirName = TEXT("InstallCache");
	}

	return FPaths::ProjectPersistentDownloadDir() / TEXT("IoStore") / DirName;
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
static FAnsiStringBuilderBase& GetChunkUrl(
	const FStringView& Host,
	const FOnDemandContainer& Container,
	const FOnDemandChunkEntry& Entry,
	FAnsiStringBuilderBase& OutUrl)
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
}

static FIoStatus FetchContainerHeader(
	FStringView Host,
	FAnsiStringBuilderBase& ChunkUrlBuilder,
	FSharedOnDemandContainer Container,
	FIoContainerHeader& OutHeader)
{
	const FIoChunkId			ChunkId = CreateContainerHeaderChunkId(Container->ContainerId);
	const FOnDemandChunkEntry*	Entry = Container->FindChunkEntry(ChunkId);

	if (Entry == nullptr)
	{
		return EIoErrorCode::Ok;
	}

	UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Fetching container header, ContainerName='%s'"), *Container->Name);
	TIoStatusOr<FIoBuffer> Response = FHttpClient::Get(GetChunkUrl(Host, *Container, *Entry, ChunkUrlBuilder).ToView(), 2, EHttpRedirects::Follow);

	if (Response.IsOk() == false)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::ReadError) << TEXT("Failed to fetch container header chunk");
		return Status;
	}

	FOnDemandChunkInfo ChunkInfo(Container, *Entry);

	FIoChunkDecodingParams Params;
	Params.CompressionFormat = ChunkInfo.CompressionFormat();
	Params.EncryptionKey = ChunkInfo.EncryptionKey();
	Params.BlockSize = ChunkInfo.BlockSize();
	Params.TotalRawSize = ChunkInfo.RawSize();
	Params.RawOffset = 0;
	Params.EncodedOffset = 0;
	Params.EncodedBlockSize = ChunkInfo.Blocks();
	Params.BlockHash = ChunkInfo.BlockHashes();

	FIoBuffer EncodedChunk = Response.ConsumeValueOrDie();
	FIoBuffer RawChunk(ChunkInfo.RawSize());
	if (FIoChunkEncoding::Decode(Params, EncodedChunk.GetView(), RawChunk.GetMutableView()) == false)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::ReadError)
			<< TEXT("Failed to decode container header chunk");
	}

	FMemoryReaderView Ar(RawChunk.GetView());
	Ar << OutHeader;
	Ar.Close();

	if (Ar.IsError() || Ar.IsCriticalError())
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::FileNotOpen) << TEXT("Failed to serialize container header");
		return Status;
	}

	return EIoErrorCode::Ok;
}

///////////////////////////////////////////////////////////////////////////////
using FPackageStoreEntryMap		= TMap<FPackageId, const FFilePackageStoreEntry*>;
using FSoftPackageReferenceMap	= TMap<FPackageId, const FFilePackageStoreEntrySoftReferences*>;

struct FContainerInstallData
{
	FPackageStoreEntryMap		PackageStoreEntries;
	FSoftPackageReferenceMap	SoftPackageReferences;
	TSet<FPackageId>			PackageIds;
	TSet<uint32>				ResolvedChunks;
	uint64						TotalSize = 0;
};

using FInstallData = TMap<FSharedOnDemandContainer, FContainerInstallData>;

FIoStatus BuildInstallData(
	const TSet<FSharedOnDemandContainer>& Containers,
	const TSet<FPackageId>& PackageIds,
	FInstallData& OutInstallData,
	TSet<FPackageId>& OutMissing)
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

		TConstArrayView<FFilePackageStoreEntrySoftReferences> SoftReferences;
		if (Header.SoftPackageReferences.bContainsSoftPackageReferences)
		{
			Data.PackageStoreEntries.Reserve(Header.PackageIds.Num());
			SoftReferences = MakeArrayView<const FFilePackageStoreEntrySoftReferences>(
				reinterpret_cast<const FFilePackageStoreEntrySoftReferences*>(Header.SoftPackageReferences.PackageIndices.GetData()),
				Header.PackageIds.Num());
		}

		for (int32 PackageIndex = 0; const FFilePackageStoreEntry& Entry : Entries)
		{
			const FPackageId PackageId = Header.PackageIds[PackageIndex];
			Data.PackageStoreEntries.Add(PackageId, &Entry);

			if (SoftReferences.IsEmpty() == false)
			{
				Data.SoftPackageReferences.Add(PackageId, &SoftReferences[PackageIndex]);
			}

			++PackageIndex;
		}
	}

	// Traverse dependencies for each package id
	using FQueue = TRingBuffer<FPackageId>;

	FQueue Queue;
	TSet<FPackageId> Visitied;

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
			if (const FFilePackageStoreEntry* Entry = Data.PackageStoreEntries.FindRef(PackageId))
			{
				Data.PackageIds.Add(PackageId);

				// Add hard references 
				for (const FPackageId& ImportedPackageId : Entry->ImportedPackages)
				{
					if (!Visitied.Contains(ImportedPackageId))
					{
						Queue.Add(ImportedPackageId);
					}
				}

				// Add soft references 
				const FIoContainerHeader& Header = *Kv.Key->Header;
				if (const FFilePackageStoreEntrySoftReferences* SoftRefs = Data.SoftPackageReferences.FindRef(PackageId))
				{
					for (uint32 Index : SoftRefs->Indices)
					{
						const FPackageId SoftPackageReference = Header.SoftPackageReferences.PackageIds[int32(Index)];
						if (!Visitied.Contains(SoftPackageReference))
						{
							Queue.Add(SoftPackageReference);
						}
					}
				}

				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			OutMissing.Add(PackageId);
		}
	}

	for (TPair<FSharedOnDemandContainer, FContainerInstallData>& Kv : OutInstallData)
	{
		const FOnDemandContainer& Container = *Kv.Key;
		FContainerInstallData& Data			= Kv.Value;

		for (const FPackageId& PackageId : Data.PackageIds)
		{
			const FIoChunkId PackageChunkId	= CreatePackageDataChunkId(PackageId);
			int32 EntryIndex				= Container.FindChunkEntryIndex(PackageChunkId);

			if (EntryIndex == INDEX_NONE)
			{
				UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Missing package data"));
				continue;
			}

			Data.ResolvedChunks.Add(EntryIndex);
			Data.TotalSize += Container.ChunkEntries[EntryIndex].EncodedSize;

			const EIoChunkType AdditionalPackageChunkTypes[] =
			{
				EIoChunkType::BulkData,
				EIoChunkType::OptionalBulkData,
				EIoChunkType::MemoryMappedBulkData 
			};

			for (EIoChunkType ChunkType : AdditionalPackageChunkTypes)
			{
				const FIoChunkId ChunkId = CreateIoChunkId(PackageId.Value(), 0, ChunkType);
				if (EntryIndex = Container.FindChunkEntryIndex(ChunkId); EntryIndex != INDEX_NONE)
				{
					Data.ResolvedChunks.Add(EntryIndex);
					Data.TotalSize += Container.ChunkEntries[EntryIndex].EncodedSize;
				}
			}
		}

		// For now we always download these chunks
		for (int32 EntryIndex = 0; const FIoChunkId& ChunkId : Container.ChunkIds)
		{
			switch(ChunkId.GetChunkType())
			{
				case EIoChunkType::ExternalFile:
				case EIoChunkType::ShaderCodeLibrary:
				case EIoChunkType::ShaderCode:
				{
					Data.ResolvedChunks.Add(EntryIndex);
					Data.TotalSize += Container.ChunkEntries[EntryIndex].EncodedSize;
				}
				default:
					break;
			}
			++EntryIndex;
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
		TEXT("StreamOnDemand"),
		TEXT("InstallOnDemand"),
		TEXT("Encrypted")
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
	return FString::ConstructFromPtrSize(Sb.ToString(), Sb.Len());
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
	FCoreDelegates::OnPostFork.RemoveAll(this);

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
	const FIoStatus CacheStatus = InitializeOnDemandInstallCache();
	if (CacheStatus.GetErrorCode() == EIoErrorCode::PendingFork)
	{
		UE_LOG(LogIoStoreOnDemand, Display, TEXT("Deferring install cache initialization until after process fork"));
		if (!FCoreDelegates::OnPostFork.IsBoundToObject(this))
		{
			FCoreDelegates::OnPostFork.AddRaw(this, &FOnDemandIoStore::OnPostFork);
		}
	}
	else if (!CacheStatus.IsOk())
	{
		return CacheStatus;
	}

#if 0
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
#endif

	return EIoErrorCode::Ok;
}

void FOnDemandIoStore::Mount(FOnDemandMountArgs&& Args, FOnDemandMountCompleted&& OnCompleted)
{
	if (Args.MountId.IsEmpty())
	{
		return OnCompleted(FOnDemandMountResult{ .Status = FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid mount ID")) });
	}

	{
		UE::TUniqueLock Lock(MountRequestMutex);
		FSharedMountRequest& MountRequest = MountRequests.FindOrAdd(Args.MountId);

		if (MountRequest.IsValid())
		{
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Mount request '%s' is already mounting"), *Args.MountId);
			return OnCompleted(FOnDemandMountResult{ .Status = FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Mount in progress")) });
		}

		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Enqueing mount request, MountId='%s'"), *Args.MountId);
		
		MountRequest = MakeShared<FMountRequest>();
		MountRequest->Args			= MoveTemp(Args);
		MountRequest->OnCompleted	= MoveTemp(OnCompleted);
	}

	TryEnterTickLoop();
}

void FOnDemandIoStore::Install(FOnDemandInstallArgs&& Args, FOnDemandInstallCompleted&& OnCompleted)
{
	FSharedInstallRequest InstallRequest = MakeShared<FInstallRequest>();
	InstallRequest->Args		= MoveTemp(Args);
	InstallRequest->OnCompleted = MoveTemp(OnCompleted);

	{
		UE::TUniqueLock Lock(MountRequestMutex);
		InstallRequests.Add(MoveTemp(InstallRequest));
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

TIoStatusOr<uint64> FOnDemandIoStore::GetInstallSize(const FOnDemandGetInstallSizeArgs& Args) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoStore::GetInstallSize);

	TSet<FSharedOnDemandContainer> AllContainers(
		const_cast<FOnDemandIoStore*>(this)->GetContainers([](const FSharedOnDemandContainer& Container)
			{
				return EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::InstallOnDemand);
			})
	);

	TSet<FPackageId> PackageIdsToInstall;
	PackageIdsToInstall.Append(Args.Packages);

	Private::FInstallData InstallData;
	TSet<FPackageId> Missing;

	FIoStatus Status = BuildInstallData(AllContainers, PackageIdsToInstall, InstallData, Missing);
	if (Status.IsOk() == false)
	{
		return Status;
	}

	const uint64 RetSize = Algo::TransformAccumulate(InstallData,
		[](const Private::FInstallData::ElementType& Pair) { return Pair.Value.TotalSize; },
		uint64(0));

	return RetSize;
}

FOnDemandChunkInfo FOnDemandIoStore::GetStreamingChunkInfo(const FIoChunkId& ChunkId)
{
	return GetChunkInfo(ChunkId, EOnDemandContainerFlags::Mounted | EOnDemandContainerFlags::StreamOnDemand);
}

FOnDemandChunkInfo FOnDemandIoStore::GetInstalledChunkInfo(const FIoChunkId& ChunkId)
{
	return GetChunkInfo(ChunkId, EOnDemandContainerFlags::Mounted | EOnDemandContainerFlags::InstallOnDemand);
}

void FOnDemandIoStore::OnPostFork(EForkProcessRole ProcessRole)
{
	FCoreDelegates::OnPostFork.RemoveAll(this);

	if (ProcessRole != EForkProcessRole::Child)
	{
		return;
	}

	const FIoStatus Status = Initialize();
	if (!Status.IsOk())
	{
		UE_LOG(LogIoStoreOnDemand, Fatal, TEXT("Failed to initialize I/O store on demand (post fork), reason '%s'"), *Status.ToString());
	}
}

FIoStatus FOnDemandIoStore::InitializeOnDemandInstallCache()
{
	if (FForkProcessHelper::IsForkRequested() && !FForkProcessHelper::IsForkedChildProcess())
	{
		return FIoStatusBuilder(EIoErrorCode::PendingFork) << TEXT("Install cache waiting for fork");
	}

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

#if !(UE_BUILD_SHIPPING|UE_BUILD_TEST)
	FString ParamValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("Iad.Fill="), ParamValue))
	{
		ParamValue.TrimStartAndEndInline();
		int64 FillSize = -1;
		LexFromString(FillSize, ParamValue);

		if (FillSize > 0)
		{
			if (ParamValue.EndsWith(TEXT("GB")))
			{
				FillSize = FillSize << 30;
			}
			if (ParamValue.EndsWith(TEXT("MB")))
			{
				FillSize = FillSize << 20;
			}

			UE_LOG(LogIoStoreOnDemand, Log, TEXT("Filling install cache with %.2lf MiB of dummy data"), double(FillSize) / 1024.0 / 1024.0);

			FIoStatus	Status = FIoStatus::Ok;
			uint64		Seed = 1;
			while (FillSize >= 0 && Status.IsOk())
			{
				const uint64		ChunkSize = 256 << 10;
				FIoBuffer			Chunk(ChunkSize);
				TArrayView<uint64>	Values(reinterpret_cast<uint64*>(Chunk.GetData()), ChunkSize / sizeof(uint64));

				for (uint64& Value : Values)
				{
					Value = Seed;
				}

				const FIoHash ChunkHash = FIoHash::HashBuffer(Chunk.GetView());
				Status = InstallCache->PutChunk(MoveTemp(Chunk), ChunkHash);
				Seed++;
				FillSize -= ChunkSize;
			}

			if (Status.IsOk())
			{
				Status = InstallCache->Flush();
			}

			UE_CLOG(!Status.IsOk(), LogIoStoreOnDemand, Warning, TEXT("Failed to fill install cache with dummy data"));
		}
	}
#endif

	return EIoErrorCode::Ok;
}

FOnDemandChunkInfo FOnDemandIoStore::GetChunkInfo(const FIoChunkId& ChunkId, EOnDemandContainerFlags ContainerFlags)
{
	TUniqueLock Lock(ContainerMutex);

	for (FSharedOnDemandContainer& Container : Containers)
	{
		if (EnumHasAllFlags(Container->Flags, ContainerFlags))
		{
			if (const FOnDemandChunkEntry* Entry = Container->FindChunkEntry(ChunkId))
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
	TArray<FSharedMountRequest> LocalMountRequests;
	{
		UE::TUniqueLock Lock(MountRequestMutex);
		for (TPair<FString, FSharedMountRequest>& Kv : MountRequests)
		{
			LocalMountRequests.Add(Kv.Value);
		}
	}

	bool bTicked = LocalMountRequests.IsEmpty() == false;

	// Tick mount request(s)
	for (FSharedMountRequest& Request : LocalMountRequests)
	{
		{
			UE::TUniqueLock Lock(MountRequestMutex);
			MountRequests.Remove(Request->Args.MountId);
		}

		if (FIoStatus Status = TickMountRequest(*Request); !Status.IsOk())
		{
			FOnDemandMountResult MountResult
			{
				.Status = MoveTemp(Status),
				.DurationInSeconds = Request->DurationInSeconds 
			};

			if (EnumHasAnyFlags(Request->Args.Options, EOnDemandMountOptions::CallbackOnGameThread))
			{
				ExecuteOnGameThread(
					UE_SOURCE_LOCATION,
					[OnCompleted = MoveTemp(Request->OnCompleted), MountResult = MoveTemp(MountResult)]() mutable
					{
						OnCompleted(MoveTemp(MountResult));
					});
			}
			else
			{
				FOnDemandMountCompleted OnCompleted = MoveTemp(Request->OnCompleted);
				OnCompleted(MoveTemp(MountResult));
			}
			continue;
		}

		TStringBuilder<128> Sb;
		{
			UE::TUniqueLock Lock(ContainerMutex);
			for (const FSharedOnDemandContainer& Container : Request->Containers)
			{
				if (EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::Mounted))
				{
					UE_LOG(LogIoStoreOnDemand, Log, TEXT("Ignoring already mounted container, ContainerName='%s'"), *Container->Name);
					continue;
				}

				Containers.Add(Container);

				// To conform with PAK mount behavior, mount requests are completed even if encryption keys are pending
				if (EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::Encrypted) &&
					Container->EncryptionKey.IsValid() == false)
				{
					FGuid KeyGuid;
					ensure(FGuid::Parse(Container->EncryptionKeyGuid, KeyGuid));
					if (FEncryptionKeyManager::Get().TryGetKey(KeyGuid, Container->EncryptionKey) == false)
					{
						EnumAddFlags(Container->Flags, EOnDemandContainerFlags::PendingEncryptionKey);
						UE_LOG(LogIoStoreOnDemand, Log, TEXT("Deferring container '%s' until encryption key '%s' becomes available"),
							*Container->Name, *Container->EncryptionKeyGuid);
					}
				}

				if (EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::PendingEncryptionKey) == false)
				{
					EnumAddFlags(Container->Flags, EOnDemandContainerFlags::Mounted);
					Container->EncryptionKeyGuid.Empty();
					Sb.Reset();
					LexToString(Container->Flags, Sb);
					UE_LOG(LogIoStoreOnDemand, Log, TEXT("Mounting container '%s', Entries=%d, Flags='%s'"),
						*Container->Name, Container->ChunkEntries.Num(), Sb.ToString());
				}
			}
		}

		FOnDemandMountResult MountResult
		{
			.MountId = MoveTemp(Request->Args.MountId),
			.DurationInSeconds = Request->DurationInSeconds
		};

		if (EnumHasAnyFlags(Request->Args.Options, EOnDemandMountOptions::CallbackOnGameThread))
		{
			ExecuteOnGameThread(
				UE_SOURCE_LOCATION,
				[OnCompleted = MoveTemp(Request->OnCompleted), MountResult = MoveTemp(MountResult)]() mutable
				{
					OnCompleted(MoveTemp(MountResult));
				});
		}
		else
		{
			FOnDemandMountCompleted OnCompleted = MoveTemp(Request->OnCompleted);
			OnCompleted(MoveTemp(MountResult));
		}
	}

	TArray<FSharedInstallRequest> LocalInstallRequests;
	{
		UE::TUniqueLock Lock(MountRequestMutex);
		LocalInstallRequests = InstallRequests;
	}

	// Tick install request(s)
	for (FSharedInstallRequest& Request : LocalInstallRequests)
	{
		FIoStatus Status = TickInstallRequest(*Request);
		if (Status.GetErrorCode() == EIoErrorCode::Unknown)
		{
			UE_LOG(LogIoStoreOnDemand, Log, TEXT("Deferring install request, reason '%s'"), *Status.ToString());
			continue;
		}

		{
			UE::TUniqueLock Lock(MountRequestMutex);
			InstallRequests.Remove(Request);
		}

		FOnDemandInstallResult InstallResult
		{
			.Status = MoveTemp(Status),
			.DurationInSeconds = Request->DurationInSeconds,
			.TotalContentSize = Request->TotalContentSize,
			.TotalInstallSize = Request->TotalInstallSize
		};

		if (EnumHasAnyFlags(Request->Args.Options, EOnDemandInstallOptions::CallbackOnGameThread))
		{
			ExecuteOnGameThread(
				UE_SOURCE_LOCATION,
				[OnCompleted = MoveTemp(Request->OnCompleted), InstallResult = MoveTemp(InstallResult)]() mutable
				{
					OnCompleted(MoveTemp(InstallResult));
				});
		}
		else
		{
			FOnDemandInstallCompleted OnCompleted = MoveTemp(Request->OnCompleted);
			OnCompleted(MoveTemp(InstallResult));
		}

		bTicked = true;
	}

	return bTicked;
}

FIoStatus FOnDemandIoStore::TickMountRequest(FMountRequest& MountRequest)
{
	UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Ticking mount request, MountId='%s'"), *MountRequest.Args.MountId);

	const double StartTime = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		MountRequest.DurationInSeconds = FPlatformTime::Seconds() - StartTime;
	};

	FOnDemandMountArgs& Args = MountRequest.Args;

	// Find containers matching the mount ID
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

	// TODO: Treat this as an error?
	if (MountRequest.Containers.IsEmpty() == false)
	{
		return FIoStatus::Ok;
	}

	FStringView Host, TocRelUrl;
	Private::SplitHostUrl(Args.Url, Host, TocRelUrl);
	const FStringView TocPath = FPathViews::GetPath(TocRelUrl);

	if (Args.Toc)
	{
		CreateContainersFromToc(Args.MountId, TocPath, *Args.Toc, MountRequest.Containers);
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

		Args.Toc = MakeUnique<FOnDemandToc>(TocStatus.ConsumeValueOrDie());

		CreateContainersFromToc(Args.MountId, TocPath, *Args.Toc, MountRequest.Containers);
	}
	else if (Args.Url.IsEmpty() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Loading TOC from URL '%s'"), *Args.Url);

		const uint32 RetryCount = 2;
		const bool bFollowRedirects = true;
		TIoStatusOr<FOnDemandToc> TocStatus = FOnDemandToc::LoadFromUrl(Args.Url, 2, bFollowRedirects);

		if (!TocStatus.IsOk())
		{
			return TocStatus.Status();
		}

		Args.Toc = MakeUnique<FOnDemandToc>(TocStatus.ConsumeValueOrDie());

		CreateContainersFromToc(Args.MountId, TocPath, *Args.Toc, MountRequest.Containers);
	}

	for (const FSharedOnDemandContainer& Container : MountRequest.Containers)
	{
		if (EnumHasAnyFlags(Args.Options, EOnDemandMountOptions::StreamOnDemand))
		{
			EnumAddFlags(Container->Flags, EOnDemandContainerFlags::StreamOnDemand);
		}
		else if (EnumHasAnyFlags(Args.Options, EOnDemandMountOptions::InstallOnDemand))
		{
			EnumAddFlags(Container->Flags, EOnDemandContainerFlags::InstallOnDemand);
		}
	}

	return EIoErrorCode::Ok;
}

FIoStatus FOnDemandIoStore::TickInstallRequest(FInstallRequest& InstallRequest)
{
	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Ticking install request, ContentHandle='%s'"),
		*LexToString(InstallRequest.Args.ContentHandle));

	const double StartTime = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		InstallRequest.DurationInSeconds = FPlatformTime::Seconds() - StartTime;
	};

	if (InstallRequest.Args.ContentHandle.IsValid() == false)
	{
		return FIoStatusBuilder(EIoErrorCode::InvalidParameter) << TEXT("Invalid content handle");
	}

	if (InstallCache.IsValid() == false || PackageStoreBackend.IsValid() == false)
	{
		if (FForkProcessHelper::IsForkRequested() && !FForkProcessHelper::IsForkedChildProcess())
		{
			return FIoStatusBuilder(EIoErrorCode::PendingFork) << TEXT("Install cache waiting for fork");
		}

		return FIoStatusBuilder(EIoErrorCode::InvalidCode) << TEXT("Install cache not configured");
	}

	//TODO: Implement cancellation

	TAnsiStringBuilder<512> ChunkUrl;
	FStringView Host, TocRelUrl;
	Private::SplitHostUrl(InstallRequest.Args.Url, Host, TocRelUrl);

	// Only install content from non-streaming container(s)
	TSet<FSharedOnDemandContainer> ContainersForInstallation(
		GetContainers([](const FSharedOnDemandContainer& Container)
		{ 
			return EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::InstallOnDemand);
		})
	);

	// TODO: move this to mount
	// Fetch all container headers
	for (const FSharedOnDemandContainer& Container : ContainersForInstallation)
	{
		if (EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::PendingEncryptionKey))
		{
			return FIoStatusBuilder(EIoErrorCode::Unknown)
				<< TEXT("Deferring install request until encryption key(s) becomes available");
		}

		if (!Container->Header.IsValid())
		{
			FSharedContainerHeader Header = MakeShared<FIoContainerHeader>();
			if (FIoStatus Status = Private::FetchContainerHeader(Host, ChunkUrl, Container, *Header); !Status.IsOk())
			{
				return Status; 
			}
			Container->Header = MoveTemp(Header);
		}
	}

	TSet<FPackageId> PackageIdsToInstall;
	for (const FPackageId& PackageId : InstallRequest.Args.PackageIds)
	{
		PackageIdsToInstall.Add(PackageId);
	}

	// Install all packages if no tag set(s) was specified
	if (InstallRequest.Args.TagSets.IsEmpty())
	{
		for (const FSharedOnDemandContainer& Container : ContainersForInstallation)
		{
			if (Container->Header.IsValid() && Container->MountId == InstallRequest.Args.MountId)
			{
				for (const FPackageId& PackageId : Container->Header->PackageIds)
				{
					PackageIdsToInstall.Add(PackageId);
				}
			}
		}
	}
	else
	{
		for (const FSharedOnDemandContainer& Container : ContainersForInstallation)
		{
			if (Container->Header.IsValid() == false)
			{
				continue;
			}

			if (InstallRequest.Args.MountId.IsEmpty() == false && InstallRequest.Args.MountId != Container->MountId)
			{
				continue;
			}

			for (const FString& Tag : InstallRequest.Args.TagSets)
			{
				for (const FOnDemandTagSet& TagSet : Container->TagSets)
				{
					if (TagSet.Tag == Tag)
					{
						for (const uint32 PackageIndex : TagSet.PackageIndicies)
						{
							const FPackageId PackageId = Container->Header->PackageIds[IntCastChecked<int32>(PackageIndex)];
							PackageIdsToInstall.Add(PackageId);
						}
					}
				}
			}
		}
	}

	// Its OK for PackageIdsToInstall to be empty at this point. Any chunks not referenced by a package must still be installed.

	// Find all I/O chunks fo the specified list of packages
	Private::FInstallData InstallData;
	TSet<FPackageId> Missing;

	FIoStatus Status = BuildInstallData(ContainersForInstallation, PackageIdsToInstall, InstallData, Missing);
	if (Status.IsOk() == false)
	{
		return Status;
	}

	// Check the other I/O backends for missing package chunks
	for (const FPackageId& PackageId : Missing)
	{
		const FIoChunkId ChunkId = CreatePackageDataChunkId(PackageId);
		if (FIoDispatcher::Get().DoesChunkExist(ChunkId) == false)
		{
			// TODO: Fail the install request or just try to continue
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Missing package chunk '%s'"), *LexToString(ChunkId));
		}
	}

	// Purge
	{
		TMap<FIoHash, uint64> ChunksToInstall;
		for (const auto& Kv : InstallData)
		{
			FOnDemandContainer&						Container = *Kv.Key;
			const Private::FContainerInstallData&	Data = Kv.Value;

			for (int32 EntryIndex : Data.ResolvedChunks)
			{
				const FOnDemandChunkEntry& Entry = Container.ChunkEntries[EntryIndex];
				ChunksToInstall.Add(Entry.Hash, Entry.EncodedSize);
			}
		}

		if (Status = InstallCache->Purge(MoveTemp(ChunksToInstall)); Status.IsOk() == false)
		{
			return Status;
		}
	}

	// Download all chunks
	const int32 MaxConcurrentRequests = 16;
	int32		ConcurrentRequests = 0;

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

		for (int32 EntryIndex : Data.ResolvedChunks)
		{
			const FIoChunkId& ChunkId				= Container.ChunkIds[EntryIndex];
			const FOnDemandChunkEntry& ChunkEntry	= Container.ChunkEntries[EntryIndex];

			++TotalChunkCount;
			TotalBytes += ChunkEntry.EncodedSize;

			if (InstallCache->IsChunkCached(ChunkEntry.Hash))
			{
				continue;
			}

			++DownloadedChunkCount;
			DownloadedBytes += ChunkEntry.EncodedSize;

			ConcurrentRequests++;
			HttpClient->Get(
				Private::GetChunkUrl(FStringView(), Container, ChunkEntry, ChunkUrl).ToView(),
				[this, &Status, ChunkId, ChunkEntry, &ConcurrentRequests]
				(TIoStatusOr<FIoBuffer> ChunkStatus, uint64 DurationMs) mutable
				{
					ConcurrentRequests--;
					if (!ChunkStatus.IsOk())
					{
						Status = ChunkStatus.Status();
						return;
					}

					FIoBuffer Chunk			= ChunkStatus.ConsumeValueOrDie();
					const FIoHash ChunkHash = FIoHash::HashBuffer(Chunk.GetView());
					if (ChunkHash != ChunkEntry.Hash)
					{
						Status = FIoStatus(EIoErrorCode::ReadError, TEXTVIEW("Hash mismatch"));
						return;
					}
					Status = InstallCache->PutChunk(MoveTemp(Chunk), ChunkHash);
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
	
	if (Status = InstallCache->Flush(); Status.IsOk() == false)
	{
		return FIoStatusBuilder(EIoErrorCode::WriteError) << TEXT("Failed to flush downloaded content to cache");
	}

	TSharedPtr<FOnDemandInternalContentHandle, ESPMode::ThreadSafe>& ContentHandle = InstallRequest.Args.ContentHandle.Handle;
	if (ContentHandle->IoStore.IsValid() == false)
	{
		// First time this content handle is used
		ContentHandle->IoStore = AsWeak(); 
	}

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

		{
			UE::TUniqueLock Lock(ContainerMutex);
			FOnDemandChunkEntryReferences& References = Container.FindOrAddChunkEntryReferences(*ContentHandle);
			for (int32 EntryIndex : Data.ResolvedChunks)
			{
				References.Indices[EntryIndex] = true;
			}
		}
	}

	InstallRequest.TotalContentSize = TotalBytes;
	InstallRequest.TotalInstallSize = DownloadedBytes;

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Downloaded %llu (%.2lf MiB) of total %llu (%.2lf MiB) requested chunk(s)'"),
		DownloadedChunkCount, double(DownloadedBytes) / 1024.0 / 1024.0, TotalChunkCount, double(TotalBytes) / 1024.0 / 1024.0);

	return EIoErrorCode::Ok;
}

void FOnDemandIoStore::OnEncryptionKeyAdded(const FGuid& Id, const FAES::FAESKey& Key)
{
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
					Container->EncryptionKeyGuid.Empty();
				}
			}
		}
	}

	// Tick pending install request waiting on encryption key(s)
	TryEnterTickLoop();
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

		const EIoContainerFlags ContainerFlags = static_cast<EIoContainerFlags>(ContainerEntry.ContainerFlags);
		if (EnumHasAnyFlags(ContainerFlags, EIoContainerFlags::Encrypted))
		{
			EnumAddFlags(Container->Flags, EOnDemandContainerFlags::Encrypted);
		}

		//TODO: Sort before uploading
		ContainerEntry.Entries.Sort([](const FOnDemandTocEntry& Lhs, const FOnDemandTocEntry& Rhs)
		{
			return Lhs.ChunkId < Rhs.ChunkId;
		});

		const int32	EntryCount		= ContainerEntry.Entries.Num();
		const uint64 TotalEntrySize = (sizeof(FIoChunkId) + sizeof(FOnDemandTocEntry)) * EntryCount;
		Container->ChunkEntryData	= MakeUnique<uint8[]>(TotalEntrySize);
		Container->ChunkIds			= MakeArrayView<FIoChunkId>(reinterpret_cast<FIoChunkId*>(Container->ChunkEntryData.Get()), EntryCount);
		Container->ChunkEntries		= MakeArrayView<FOnDemandChunkEntry>(
			reinterpret_cast<FOnDemandChunkEntry*>(Container->ChunkIds.GetData() + EntryCount), EntryCount);

		for (int32 EntryIndex = 0; EntryIndex < EntryCount; EntryIndex++)
		{
			const FOnDemandTocEntry& TocEntry = ContainerEntry.Entries[EntryIndex];

			Container->ChunkIds[EntryIndex]		= TocEntry.ChunkId;
			Container->ChunkEntries[EntryIndex]	= FOnDemandChunkEntry
			{
				.Hash					= TocEntry.Hash,
				.RawSize				= uint32(TocEntry.RawSize),
				.EncodedSize			= uint32(TocEntry.EncodedSize),
				.BlockOffset			= TocEntry.BlockOffset,
				.BlockCount				= TocEntry.BlockCount,
				.CompressionFormatIndex	= 0
			};
		}

		const uint32 ContainerIndex = Out.Num();
		for (FOnDemandTocTagSet& TagSet : Toc.TagSets)
		{
			for (FOnDemandTocTagSetPackageList& ContainerPackageIndices : TagSet.Packages)
			{
				if (ContainerPackageIndices.ContainerIndex == ContainerIndex)
				{
					FOnDemandTagSet& NewTagSet = Container->TagSets.AddDefaulted_GetRef();
					NewTagSet.Tag = TagSet.Tag;
					NewTagSet.PackageIndicies = MoveTemp(ContainerPackageIndices.PackageIndicies);
					break;
				}
			}
		}

		Out.Add(MoveTemp(Container));
	}
}

TArray<FSharedOnDemandContainer> FOnDemandIoStore::GetContainers()
{
	UE::TUniqueLock Lock(ContainerMutex);
	return Containers;
}

TArray<FSharedOnDemandContainer> FOnDemandIoStore::GetContainers(TFunctionRef<bool(const FSharedOnDemandContainer& Container)> Predicate)
{
	TArray<FSharedOnDemandContainer> Ret;

	UE::TUniqueLock Lock(ContainerMutex);
	Algo::CopyIf(Containers, Ret, Predicate);

	return Ret;
}

void FOnDemandIoStore::ReleaseContent(FOnDemandInternalContentHandle& ContentHandle)
{
	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Releasing content handle '%s'"), *LexToString(ContentHandle));

	UE::TUniqueLock Lock(ContainerMutex);
	for (FSharedOnDemandContainer& Container : Containers)
	{
		if (EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::StreamOnDemand))
		{
			continue;
		}

		Container->ChunkEntryReferences.SetNum(
			Algo::RemoveIf(
				Container->ChunkEntryReferences,
				[ContentHandleId = ContentHandle.HandleId()](const FOnDemandChunkEntryReferences& Refs)
				{ 
					return Refs.ContentHandleId == ContentHandleId;
				}));
	}
}

void FOnDemandIoStore::GetReferencedContent(TArray<FSharedOnDemandContainer>& OutContainers, TArray<TBitArray<>>& OutChunkEntryIndices)
{
	UE::TUniqueLock Lock(ContainerMutex);
	for (FSharedOnDemandContainer& Container : Containers)
	{
		if (EnumHasAnyFlags(Container->Flags, EOnDemandContainerFlags::StreamOnDemand))
		{
			continue;
		}

		TBitArray<> Indices = Container->GetReferencedChunkEntries();
		if (Indices.IsEmpty() == false)
		{
			OutContainers.Add(Container);
			OutChunkEntryIndices.Add(MoveTemp(Indices));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
FOnDemandInternalContentHandle::~FOnDemandInternalContentHandle()
{
	if (TSharedPtr<FOnDemandIoStore, ESPMode::ThreadSafe> Pinned = IoStore.Pin(); Pinned.IsValid())
	{
		Pinned->ReleaseContent(*this);
	}
}

FString LexToString(const FOnDemandInternalContentHandle& Handle)
{
	return FString::Printf(TEXT("0x%llX (%s)"), Handle.HandleId(), *Handle.DebugName);
}

///////////////////////////////////////////////////////////////////////////////
FOnDemandContentHandle::FOnDemandContentHandle()
{
}

FOnDemandContentHandle::~FOnDemandContentHandle()
{
}

FOnDemandContentHandle FOnDemandContentHandle::Create()
{
	FOnDemandContentHandle NewHandle;
	NewHandle.Handle = MakeShared<FOnDemandInternalContentHandle, ESPMode::ThreadSafe>();
	return NewHandle;
}

FOnDemandContentHandle FOnDemandContentHandle::Create(FSharedString DebugName)
{
	FOnDemandContentHandle NewHandle;
	NewHandle.Handle = MakeShared<FOnDemandInternalContentHandle, ESPMode::ThreadSafe>(DebugName);
	return NewHandle;
}

FOnDemandContentHandle FOnDemandContentHandle::Create(FStringView DebugName)
{
	return Create(FSharedString(DebugName));
}

FString LexToString(const FOnDemandContentHandle& Handle)
{
	return Handle.IsValid() ? LexToString(*Handle.Handle) : TEXT("Invalid");
}

} // namespace UE::IoStore
