// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(UE_WITH_IAS_TOOL)

#include "Command.h"

#include "Algo/Find.h"
#include "Containers/StringView.h"
#include "HAL/FileManager.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoStatus.h"
#include "IO/IoStore.h"
#include "IO/IoStoreOnDemand.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/StringBuilder.h"
#include "Serialization/LargeMemoryWriter.h"

namespace UE::IoStore::Tool
{

////////////////////////////////////////////////////////////////////////////////
FIoStatus WriteChunk(const FString& Directory, FMemoryView Chunk, const FIoHash& Hash)
{
	IFileManager& FileMgr = IFileManager::Get();
	const FString HashString = LexToString(Hash);

	TStringBuilder<256> Sb;
	Sb << Directory << TEXT("/") << HashString.Left(2);

	bool bTree = true;
	if (FileMgr.MakeDirectory(Sb.ToString(), bTree) == false)
	{
		return FIoStatusBuilder(EIoErrorCode::WriteError)
			<< TEXT("Failed to create directory '")
			<< FString(Sb.ToString())
			<< TEXT("'");
	}

	Sb << TEXT("/") << HashString << TEXT(".iochunk");

	if (TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(Sb.ToString())); Ar.IsValid())
	{
		UE_LOG(LogIoStore, Display, TEXT("Writing file '%s' (%.2lf KiB)"),
			Sb.ToString(), double(Chunk.GetSize()) / 1024.0);
		Ar->Serialize((void*)Chunk.GetData(), Chunk.GetSize());

		return EIoErrorCode::Ok;
	}

	return FIoStatusBuilder(EIoErrorCode::WriteError)
		<< TEXT("Failed to write file '")
		<< FString(Sb.ToString())
		<< TEXT("'");
}

////////////////////////////////////////////////////////////////////////////////
FIoStatus WriteChunk(const FString& Directory, FMemoryView Chunk)
{
	return WriteChunk(Directory, Chunk, FIoHash::HashBuffer(Chunk));
}

