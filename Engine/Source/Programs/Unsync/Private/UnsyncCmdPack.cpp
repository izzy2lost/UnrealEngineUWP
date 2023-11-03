// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCmdPack.h"
#include "UnsyncCompression.h"
#include "UnsyncFile.h"
#include "UnsyncHashTable.h"
#include "UnsyncSerialization.h"
#include "UnsyncThread.h"

#include <stdio.h>
#include <stdlib.h>
#include <atomic>

namespace unsync {

template<typename CallbackT>
static void
ForLines(std::string_view String, CallbackT Callback)
{
	while (!String.empty())
	{
		size_t LineEndPos = String.find('\n');
		if (LineEndPos == std::string::npos)
		{
			LineEndPos = String.length();
		}

		std::string_view LineView = String.substr(0, LineEndPos);

		if (LineView.ends_with('\r'))
		{
			LineView = LineView.substr(0, LineView.length() - 1);
		}

		Callback(LineView);

		String = String.substr(LineEndPos + 1);
	}
}

static void
BuildP4HaveSet(const FPath& Root, std::string_view P4HaveDataUtf8, FDirectoryManifest::FFileMap& Result)
{
	auto Callback = [&Result, &Root](std::string_view LineView)
	{
		if (LineView.starts_with("---"))  // p4 diagnostic data
		{
			return;
		}

		size_t HashPos = LineView.find('#');
		if (HashPos == std::string::npos)
		{
			return;
		}

		size_t SplitPos = LineView.find(" - ", HashPos);
		if (SplitPos == std::string::npos)
		{
			return;
		}

		std::string_view DepotPathUtf8 = LineView.substr(0, SplitPos);
		std::string_view LocalPathUtf8 = LineView.substr(SplitPos + 3);

		FPath LocalPath(ConvertUtf8ToWide(LocalPathUtf8));

		FPath RelativePath = GetRelativePath(LocalPath, Root);
		if (RelativePath.empty())
		{
			return;
		}

		FFileManifest FileManifest;
		FileManifest.CurrentPath			 = std::move(LocalPath);
		FileManifest.RevisionControlIdentity = std::move(DepotPathUtf8);

		std::wstring RelativePathStr = RelativePath.wstring();
		Result.insert(std::make_pair(std::move(RelativePathStr), FileManifest));
	};

	ForLines(P4HaveDataUtf8, Callback);
}

struct FPackIndexEntry
{
	FHash128 BlockHash		= {};
	FHash128 CompressedHash = {};
	uint32	 Offset			= 0;
	uint32	 CompressedSize = 0;
};
static_assert(sizeof(FPackIndexEntry) == 40);

inline void
AddHash(uint64* Accumulator, const FHash128& Hash)
{
	uint64 BlockHashParts[2];
	memcpy(BlockHashParts, &Hash, sizeof(FHash128));
	Accumulator[0] += BlockHashParts[0];
	Accumulator[1] += BlockHashParts[1];
}

inline FHash128
MakeHashFromParts(uint64* Parts)
{
	FHash128 Result;
	memcpy(&Result, Parts, sizeof(Result));
	return Result;
}

struct FPackWriteContext
{
	static constexpr uint64 MaxPackFileSize = 1_GB;

	FPackWriteContext(const FPath& InOutputRoot) : OutputRoot(InOutputRoot) { Reset(); }
	~FPackWriteContext() { FinishPack(); }

	void AddBlock(const FGenericBlock& Block, FHash128 CompressedHash, FBufferView CompressedData)
	{
		std::lock_guard<std::mutex> LockGuard(Mutex);

		UNSYNC_ASSERT(CompressedData.Size <= MaxPackFileSize);

		if (PackBuffer.Size() + CompressedData.Size > MaxPackFileSize)
		{
			FinishPack();
		}

		FPackIndexEntry IndexEntry;
		IndexEntry.BlockHash	  = Block.HashStrong.ToHash128();
		IndexEntry.CompressedHash = CompressedHash;
		IndexEntry.Offset		  = CheckedNarrow(PackBuffer.Size());
		IndexEntry.CompressedSize = CheckedNarrow(CompressedData.Size);

		IndexEntries.push_back(IndexEntry);
		PackBuffer.Append(CompressedData);

		AddHash(IndexFileHashSum, IndexEntry.BlockHash);
	}

