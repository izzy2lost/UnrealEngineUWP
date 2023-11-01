// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCmdPack.h"
#include "UnsyncFile.h"
#include "UnsyncThread.h"
#include "UnsyncSerialization.h"
#include "UnsyncCompression.h"
#include "UnsyncHashTable.h"

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
		if (LineView.starts_with("---")) // p4 diagnostic data
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
		FileManifest.CurrentPath = std::move(LocalPath);

		std::wstring RelativePathStr = RelativePath.wstring();
		Result.insert(std::make_pair(std::move(RelativePathStr), FileManifest));

		UNSYNC_UNUSED(DepotPathUtf8);  // perhaps we should store this too?
	};

	ForLines(P4HaveDataUtf8, Callback);
}

struct FPackIndexEntry
{
	FHash160 Hash	 = {};
	uint32	 CompressedSize = 0;
	uint64	 Offset  = 0;
};
static_assert(sizeof(FPackIndexEntry) == 32);

int32 CmdPack(const FCmdPackOptions& Options)
{
	const FFileAttributes RootAttrib = GetFileAttrib(Options.RootPath);

	const FPath InputRoot	 = Options.RootPath;
	const FPath ManifestRoot = InputRoot / ".unsync";

	UNSYNC_LOG(L"Generating package for directory '%ls' ...", InputRoot.wstring().c_str());
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

	// TODO: allow explicit output path
	const bool bOutputDirectoryCreated = (PathExists(ManifestRoot) && IsDirectory(ManifestRoot)) || CreateDirectories(ManifestRoot);
	if (!bOutputDirectoryCreated)
	{
		UNSYNC_ERROR(L"Failed to create output directory '%ls'", ManifestRoot.wstring().c_str());
		return -1;
	}

	FPath		TempOutputPackFilename = ManifestRoot / "blocks.bin.tmp";
	FNativeFile PackFile(TempOutputPackFilename, EFileMode::CreateWriteOnly);
	if (!PackFile.IsValid())
	{
		UNSYNC_ERROR(L"Failed open pack output file '%ls'", TempOutputPackFilename.wstring().c_str());
		return -1;
	}

	FPath		TempOutputIndexFilename = ManifestRoot / "blocks.idx.tmp";
	FNativeFile IndexFile(TempOutputIndexFilename, EFileMode::CreateWriteOnly);
	if (!IndexFile.IsValid())
	{
		UNSYNC_ERROR(L"Failed open index output file '%ls'", TempOutputIndexFilename.wstring().c_str());
		return -1;
	}

	THashSet<FHash160> SeenMacroBlockHashSet;
	UNSYNC_LOG(L"Loading existing block packs ...");
	{
		UNSYNC_LOG_INDENT;
		FPath ExistingIndexSearchPath = ManifestRoot; // TODO: could allow overriding this
		FPath ExpectedExtension		  = FPath(".idx");
		for (const std::filesystem::directory_entry& Dir : RecursiveDirectoryScan(ExistingIndexSearchPath))
		{
			if (!Dir.is_regular_file())
			{
				continue;
			}

			const FPath& FilePath = Dir.path();
			FPathStringView FilePathView(FilePath.native());
			if (!FilePathView.ends_with(ExpectedExtension.native()))
			{
				continue;
			}

			FBuffer ExistingEntries = ReadFileToBuffer(FilePath);
			uint64	NumEntries		= ExistingEntries.Size() / sizeof(FPackIndexEntry);
			for (const FPackIndexEntry& Entry : MakeView(reinterpret_cast<FPackIndexEntry*>(ExistingEntries.Data()), NumEntries))
			{
				SeenMacroBlockHashSet.insert(Entry.Hash);
			}
		}
	}

	const FPath DirectoryManifestPath = ManifestRoot / "manifest.bin";

	std::string P4HaveBuffer;

	FDirectoryManifest DirectoryManifest;

	if (!Options.P4HavePath.empty())
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

		DirectoryManifest.Algorithm = Options.Algorithm;

		BuildP4HaveSet(InputRoot, P4HaveBuffer, DirectoryManifest.Files);

		UNSYNC_LOG(L"Loaded entries from p4 manifest: %llu", llu(DirectoryManifest.Files.size()));

		UNSYNC_LOG(L"Reading file attributes ...");
		auto UpdateFileMetadata = [](std::pair<const std::wstring, FFileManifest>& It)
		{
			FFileAttributes Attrib = GetFileAttrib(It.second.CurrentPath);

			It.second.Mtime		= Attrib.Mtime;
			It.second.Size		= Attrib.Size;
			It.second.bReadOnly = true;	 // treat all p4 files as read-only in the manifest
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

	FComputeBlocksParams BlockParams;
	BlockParams.Algorithm = Options.Algorithm;
	BlockParams.BlockSize = Options.BlockSize;
	BlockParams.bNeedMacroBlocks = true;

	std::atomic<uint64> PackFileOffset;
	std::atomic<uint64> IndexFileOffset;
	std::atomic<uint64> ProcessedRawBytes;

	std::atomic<uint64> IndexFileHashSum[2];

	std::mutex Mutex;
	BlockParams.OnMacroBlockGenerated =
		[&Mutex, &PackFile, &PackFileOffset, &IndexFile, &IndexFileOffset, &SeenMacroBlockHashSet, &IndexFileHashSum, &ProcessedRawBytes](
			const FGenericBlock& Block,
			FBufferView			 Data)
	{
		{
			std::lock_guard<std::mutex> LockGuard(Mutex);
			if (!SeenMacroBlockHashSet.insert(Block.HashStrong.ToHash160()).second)
			{
				return;
			}
		}

		const uint64 MaxCompressedSize	  = GetMaxCompressedSize(Block.Size);
		FIOBuffer	 CompressedData		  = FIOBuffer::Alloc(MaxCompressedSize, L"PackBlock");
		uint64		 ActualCompressedSize = CompressInto(Data, CompressedData.GetMutBufferView(), 9);

		if (ActualCompressedSize)
		{
			ProcessedRawBytes += Data.Size;

			CompressedData.SetDataRange(0, ActualCompressedSize);

			uint64 PackWriteOffset = PackFileOffset.fetch_add(ActualCompressedSize);
			PackFile.Write(CompressedData.GetData(), PackWriteOffset, ActualCompressedSize);

			FPackIndexEntry IndexEntry;
			IndexEntry.Hash			  = Block.HashStrong.ToHash160();
			IndexEntry.CompressedSize = CheckedNarrow<uint64>(ActualCompressedSize);
			IndexEntry.Offset		  = PackWriteOffset;

			uint64 IndexWriteOffset = IndexFileOffset.fetch_add(sizeof(IndexEntry));
			IndexFile.Write(&IndexEntry, IndexWriteOffset, sizeof(IndexEntry));

			FHash128 BlockHash128 = Block.HashStrong.ToHash128();
			uint64	 BlockHashParts[2];
			memcpy(BlockHashParts, &BlockHash128, sizeof(BlockHash128));
			IndexFileHashSum[0] += BlockHashParts[0];
			IndexFileHashSum[1] += BlockHashParts[1];
		}
	};

	{
		UNSYNC_LOG(L"Loading previous manifest ... ")
		FDirectoryManifest OldManifest;
		if (PathExists(DirectoryManifestPath) && LoadDirectoryManifest(OldManifest, InputRoot, DirectoryManifestPath))
		{
			UNSYNC_LOG(L"Previous manifest loaded")

			BlockParams.Algorithm = OldManifest.Algorithm;

			FDirectoryManifest& NewManifest = DirectoryManifest;

			// Copy file blocks from old manifest, if possible
			for (auto& NewManifestFileEntry : NewManifest.Files)
			{
				const std::wstring& FileName			 = NewManifestFileEntry.first;
				auto				OldManifestFileEntry = OldManifest.Files.find(FileName);
				if (OldManifestFileEntry == OldManifest.Files.end())
				{
					continue;
				}

				FFileManifest& NewEntry = NewManifestFileEntry.second;
				FFileManifest& OldEntry = OldManifestFileEntry->second;

				if (NewEntry.Mtime == OldEntry.Mtime && NewEntry.Size == OldEntry.Size)
				{
					NewEntry.Blocks		 = std::move(OldEntry.Blocks);
					NewEntry.MacroBlocks = std::move(OldEntry.MacroBlocks);
					NewEntry.BlockSize	 = OldEntry.BlockSize;
				}
			}
		}
	}

	UNSYNC_LOG(L"Computing file blocks ...");
	UpdateDirectoryManifestBlocks(DirectoryManifest, InputRoot, BlockParams);

	const uint64 CompressedSize = PackFileOffset;

	if (!GDryRun)
	{
		UNSYNC_LOG(L"Saving directory manifest '%ls'", DirectoryManifestPath.wstring().c_str());
		SaveDirectoryManifest(DirectoryManifest, DirectoryManifestPath);
	}

	uint64 SourceSize = 0;
	for (const auto& It : DirectoryManifest.Files)
	{
		SourceSize += It.second.Size;
	}

	const uint64 NumSourceFiles = DirectoryManifest.Files.size();
	UNSYNC_LOG(L"Source files: %llu", llu(NumSourceFiles));
	UNSYNC_LOG(L"Source size: %llu bytes (%.2f MB)", llu(SourceSize), SizeMb(SourceSize));
	UNSYNC_LOG(L"New data size: %llu bytes (%.2f MB)", llu(ProcessedRawBytes), SizeMb(ProcessedRawBytes));
	UNSYNC_LOG(L"Compressed size: %llu bytes (%.2f MB), %.0f%%",
			   llu(CompressedSize),
			   SizeMb(CompressedSize),
			   ProcessedRawBytes > 0 ? (100.0 * double(CompressedSize) / double(ProcessedRawBytes)) : 0);

	PackFile.Close();
	IndexFile.Close();

	std::error_code ErrorCode;

	if (PackFileOffset == 0)
	{
		UNSYNC_LOG(L"No new blocks found");
		FileRemove(TempOutputPackFilename, ErrorCode);
		FileRemove(TempOutputIndexFilename, ErrorCode);
	}
	else
	{
		uint64 IndexHashParts[2];
		IndexHashParts[0] = IndexFileHashSum[0];
		IndexHashParts[1] = IndexFileHashSum[1];
		FHash128 BlockHash128;
		memcpy(&BlockHash128, IndexHashParts, sizeof(BlockHash128));
		std::string OutputId = HashToHexString(BlockHash128);

		FPath FinalPackFilename	 = ManifestRoot / (OutputId + ".bin");
		FPath FinalIndexFilename = ManifestRoot / (OutputId + ".idx");

		UNSYNC_LOG(L"Saving new pack: %hs", OutputId.c_str());

		if (!FileRename(TempOutputPackFilename, FinalPackFilename, ErrorCode))
		{
			UNSYNC_ERROR(L"Failed to rename temporary pack file to '%ls'", FinalPackFilename.wstring().c_str());
			return 1;
		}

		if (!FileRename(TempOutputIndexFilename, FinalIndexFilename, ErrorCode))
		{
			UNSYNC_ERROR(L"Failed to rename temporary index file to '%ls'", FinalIndexFilename.wstring().c_str());
			return 1;
		}
	}

	return 0;
}

}