////////////////////////////////////////////////////////////////////////////////
static int32 ChunkPluginCommandEntry(const FContext& Context)
{
	const FString Platform				= FString(Context.Get<FStringView>(TEXT("-Platform"), FString()));
	const FString BuildVersion			= FString(Context.Get<FStringView>(TEXT("-BuildVersion"), FString()));
	const FString OnDemandTocName		= FString(Context.Get<FStringView>(TEXT("-OnDemandTocName"), FString()));
	const FString InputFolder			= FString(Context.Get<FStringView>(TEXT("-InputFolder"), FString()));
	const FString OutputFolder			= FString(Context.Get<FStringView>(TEXT("-OutputFolder"), FString()));
	const FString IntermediateFolder	= FString(Context.Get<FStringView>(TEXT("-IntermediateFolder"), FString()));
	const bool bIncludeSigPak			= Context.Get<bool>(TEXT("-IncludeSigPak"), false);
	const bool bDeleteContainerFiles	= !Context.Get<bool>(TEXT("-KeepContainerFiles"), false);
	FString ContainerFolder				= InputFolder;
	FString IoStoreOutputFolder			= OutputFolder / TEXT("iostore");
	FString ChunksOutputFolder			= IoStoreOutputFolder / TEXT("chunks");

	FPaths::NormalizeDirectoryName(ContainerFolder);
	FPaths::NormalizeDirectoryName(IoStoreOutputFolder);
	FPaths::NormalizeDirectoryName(ChunksOutputFolder);

	UE_LOG(LogIoStore, Display, TEXT("I/O store chunk plugin:"));
	UE_LOG(LogIoStore, Display, TEXT("----------------------------------------"));
	UE_LOG(LogIoStore, Display, TEXT("\tBuildVersion: %s"), *BuildVersion);
	UE_LOG(LogIoStore, Display, TEXT("\tPlatform: %s"), *Platform);
	UE_LOG(LogIoStore, Display, TEXT("\tOnDemandTocName: %s"), *OnDemandTocName);
	UE_LOG(LogIoStore, Display, TEXT("\tInputFolder: %s"), *InputFolder);
	UE_LOG(LogIoStore, Display, TEXT("\tOutputFolder: %s"), *OutputFolder);
	UE_LOG(LogIoStore, Display, TEXT("\tIntermediateFolder: %s"), *IntermediateFolder);
	UE_LOG(LogIoStore, Display, TEXT("\tIncludeSigPak: %s"), bIncludeSigPak ? TEXT("true") : TEXT("false"));
	UE_LOG(LogIoStore, Display, TEXT("\tDeleteContainerFiles: %s"), bDeleteContainerFiles ? TEXT("true") : TEXT("false"));

	IFileManager& FileMgr = IFileManager::Get();
	if (FileMgr.MakeDirectory(*ChunksOutputFolder, true) == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to create directory '%s'"), *ChunksOutputFolder);
		return -1;
	}

	TMap<FGuid, FAES::FAESKey> EncryptionKeys;
	if (IFileManager::Get().DirectoryExists(*ContainerFolder) == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Directory '%s' does not exist"), *ContainerFolder);
		return -1;
	}

	TArray<FString> ContainerFilenames;
	FileMgr.FindFiles(ContainerFilenames, *ContainerFolder, TEXT("*.utoc"));
	UE_LOG(LogIoStore, Display, TEXT("Found %d container files(s)"), ContainerFilenames.Num());

	FOnDemandToc OnDemandToc;
	//OnDemandToc.Header.ChunksDirectory = TODO 
	OnDemandToc.Containers.Reserve(ContainerFilenames.Num());

	TArray<FString> FilesToDelete;

	for (const FString& Filename : ContainerFilenames)
	{
		const FString FullPath = ContainerFolder / Filename;
		FIoStoreReader ContainerFileReader;
		{
			FIoStatus Status = ContainerFileReader.Initialize(*FPaths::ChangeExtension(FullPath, TEXT("")), EncryptionKeys);
			if (!Status.IsOk())
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to open container '%s' for reading"), *FullPath);
				continue;
			}
		}

		UE_LOG(LogIoStore, Display, TEXT("Processing container '%s'"), *FullPath);

		const uint32 BlockSize = ContainerFileReader.GetCompressionBlockSize();
		if (OnDemandToc.Header.BlockSize == 0)
		{
			OnDemandToc.Header.BlockSize = ContainerFileReader.GetCompressionBlockSize();
		}
		check(OnDemandToc.Header.BlockSize == ContainerFileReader.GetCompressionBlockSize());

		TArray<FIoStoreTocChunkInfo> ChunkInfos;
		ContainerFileReader.EnumerateChunks([&ChunkInfos](FIoStoreTocChunkInfo&& Info)
		{ 
			ChunkInfos.Emplace(MoveTemp(Info));
			return true;
		});

		UE_LOG(LogIoStore, Display, TEXT("Serializing %d chunks"), ChunkInfos.Num());
		FOnDemandTocContainerEntry& ContainerEntry = OnDemandToc.Containers.AddDefaulted_GetRef();
		ContainerEntry.ContainerId		= ContainerFileReader.GetContainerId();
		ContainerEntry.ContainerName	= FPaths::GetBaseFilename(FullPath);

		if (EnumHasAnyFlags(ContainerFileReader.GetContainerFlags(), EIoContainerFlags::Encrypted))
		{
			ContainerEntry.EncryptionKeyGuid = LexToString(ContainerFileReader.GetEncryptionKeyGuid());
		}

		for (const FIoStoreTocChunkInfo& ChunkInfo : ChunkInfos)
		{
			const bool bDecrypt = false;
			TIoStatusOr<FIoStoreCompressedReadResult> Status = ContainerFileReader.ReadCompressed(ChunkInfo.Id, FIoReadOptions(), bDecrypt);
			if (!Status.IsOk())
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to read container chunk, Container='%s', Reason='%s'"),
					*FullPath, *Status.Status().ToString());
				return -1;
			}

			FIoStoreCompressedReadResult ReadResult = Status.ConsumeValueOrDie();

			const uint32 BlockOffset	= ContainerEntry.BlockSizes.Num();
			const uint32 BlockCount		= ReadResult.Blocks.Num();
			const FIoHash ChunkHash		= FIoHash::HashBuffer(ReadResult.IoBuffer.GetView());
			const FString HashString	= LexToString(ChunkHash);

			FMemoryView EncodedBlocks = ReadResult.IoBuffer.GetView();
			uint64 RawChunkSize = 0;
			uint64 EncodedChunkSize = 0;
			for (const FIoStoreCompressedBlockInfo& BlockInfo : ReadResult.Blocks)
			{
				check(Align(BlockInfo.CompressedSize, FAES::AESBlockSize) == BlockInfo.AlignedSize);
				const uint64 EncodedBlockSize = BlockInfo.AlignedSize;
				ContainerEntry.BlockSizes.Add(uint32(BlockInfo.CompressedSize));

				FMemoryView EncodedBlock = EncodedBlocks.Left(EncodedBlockSize);
				EncodedBlocks += EncodedBlock.GetSize();
				ContainerEntry.BlockHashes.Add(FIoChunkEncoding::HashBlock(EncodedBlock));

				EncodedChunkSize += EncodedBlockSize;
				RawChunkSize += BlockInfo.UncompressedSize;

				if (OnDemandToc.Header.CompressionFormat.IsEmpty() && BlockInfo.CompressionMethod != NAME_None)
				{
					OnDemandToc.Header.CompressionFormat = BlockInfo.CompressionMethod.ToString();
				}
			}

			if (EncodedChunkSize != ReadResult.IoBuffer.GetSize())
			{
				UE_LOG(LogIoStore, Error, TEXT("Chunk size mismatch, Container='%s', ChunkId='%s'"),
					*FullPath, *LexToString(ChunkInfo.Id));
				return -1;
			}

			if (FIoStatus WriteStatus = WriteChunk(ChunksOutputFolder, ReadResult.IoBuffer.GetView(), ChunkHash);
				WriteStatus.IsOk() == false)
			{
				UE_LOG(LogIoStore, Error, TEXT("%s"), *WriteStatus.ToString());
				return -1;
			}

			FOnDemandTocEntry& TocEntry = ContainerEntry.Entries.AddDefaulted_GetRef();
			TocEntry.ChunkId = ChunkInfo.Id;
			TocEntry.Hash = ChunkHash;
			TocEntry.RawSize = RawChunkSize;
			TocEntry.EncodedSize = EncodedChunkSize;
			TocEntry.BlockOffset = BlockOffset;
			TocEntry.BlockCount = BlockCount;
		}

		if (bDeleteContainerFiles)
		{
			FilesToDelete.Add(FullPath);
			ContainerFileReader.GetContainerFilePaths(FilesToDelete);
		}
	}


	IFileManager& FileMan = IFileManager::Get();
	for (const FString& Path : FilesToDelete)
	{
		//UE_LOG(LogIas, Display, TEXT("Attempt Deleting '%s'"), *Path);
		if (FileMan.FileExists(*Path))
		{
			UE_LOG(LogIas, Display, TEXT("Deleting '%s'"), *Path);
			if (!FileMan.Delete(*Path, /*RequireExists*/true))
			{
				UE_LOG(LogIas, Error, TEXT("Failed to delete '%s'"), *Path);
			}
		}
	}

	// Write additional file(s)
	if (bIncludeSigPak)
	{
		const FStringView AllowedExt[]
		{
			TEXTVIEW("pak"),
			TEXTVIEW("sig"),
		};

		TStringBuilder<256> Sb;
		TArray<FString> AdditionalFiles;
		FileMgr.FindFiles(AdditionalFiles, *ContainerFolder, TEXT("*.*"));

		UE_LOG(LogIoStore, Display, TEXT("Serializing %d additional file(s)"), AdditionalFiles.Num());
		for (const FString& Filename : AdditionalFiles)
		{
			const bool bIncludeDot = false;
			const FStringView Ext = FPathViews::GetExtension(Filename, bIncludeDot);

			if (const FStringView* Result =
				Algo::FindByPredicate(AllowedExt, [&Ext](const FStringView& E) { return Ext == E; });
				Result == nullptr)
			{
				continue;
			}

			const FString FullPath = ContainerFolder / Filename;
			TArray<uint8> FileData;
			if (!FFileHelper::LoadFileToArray(FileData, *FullPath))
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed reading file '%s'"), *FullPath);
				return -1;
			}

			FMemoryView Chunk = MakeMemoryView(FileData.GetData(), FileData.Num());
			const FIoHash ChunkHash = FIoHash::HashBuffer(Chunk);
			if (FIoStatus WriteStatus = WriteChunk(ChunksOutputFolder, Chunk, ChunkHash);
				WriteStatus.IsOk() == false)
			{
				UE_LOG(LogIoStore, Error, TEXT("%s"), *WriteStatus.ToString());
				return -1;
			}

			UE_LOG(LogIoStore, Display, TEXT("Adding additional file '%s'"), *Filename);
			OnDemandToc.AdditionalFiles.Add(FOnDemandTocAdditionalFile
			{
				.Hash = ChunkHash,
				.Filename = Filename,
				.FileSize = IntCastChecked<uint64>(FileData.Num())
			});
		}
	}

	OnDemandToc.Meta.EpochTimestamp = FDateTime::UtcNow().ToUnixTimestamp();
	OnDemandToc.Meta.TargetPlatform = Platform;
	OnDemandToc.Meta.BuildVersion	= BuildVersion;
	{
		FString Filename = FPathViews::SetExtension(OnDemandTocName, TEXT(".uondemandtoc"));
		Filename.ToLowerInline();

		const FString TocPath = IoStoreOutputFolder / Filename;
		if (TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*TocPath)); Ar.IsValid())
		{
			*Ar << OnDemandToc;
			Ar->Close();
			if (Ar->IsError())
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to serialize TOC '%s'"), *TocPath);
				return -1;
			}
			else
			{
				const int64 TocSize = Ar->Tell();
				UE_LOG(LogIoStore, Display, TEXT("Writing file '%s' (%.2lf KiB)"),
					*TocPath, double(TocSize) / 1024);
			}
		}
		else
		{
			UE_LOG(LogIoStore, Display, TEXT("Failed writing file '%s'"), *TocPath );
			return -1;
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static FCommand ChunkPluginCommand(
	ChunkPluginCommandEntry,
	TEXT("ChunkPlugin"),
	TEXT(""),
	{
		TArgument<FStringView>(TEXT("-Platform"),			TEXT("Platform name.")),
		TArgument<FStringView>(TEXT("-BuildVersion"),		TEXT("Build version")),
		TArgument<FStringView>(TEXT("-OnDemandTocName"),	TEXT("On Demand TOC Name")),
		TArgument<FStringView>(TEXT("-InputFolder"),		TEXT("Input folder to plugin information.")),
		TArgument<FStringView>(TEXT("-OutputFolder"),		TEXT("Ouptut folder.")),
		TArgument<FStringView>(TEXT("-IntermediateFolder"),	TEXT("Intermediate folder.")),
		TArgument<FStringView>(TEXT("-ErrorOutput"),		TEXT("Error output.")),
		TArgument<bool>(TEXT("-IncludeSigPak"),				TEXT("Include .sig and .pak file in the uondemandtoc")),
		TArgument<bool>(TEXT("-KeepContainerFiles"),		TEXT("Should we keep the container files after processing them.")),
	}
);

} // namespace UE::IoStore::Tool

#endif // UE_WITH_IAS_TOOL