	void FinishPack()
	{
		if (IndexEntries.empty())
		{
			return;
		}

		FHash128	BlockHash128 = MakeHashFromParts(IndexFileHashSum);
		std::string OutputId	 = HashToHexString(BlockHash128);

		FPath FinalPackFilename	 = OutputRoot / (OutputId + ".unsync_pack");
		FPath FinalIndexFilename = OutputRoot / (OutputId + ".unsync_index");

		UNSYNC_LOG(L"Saving new pack: %hs", OutputId.c_str());

		if (!WriteBufferToFile(FinalPackFilename, PackBuffer, EFileMode::CreateWriteOnly))
		{
			UNSYNC_FATAL(L"Failed to write pack file '%ls'", FinalPackFilename.wstring().c_str());
		}

		const uint8* IndexData	   = reinterpret_cast<const uint8*>(IndexEntries.data());
		uint64		 IndexDataSize = sizeof(IndexEntries[0]) * IndexEntries.size();
		if (!WriteBufferToFile(FinalIndexFilename, IndexData, IndexDataSize, EFileMode::CreateWriteOnly))
		{
			UNSYNC_FATAL(L"Failed to write index file '%ls'", FinalIndexFilename.wstring().c_str());
		}

		Reset();
	}

private:
	void Reset()
	{
		PackBuffer.Reserve(MaxPackFileSize);
		PackBuffer.Clear();
		IndexEntries.clear();

		IndexFileHashSum[0] = 0;
		IndexFileHashSum[1] = 0;
	}

	std::mutex Mutex;

	// Independent sums of low and high 32 bits of all seen block hashes.
	// Used to generate a stable hash while allowing out-of-order block processing.
	uint64 IndexFileHashSum[2] = {};

	FBuffer						 PackBuffer;
	std::vector<FPackIndexEntry> IndexEntries;

