// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubBulkData.h"

#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

FLiveLinkHubBulkData::FScopedBulkDataMemoryReader::FScopedBulkDataMemoryReader(const int64 InOffset, const int32 InBytesToRead, FLiveLinkHubBulkData* InBulkData)
{
	Memory.SetNumUninitialized(InBytesToRead);

	LocalBulkDataOffset = InBulkData->ReadBulkDataImpl(InOffset, InBytesToRead, Memory.GetData());
	MemoryReader = MakeUnique<FMemoryReader>(Memory, true);
}

FLiveLinkHubBulkData::~FLiveLinkHubBulkData()
{
	UnloadBulkData();
}

void FLiveLinkHubBulkData::CloseFileReader()
{
	RecordingFileReader.Reset();
}

void FLiveLinkHubBulkData::UnloadBulkData()
{
	CloseFileReader();
	BulkData.UnloadBulkData();
}

void FLiveLinkHubBulkData::ReadBulkData(const int64 InBytesToRead, uint8* InMemory)
{
	BulkDataOffset = ReadBulkDataImpl(BulkDataOffset, InBytesToRead, InMemory);
}

FLiveLinkHubBulkData::FScopedBulkDataMemoryReader FLiveLinkHubBulkData::CreateBulkDataMemoryReader(const int64 InBytesToRead)
{
	FScopedBulkDataMemoryReader Reader(BulkDataOffset, InBytesToRead, this);
	BulkDataOffset = Reader.GetBulkDataOffset();

	return Reader;
}

void FLiveLinkHubBulkData::ResetBulkDataOffset()
{
	BulkDataOffset = BulkData.GetBulkDataOffsetInFile();
}

void FLiveLinkHubBulkData::SetBulkDataOffset(const int64 InNewOffset)
{
	BulkDataOffset = InNewOffset;
}

void FLiveLinkHubBulkData::Serialize(FArchive& Ar, UObject* Owner)
{
	BulkData.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
	BulkData.Serialize(Ar, Owner);
}

void FLiveLinkHubBulkData::WriteBulkData(FBufferArchive& BufferArchive)
{
	BulkData.Lock(LOCK_READ_WRITE);
	BulkData.Realloc(BufferArchive.Num());
	unsigned char* BulkDataPtr = BulkData.Realloc(BufferArchive.Num());
	FMemory::Memcpy(BulkDataPtr, BufferArchive.GetData(), BufferArchive.Num());
	BulkData.Unlock();
}

int64 FLiveLinkHubBulkData::ReadBulkDataImpl(int64 InOffset, int64 InBytesToRead, uint8* InMemory)
{
	if (!RecordingFileReader.IsValid())
	{
		check(BulkData.DoesExist());
		check(BulkData.CanLoadFromDisk());
		check(!BulkData.IsInlined());
		check(!BulkData.IsInSeparateFile());
		check(!BulkData.IsBulkDataLoaded());

		RecordingFileReader = TUniquePtr<IAsyncReadFileHandle>(BulkData.OpenAsyncReadHandle());
	}

	check(RecordingFileReader);

	const TUniquePtr<IAsyncReadRequest> ReadRequest(RecordingFileReader->ReadRequest(
		InOffset,
		InBytesToRead,
		AIOP_High,
		nullptr,
		InMemory
		));
	ReadRequest->WaitCompletion();

	return InOffset + InBytesToRead;
}
