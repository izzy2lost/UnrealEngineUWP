// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncPack.h"
#include "UnsyncFile.h"
#include "UnsyncCompression.h"
#include "UnsyncHashTable.h"
#include "UnsyncCore.h"

namespace unsync {

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

FPackWriteContext::FPackWriteContext(const FPath& InOutputRoot) : OutputRoot(InOutputRoot)
{
	InternalReset();
}

FPackWriteContext::~FPackWriteContext()
{
	InternalFinishPack();
}

void
FPackWriteContext::AddCompressedBlock(const FGenericBlock& Block, FHash128 CompressedHash, FBufferView CompressedData)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);

	UNSYNC_ASSERT(CompressedData.Size <= GMaxPackFileSize);

	if (PackBuffer.Size() + CompressedData.Size > GMaxPackFileSize)
	{
		InternalFinishPack();
	}

	FPackIndexEntry IndexEntry;
	IndexEntry.BlockHash	  = Block.HashStrong.ToHash128();
	IndexEntry.CompressedHash = CompressedHash;
	IndexEntry.PackBlockOffset = CheckedNarrow(PackBuffer.Size());
	IndexEntry.PackBlockSize   = CheckedNarrow(CompressedData.Size);

	IndexEntries.push_back(IndexEntry);
	PackBuffer.Append(CompressedData);

	UNSYNC_ASSERT(PackBuffer.Size() == IndexEntry.PackBlockOffset + IndexEntry.PackBlockSize);

	AddHash(IndexFileHashSum, IndexEntry.BlockHash);

	ProcessedRawBytes += Block.Size;
	ProcessedCompressedBytes = CompressedData.Size;
}

void
FPackWriteContext::FinishPack()
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
	InternalFinishPack();
}

void
FPackWriteContext::InternalFinishPack()
{
	// Assumes the function is called from a thread-safe context

	if (IndexEntries.empty())
	{
		return;
	}

	FHash128	BlockHash128 = MakeHashFromParts(IndexFileHashSum);
	std::string OutputId	 = HashToHexString(BlockHash128);

	FPath FinalPackFilename	 = OutputRoot / (OutputId + ".unsync_pack");
	FPath FinalIndexFilename = OutputRoot / (OutputId + ".unsync_index");

	UNSYNC_LOG(L"Saving new pack: %hs", OutputId.c_str());

	if (!GDryRun)
	{
		// TODO: offload final disk write to a background thread

		if (!WriteBufferToFile(FinalPackFilename, PackBuffer, EFileMode::CreateWriteOnly))
		{
			UNSYNC_FATAL(L"Failed to write pack file '%ls'", FinalPackFilename.wstring().c_str());
		}

		const uint8* IndexData	   = reinterpret_cast<const uint8*>(IndexEntries.data());
		uint64		 IndexDataSize = sizeof(IndexEntries[0]) * IndexEntries.size();

		FPackIndexHeader IndexHeader;
		const uint64	 IndexTotalSize = sizeof(IndexHeader) + IndexDataSize;

		FNativeFile IndexFile(FinalIndexFilename, EFileMode::CreateWriteOnly, IndexTotalSize);
		if (!IndexFile.IsValid())
		{
			UNSYNC_FATAL(L"Failed to write pack index file '%ls'", FinalIndexFilename.wstring().c_str());
		}

		uint64 WroteBytes = 0;
		
		WroteBytes += IndexFile.Write(&IndexHeader, WroteBytes, sizeof(IndexHeader));
		WroteBytes += IndexFile.Write(IndexData, WroteBytes, IndexDataSize);

		if (WroteBytes != IndexTotalSize)
		{
			UNSYNC_FATAL(L"Failed to write pack index file '%ls'", FinalIndexFilename.wstring().c_str());
		}
	}

	GeneratedPackIds.push_back(BlockHash128);

	InternalReset();
}

void
FPackWriteContext::InternalReset()
{
	PackBuffer.Reserve(GMaxPackFileSize);
	PackBuffer.Clear();
	IndexEntries.clear();

	IndexFileHashSum[0] = 0;
	IndexFileHashSum[1] = 0;
}

FPackWriteContext::FCompressedBlock
FPackWriteContext::CompressBlock(FBufferView RawData)
{
	FPackWriteContext::FCompressedBlock Result;

	const uint64 MaxCompressedSize = GetMaxCompressedSize(RawData.Size);
	Result.Data					   = FIOBuffer::Alloc(MaxCompressedSize, L"PackBlock");
	uint64 ActualCompressedSize	   = CompressInto(RawData, Result.Data.GetMutBufferView(), 9);

	if (!ActualCompressedSize)
	{
		UNSYNC_FATAL(L"Failed to compress file block");
	}
	Result.Data.SetDataRange(0, ActualCompressedSize);

	Result.Hash = HashBlake3Bytes<FHash128>(Result.Data.GetData(), ActualCompressedSize);

	return Result;
}

void FPackWriteContext::CompressAndAddBlock(const FGenericBlock& Block, FBufferView RawData)
{
	FCompressedBlock Compressed = CompressBlock(RawData);
	AddCompressedBlock(Block, Compressed.Hash, Compressed.Data.GetBufferView());
}

void
FPackWriteContext::GetUniqueGeneratedPackIds(std::vector<FHash128>& Output)
{
	THashSet<FHash128> KnownHashes;
	for (const FHash128& Hash : Output)
	{
		KnownHashes.insert(Hash);
	}

	std::lock_guard<std::mutex> LockGuard(Mutex);

	for (const FHash128& Hash : GeneratedPackIds)
	{
		if (KnownHashes.insert(Hash).second)
		{
			Output.push_back(Hash);
		}
	}
}

}  // namespace unsync