	FPath OutputRoot;
};

static bool
EnsureDirectoryExists(const FPath& Path)
{
	return (PathExists(Path) && IsDirectory(Path)) || CreateDirectories(Path);
}

static int32
RunSubprocess(const char* Command, const FPath& WorkingDirectory, std::string& StdOutBuffer)
{
#if UNSYNC_PLATFORM_WINDOWS

	char TempBuffer[65536];

	wchar_t* PrevWD = _wgetcwd(nullptr, 0);

	if (!PrevWD)
	{
		UNSYNC_ERROR(L"Failed to get current working directory")
		return -1;
	}

	int32 ErrorCode = _wchdir(WorkingDirectory.native().c_str());

	FILE* Pipe = _popen(Command, "rt");
	if (Pipe)
	{
		for (;;)
		{
			size_t ReadSize = fread(TempBuffer, 1, sizeof(TempBuffer), Pipe);
			if (ReadSize != 0)
			{
				StdOutBuffer.append(TempBuffer, ReadSize);
			}

			if (feof(Pipe))
			{
				break;
			}
		}

		ErrorCode = _pclose(Pipe);
	}
	else
	{
		ErrorCode = errno;
	}

	_wchdir(PrevWD);
	free(PrevWD);

	return ErrorCode;

#else  // UNSYNC_PLATFORM_WINDOWS

	UNSYNC_FATAL(L"RunSubprocess() is not implemented");
	return -1;

#endif	// UNSYNC_PLATFORM_WINDOWS
}

int32
CmdPack(const FCmdPackOptions& Options)
{
	const FFileAttributes RootAttrib = GetFileAttrib(Options.RootPath);

	const FPath InputRoot	 = Options.RootPath;
	const FPath ManifestRoot = InputRoot / ".unsync";
	const FPath StoreRoot	 = Options.StorePath.empty() ? ManifestRoot : Options.StorePath;
	const FPath PackRoot	 = StoreRoot / "pack";

	UNSYNC_LOG(L"Generating package for directory '%ls'", InputRoot.wstring().c_str());
	UNSYNC_LOG_INDENT;

	if (!RootAttrib.bValid)
	{
		UNSYNC_ERROR(L"Input directory '%ls' does not exist", InputRoot.wstring().c_str());
		return -1;
	}

	if (!RootAttrib.bDirectory)
	{
		UNSYNC_ERROR(L"Input '%ls' is not a directory", InputRoot.wstring().c_str());
		return -1;
	}

	std::string P4HaveBuffer;

	if (Options.bRunP4Have)
	{
		UNSYNC_LOG(L"Runinng `p4 have`");
		int32 ReturnCode = RunSubprocess("p4 have ...", InputRoot, P4HaveBuffer);
		if (ReturnCode != 0)
		{
			UNSYNC_ERROR("Error reported while running `p4 have`: %d", ReturnCode);
			return -1;
		}
	}
	else if (!Options.P4HavePath.empty())
	{
		UNSYNC_LOG(L"Loading p4 manifest file '%ls'", Options.P4HavePath.wstring().c_str());

		FNativeFile P4HaveFile(Options.P4HavePath, EFileMode::ReadOnly);
		if (!P4HaveFile.IsValid())
		{
			UNSYNC_ERROR(L"Could not open p4 manifest file '%ls'", Options.P4HavePath.wstring().c_str());
			return -1;
		}

		P4HaveBuffer.resize(P4HaveFile.GetSize());
		uint64 ReadBytes = P4HaveFile.Read(P4HaveBuffer.data(), 0, P4HaveFile.GetSize());
		if (ReadBytes != P4HaveFile.GetSize())
		{
			UNSYNC_ERROR(L"Could not read the entire p4 manifest from '%ls'", Options.P4HavePath.wstring().c_str());
			return -1;
		}
	}

	// TODO: allow explicit output path
	{
		if (!EnsureDirectoryExists(ManifestRoot))
		{
			UNSYNC_ERROR(L"Failed to create manifest output directory '%ls'", ManifestRoot.wstring().c_str());
			return -1;
		}

		if (!EnsureDirectoryExists(PackRoot))
		{
			UNSYNC_ERROR(L"Failed to create pack output directory '%ls'", PackRoot.wstring().c_str());
			return -1;
		}
	}

	THashSet<FHash128> SeenBlockHashSet;
	UNSYNC_LOG(L"Loading block database");
	{
		UNSYNC_LOG_INDENT;
		FPath ExpectedExtension = FPath(".unsync_index");
		for (const std::filesystem::directory_entry& Dir : RecursiveDirectoryScan(PackRoot))
		{
			if (!Dir.is_regular_file())
			{
				continue;
			}

			const FPath&	FilePath = Dir.path();
			FPathStringView FilePathView(FilePath.native());
			if (!FilePathView.ends_with(ExpectedExtension.native()))
			{
				continue;
			}

			FBuffer ExistingEntries = ReadFileToBuffer(FilePath);
			uint64	NumEntries		= ExistingEntries.Size() / sizeof(FPackIndexEntry);
			for (const FPackIndexEntry& Entry : MakeView(reinterpret_cast<FPackIndexEntry*>(ExistingEntries.Data()), NumEntries))
			{
				SeenBlockHashSet.insert(Entry.BlockHash);
			}
		}
	}

	const FPath DirectoryManifestPath = ManifestRoot / "manifest.bin";

	FDirectoryManifest DirectoryManifest;
	FDirectoryManifest OldDirectoryManifest;

	if (PathExists(DirectoryManifestPath))
	{
		UNSYNC_LOG(L"Loading previous manifest ");
		UNSYNC_LOG_INDENT;

		if (LoadDirectoryManifest(OldDirectoryManifest, InputRoot, DirectoryManifestPath))
		{
			if (OldDirectoryManifest.bHasFileRevisionControl)
			{
				UNSYNC_LOG(L"Loaded existing manifest with revision control data");
			}
			else
			{
				UNSYNC_LOG(L"Loaded existing manifest without revision control data");
			}
		}
	}

	if (!P4HaveBuffer.empty())
	{
		UNSYNC_LOG(L"Processing revision control data");

		DirectoryManifest.Algorithm = Options.Algorithm;

		BuildP4HaveSet(InputRoot, P4HaveBuffer, DirectoryManifest.Files);
		DirectoryManifest.bHasFileRevisionControl = true;

		UNSYNC_LOG(L"Loaded entries from p4 manifest: %llu", llu(DirectoryManifest.Files.size()));

		UNSYNC_LOG(L"Updating file attributes");
		auto UpdateFileMetadata = [&OldDirectoryManifest](std::pair<const std::wstring, FFileManifest>& It)
		{
			if (OldDirectoryManifest.bHasFileRevisionControl)
			{
				auto OldFileIt = OldDirectoryManifest.Files.find(It.first);
				if (OldFileIt != OldDirectoryManifest.Files.end())
				{
					if (It.second.RevisionControlIdentity == OldFileIt->second.RevisionControlIdentity)
					{
						It.second.Mtime		= OldFileIt->second.Mtime;
						It.second.Size		= OldFileIt->second.Size;
						It.second.bReadOnly = OldFileIt->second.bReadOnly;
					}
				}
			}

			if (!It.second.IsValid())
			{
				FFileAttributes Attrib = GetFileAttrib(It.second.CurrentPath);

				It.second.Mtime		= Attrib.Mtime;
				It.second.Size		= Attrib.Size;
				It.second.bReadOnly = true;	 // treat all p4 files as read-only in the manifest
			}
		};
		ParallelForEach(DirectoryManifest.Files, UpdateFileMetadata);
	}
	else
	{
		// create a lightweight manifest, without blocks
		FComputeBlocksParams LightweightManifestParams;
		LightweightManifestParams.Algorithm	  = Options.Algorithm;
		LightweightManifestParams.bNeedBlocks = false;
		LightweightManifestParams.BlockSize	  = 0;

		DirectoryManifest = CreateDirectoryManifest(InputRoot, LightweightManifestParams);
	}

	UNSYNC_LOG(L"Found files: %llu", llu(DirectoryManifest.Files.size()));

	std::atomic<uint64> ProcessedRawBytes;
	std::atomic<uint64> CompressedBytes;

	FPackWriteContext PackWriter(PackRoot);

	FThreadLogConfig LogConfig;

	std::mutex Mutex;

	FOnBlockGenerated OnBlockGenerated =
		[&Mutex, &PackWriter, &SeenBlockHashSet, &ProcessedRawBytes, &CompressedBytes, &LogConfig](const FGenericBlock& Block,
																								   FBufferView			Data)
	{
		FThreadLogConfig::FScope LogConfigScope(LogConfig);

		{
			std::lock_guard<std::mutex> LockGuard(Mutex);
			if (!SeenBlockHashSet.insert(Block.HashStrong.ToHash128()).second)
			{
				return;
			}
		}

		const uint64 MaxCompressedSize	  = GetMaxCompressedSize(Block.Size);
		FIOBuffer	 CompressedData		  = FIOBuffer::Alloc(MaxCompressedSize, L"PackBlock");
		uint64		 ActualCompressedSize = CompressInto(Data, CompressedData.GetMutBufferView(), 9);

		if (!ActualCompressedSize)
		{
			UNSYNC_FATAL(L"Failed to compress file block");
		}
		CompressedData.SetDataRange(0, ActualCompressedSize);

		ProcessedRawBytes += Block.Size;
		CompressedBytes += ActualCompressedSize;

		FHash128 CompressedHash = HashBlake3Bytes<FHash128>(CompressedData.GetData(), ActualCompressedSize);

		PackWriter.AddBlock(Block, CompressedHash, CompressedData.GetBufferView());
	};

	FComputeBlocksParams BlockParams;
	BlockParams.Algorithm = Options.Algorithm;
	BlockParams.BlockSize = Options.BlockSize;

	// TODO: threading makes pack files non-deterministic...
	// Perhaps a strictly ordered parallel pipeline mechanism could be implemented?
	BlockParams.bAllowThreading	 = true;
	BlockParams.OnBlockGenerated = OnBlockGenerated;

	if (OldDirectoryManifest.IsValid())
	{
		// Copy file blocks from old manifest, if possible
		if (AlgorithmOptionsCompatible(DirectoryManifest.Algorithm, OldDirectoryManifest.Algorithm))
		{
			MoveCompatibleManifestBlocks(DirectoryManifest, std::move(OldDirectoryManifest));
		}
		else
		{
			UNSYNC_LOG(L"Incremental file block generation is not possible due to algorithm options mismatch");
		}
	}

	UNSYNC_LOG(L"Computing file blocks");
	UpdateDirectoryManifestBlocks(DirectoryManifest, InputRoot, BlockParams);

	uint64 ManifestUniqueBytes	   = 0;
	uint64 ManifestCompressedBytes = 0;

	UNSYNC_LOG(L"Saving directory manifest");

	{
		FBuffer			 ManifestBuffer;
		FVectorStreamOut ManifestStream(ManifestBuffer);
		bool			 bManifestSerialized = SaveDirectoryManifest(DirectoryManifest, ManifestStream);
		if (!bManifestSerialized)
		{
			UNSYNC_FATAL(L"Failed to serialize directory manifest to memory");
			return -1;
		}

		std::vector<FGenericBlock> ManifestBlocks;
		FOnBlockGenerated		   OnManifestBlockGenerated =
			[&OnBlockGenerated, &Mutex, &ManifestBlocks](const FGenericBlock& Block, FBufferView Data)
		{
			{
				std::lock_guard<std::mutex> LockGuard(Mutex);
				ManifestBlocks.push_back(Block);
			}
			OnBlockGenerated(Block, Data);
		};

		ManifestUniqueBytes -= ProcessedRawBytes.load();
		ManifestCompressedBytes -= CompressedBytes.load();

		BlockParams.OnBlockGenerated = OnManifestBlockGenerated;
		FMemReader ManifestDataReader(ManifestBuffer);
		ComputeBlocks(ManifestDataReader, BlockParams);

		if (!WriteBufferToFile(DirectoryManifestPath, ManifestBuffer))
		{
			UNSYNC_FATAL(L"Failed to save directory manifest to file '%ls'", DirectoryManifestPath.wstring().c_str());
			return 1;
		}

		std::sort(ManifestBlocks.begin(), ManifestBlocks.end(), FGenericBlock::FCompareByOffset());

		for (const FGenericBlock& Block : ManifestBlocks)
		{
			UNSYNC_ASSERT(SeenBlockHashSet.find(Block.HashStrong.ToHash128()) != SeenBlockHashSet.end());
		}

		FBufferView ManifestBlocksBuffer;
		ManifestBlocksBuffer.Data = reinterpret_cast<const uint8*>(ManifestBlocks.data());
		ManifestBlocksBuffer.Size = sizeof(ManifestBlocks[0]) * ManifestBlocks.size();

		FHash128 ManifestBlocksBufferHash = HashBlake3Bytes<FHash128>(ManifestBlocksBuffer.Data, ManifestBlocksBuffer.Size);

		std::string SnapshotId	 = HashToHexString(ManifestBlocksBufferHash);  // TODO: allow overriding this from command line
		FPath		SnapshotPath = StoreRoot / (SnapshotId + ".unsync_snapshot");

		UNSYNC_LOG(L"Writing snapshot: %hs", SnapshotId.c_str());

		bool bSnapshotWritten = WriteBufferToFile(SnapshotPath, ManifestBlocksBuffer.Data, ManifestBlocksBuffer.Size);
		if (!bSnapshotWritten)
		{
			UNSYNC_FATAL(L"Failed to write snapthot file '%ls'", SnapshotPath.wstring().c_str());
		}

		ManifestUniqueBytes += ProcessedRawBytes.load();
		ManifestCompressedBytes += CompressedBytes.load();
	}

	PackWriter.FinishPack();

	uint64 SourceSize = 0;
	for (const auto& It : DirectoryManifest.Files)
	{
		SourceSize += It.second.Size;
	}

	const uint64 NumSourceFiles = DirectoryManifest.Files.size();
	UNSYNC_LOG(L"Source files: %llu", llu(NumSourceFiles));
	UNSYNC_LOG(L"Source size: %llu bytes (%.2f MB)", llu(SourceSize), SizeMb(SourceSize));
	UNSYNC_LOG(L"Manifest unique data size: %llu bytes (%.2f MB)", llu(ManifestUniqueBytes), SizeMb(ManifestUniqueBytes));
	UNSYNC_LOG(L"Manifest unique compressed size: %llu bytes (%.2f MB)", llu(ManifestCompressedBytes), SizeMb(ManifestCompressedBytes));
	UNSYNC_LOG(L"New data size: %llu bytes (%.2f MB)", llu(ProcessedRawBytes), SizeMb(ProcessedRawBytes));
	UNSYNC_LOG(L"Compressed size: %llu bytes (%.2f MB), %.0f%%",
			   llu(CompressedBytes.load()),
			   SizeMb(CompressedBytes.load()),
			   ProcessedRawBytes > 0 ? (100.0 * double(CompressedBytes.load()) / double(ProcessedRawBytes)) : 0);

	return 0;
}

}  // namespace unsync
