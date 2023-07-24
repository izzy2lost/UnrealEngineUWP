// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeBuffer.h"
#include "ComputePlatform.h"
#include <assert.h>
#include <iostream>
#include <assert.h>

struct FComputeBufferDetail
{
	enum class EWriteState : unsigned long long
	{
		// Writer has moved to the next chunk
		MovedToNext = 0b00,

		// Chunk is still being appended to
		Writing = 0b10,

		// This chunk marks the end of the stream
		Complete = 0b11,
	};

	union FChunkState
	{
		long long Value;

		FChunkState(long long InValue)
			: Value(InValue)
		{
		}

		// Constructor
		FChunkState(EWriteState WriteState, int ReaderFlags, int Length)
			: Value((unsigned long long)Length | ((unsigned long long)ReaderFlags << 31) | ((unsigned long long)WriteState << 62))
		{
		}

		// Written length of this chunk
		int GetLength() const { return (int)(Value & 0x7fffffff); }

		// Set of flags which are set for each reader that still has to read from a chunk
		int GetReaderFlags() const { return (int)((Value >> 31) & 0x7fffffff); }

		// State of the writer
		EWriteState GetWriteState() const { return (EWriteState)((unsigned long long)Value >> 62); }

		// Test whether a particular reader is still referencing the chunk
		bool HasReaderFlag(int ReaderIdx) const { return (Value & (1ULL << (31 + ReaderIdx))) != 0; }

	private:
		struct // For debugging purposes only; non-portable assumption of bitfield layout
		{
			unsigned long long Length : 31;
			unsigned long long ReaderFlags : 31;
			EWriteState WriteState : 2;
		};
	};

	struct FChunkStatePtr
	{
	public:
		// Read the state value from memory
		FChunkState Get() const
		{
			return FChunkState(FComputePlatform::AtomicRead64(&State.Value));
		}

		// Append data to the chunk
		void Append(long long Length)
		{
			FComputePlatform::AtomicAdd64(&State.Value, Length);
		}

		// Mark the chunk as being written to
		void StartWriting(int numReaders)
		{
			FComputePlatform::AtomicWrite64(&State.Value, FChunkState(EWriteState::Writing, (1 << numReaders) - 1, 0).Value);
		}

		// Mark this chunk as the end of the stream
		void MarkComplete()
		{
			FComputePlatform::AtomicOr64(&State.Value, FChunkState(EWriteState::Complete, 0, 0).Value);
		}

		// Clear the reader flag
		void FinishReading(int ReaderIdx)
		{
			FComputePlatform::AtomicAnd64(&State.Value, ~FChunkState((EWriteState)0, 1 << ReaderIdx, 0).Value);
		}

		// Move to the next chunk
		void FinishWriting()
		{
			FComputePlatform::AtomicAnd64(&State.Value, ~FChunkState(EWriteState::Writing, 0, 0).Value);
		}

	private:
		volatile FChunkState State;
	};

	union FReaderState
	{
		long long Value;

		FReaderState(long long InValue) : Value(InValue) { }
		FReaderState(int chunkIdx, int offset) : Value(((unsigned long long)chunkIdx << 32) | offset) { }

		int GetChunkIdx() const { return (int)(Value >> 32); }
		int GetOffset() const { return (int)(unsigned int)Value; }

	private:
		struct // For debugging purposes only; non-portable assumption of bitfield layout
		{
			unsigned int Offset : 32;
			unsigned int ChunkIdx : 32;
		};
	};

	struct FReaderStatePtr
	{
	public:
		FReaderState Get() const
		{
			return FReaderState(FComputePlatform::AtomicRead64((volatile long long*)&State.Value));
		}

		void Set(FReaderState State)
		{
			FComputePlatform::AtomicWrite64(&State.Value, State.Value);
		}

		void Advance(size_t Length)
		{
			FComputePlatform::AtomicAdd64(&State.Value, Length);
		}

	private:
		volatile FReaderState State;
	};

	struct FHeader
	{
		int NumReaders;
		int NumChunks;
		int ChunkLength;
		int WriteChunkIdx;
		FChunkStatePtr Chunks[FComputeBuffer::MaxChunks];
		FReaderStatePtr Readers[FComputeBuffer::MaxReaders];
	};

	wchar_t Name[260];

	FHeader* Header;
	unsigned char* ChunkPtrs[FComputeBuffer::MaxChunks];

	FComputeManualResetEvent WriterEvent;
	FComputeManualResetEvent ReaderEvents[FComputeBuffer::MaxReaders];

	FComputeBufferReader Reader;
	FComputeBufferWriter Writer;

	FComputeBufferDetail(const wchar_t* Name)
		: Header(nullptr)
		, ChunkPtrs{ nullptr, }
		, RefCount(1)
	{
		static_assert(sizeof(FChunkStatePtr) == sizeof(long long), "Incorrect size of FChunkStatePtr; check union is declared correctly.");
		static_assert(sizeof(FReaderStatePtr) == sizeof(long long), "Incorrect size of FReaderStatePtr; check union is declared correctly.");

		wcscpy_s(this->Name, Name);
	}

	~FComputeBufferDetail()
	{
		Header = nullptr;
	}

	static std::shared_ptr<FComputeBufferDetail> CreateNew(const FComputeBuffer::FParams& Params)
	{
		long long Capacity = sizeof(FHeader) + (Params.NumChunks * sizeof(unsigned int)) + (Params.NumChunks * Params.ChunkLength);

		const wchar_t* Name = Params.Name;

		wchar_t BaseNameBuffer[FComputeBuffer::MaxNameLength];
		if (Name == nullptr)
		{
			FComputePlatform::CreateUniqueName(BaseNameBuffer, FComputeBuffer::MaxNameLength);
			Name = BaseNameBuffer;
		}

		wchar_t NameBuffer[FComputeBuffer::MaxNameLength];
		swprintf(NameBuffer, FComputeBuffer::MaxNameLength, L"%s_M", Name);

		std::shared_ptr<FComputeBufferDetail> Detail = std::make_shared<FComputeBufferDetail>(Name);
		if (!Detail->MemoryMappedFile.Create(NameBuffer, Capacity))
		{
			return nullptr;
		}

		Detail->Header = (FHeader*)Detail->MemoryMappedFile.GetPointer();
		if (Detail->Header == nullptr)
		{
			return nullptr;
		}

		memset(Detail->Header, 0, sizeof(*Header));
		Detail->Header->NumReaders = Params.NumReaders;
		Detail->Header->NumChunks = Params.NumChunks;
		Detail->Header->ChunkLength = Params.ChunkLength;

		swprintf(NameBuffer, FComputeBuffer::MaxNameLength, L"%s_W", Name);
		if (!Detail->WriterEvent.Create(NameBuffer))
		{
			return nullptr;
		}

		for (int ReaderIdx = 0; ReaderIdx < Detail->Header->NumReaders; ReaderIdx++)
		{
			swprintf(NameBuffer, FComputeBuffer::MaxNameLength, L"%s_R%d", Name, ReaderIdx);
			if (!Detail->ReaderEvents[ReaderIdx].Create(NameBuffer))
			{
				return nullptr;
			}
		}

		Detail->Header->Chunks[0].StartWriting(Detail->Header->NumReaders);

		InitShared(Detail);
		return Detail;
	}

	static std::shared_ptr<FComputeBufferDetail> OpenExisting(const wchar_t* Name)
	{
		wchar_t NameBuffer[FComputeBuffer::MaxNameLength];
		swprintf(NameBuffer, FComputeBuffer::MaxNameLength, L"%s_M", Name);

		std::shared_ptr<FComputeBufferDetail> Detail = std::make_shared<FComputeBufferDetail>(Name);
		if (!Detail->MemoryMappedFile.OpenExisting(NameBuffer))
		{
			return nullptr;
		}

		Detail->Header = (FHeader*)Detail->MemoryMappedFile.GetPointer();
		if (Detail->Header == nullptr)
		{
			return nullptr;
		}

		swprintf(NameBuffer, FComputeBuffer::MaxNameLength, L"%s_W", Name);
		if (!Detail->WriterEvent.OpenExisting(NameBuffer))
		{
			return nullptr;
		}

		for (int ReaderIdx = 0; ReaderIdx < Detail->Header->NumReaders; ReaderIdx++)
		{
			swprintf(NameBuffer, FComputeBuffer::MaxNameLength, L"%s_R%d", Name, ReaderIdx);
			if (!Detail->ReaderEvents[ReaderIdx].OpenExisting(NameBuffer))
			{
				return nullptr;
			}
		}

		InitShared(Detail);
		return Detail;
	}

	void AddRef()
	{
		FComputePlatform::AtomicIncrement(&RefCount);
	}

	void Release()
	{
		if (FComputePlatform::AtomicDecrement(&RefCount) == 0)
		{
			delete this;
		}
	}

	bool IsComplete(int ReaderIdx) const
	{
		FReaderState ReaderState = Header->Readers[ReaderIdx].Get();
		FChunkState ChunkState = Header->Chunks[ReaderState.GetChunkIdx()].Get();

		return ChunkState.GetWriteState() == EWriteState::Complete && ReaderState.GetOffset() == ChunkState.GetLength();
	}

	void AdvanceReadPosition(int ReaderIdx, size_t Size)
	{
		Header->Readers[ReaderIdx].Advance(Size);
	}

	size_t GetMaxReadSize(int ReaderIdx) const
	{
		const FReaderStatePtr& ReaderStatePtr = Header->Readers[ReaderIdx];
		FReaderState ReaderState = ReaderStatePtr.Get();

		const FChunkStatePtr& ChunkStatePtr = Header->Chunks[ReaderState.GetChunkIdx()];
		FChunkState ChunkState = ChunkStatePtr.Get();

		if (ChunkState.HasReaderFlag(ReaderIdx))
		{
			return ChunkState.GetLength() - ReaderState.GetOffset();
		}
		else
		{
			return 0;
		}
	}

	const unsigned char* WaitToRead(int ReaderIdx, size_t MinSize, int TimeoutMs)
	{
		for (; ; )
		{
			FReaderStatePtr& ReaderStatePtr = Header->Readers[ReaderIdx];
			FReaderState ReaderState = ReaderStatePtr.Get();

			FChunkStatePtr& ChunkStatePtr = Header->Chunks[ReaderState.GetChunkIdx()];
			FChunkState ChunkState = ChunkStatePtr.Get();

			if (!ChunkState.HasReaderFlag(ReaderIdx))
			{
				// Wait until the current chunk is readable
				ReaderEvents[ReaderIdx].Reset();
				if (!ChunkState.HasReaderFlag(ReaderIdx) && !ReaderEvents[ReaderIdx].Wait(TimeoutMs))
				{
					return nullptr;
				}
			}
			else if (ReaderState.GetOffset() + MinSize <= ChunkState.GetLength())
			{
				// We have enough data in the chunk to be able to read a message
				return ChunkPtrs[ReaderState.GetChunkIdx()] + ReaderState.GetOffset();
			}
			else if (ChunkState.GetWriteState() == EWriteState::Writing)
			{
				// Wait until there is more data in the chunk
				ReaderEvents[ReaderIdx].Reset();
				if (Header->Chunks[ReaderState.GetChunkIdx()].Get().Value == ChunkState.Value && !ReaderEvents[ReaderIdx].Wait(TimeoutMs))
				{
					return nullptr;
				}
			}
			else if (ReaderState.GetOffset() < ChunkState.GetLength() || ChunkState.GetWriteState() == EWriteState::Complete)
			{
				// Cannot read the requested amount of data from this chunk.
				return nullptr;
			}
			else if (ChunkState.GetWriteState() == EWriteState::MovedToNext)
			{
				// Move to the next chunk
				ChunkStatePtr.FinishReading(ReaderIdx);
				WriterEvent.Set();

				int chunkIdx = ReaderState.GetChunkIdx() + 1;
				if (chunkIdx == Header->NumChunks)
				{
					chunkIdx = 0;
				}

				ReaderStatePtr.Set(FReaderState(chunkIdx, 0));
			}
			else
			{
				assert(false);
			}
		}
	}

	void MarkComplete()
	{
		Header->Chunks[Header->WriteChunkIdx].MarkComplete();
		SetAllReaderEvents();
	}

	void AdvanceWritePosition(size_t Size)
	{
		Header->Chunks[Header->WriteChunkIdx].Append(Size);
		SetAllReaderEvents();
	}

	size_t GetMaxWriteSize() const
	{
		FChunkState Value = Header->Chunks[Header->WriteChunkIdx].Get();
		if (Value.GetWriteState() == EWriteState::Complete)
		{
			return 0;
		}
		else
		{
			return Header->ChunkLength - Value.GetLength();
		}
	}

	size_t Write(const void* Buffer, size_t MaxSize, int TimeoutMs)
	{
		unsigned char* SendData = WaitToWrite(1, TimeoutMs);
		if (SendData == nullptr)
		{
			return 0;
		}

		size_t SendSize = GetMaxWriteSize();
		if (MaxSize < SendSize)
		{
			SendSize = MaxSize;
		}

		memcpy(SendData, Buffer, SendSize);
		AdvanceWritePosition(SendSize);
		return SendSize;
	}

	unsigned char* WaitToWrite(size_t MinSize, int TimeoutMs)
	{
		assert(MinSize <= Header->ChunkLength);

		for (; ; )
		{
			FChunkStatePtr& WriteChunkStatePtr = Header->Chunks[Header->WriteChunkIdx];

			FChunkState ChunkState = WriteChunkStatePtr.Get();
			if (ChunkState.GetWriteState() == EWriteState::Writing)
			{
				int Length = ChunkState.GetLength();
				if (Length + MinSize <= Header->ChunkLength)
				{
					return ChunkPtrs[Header->WriteChunkIdx] + Length;
				}

				WriteChunkStatePtr.FinishWriting(); // STATE CHANGE
				SetAllReaderEvents();
			}

			if (ChunkState.GetWriteState() == EWriteState::Complete)
			{
				return nullptr;
			}

			int NextWriteChunkIdx = Header->WriteChunkIdx + 1;
			if (NextWriteChunkIdx == Header->NumChunks)
			{
				NextWriteChunkIdx = 0;
			}

			FChunkStatePtr& NextWriteChunkStatePtr = Header->Chunks[NextWriteChunkIdx];
			while (NextWriteChunkStatePtr.Get().GetReaderFlags() != 0)
			{
				if (!WriterEvent.Wait(TimeoutMs))
				{
					return nullptr;
				}
				WriterEvent.Reset();
			}

			Header->WriteChunkIdx = NextWriteChunkIdx;

			NextWriteChunkStatePtr.StartWriting(Header->NumReaders);
		}
	}

private:
	FComputeMemoryMappedFile MemoryMappedFile;
	volatile long RefCount;

	static void InitShared(std::shared_ptr<FComputeBufferDetail> Detail)
	{
		Detail->Reader = FComputeBufferReader(Detail, 0);
		Detail->Writer = FComputeBufferWriter(Detail);

		FHeader* Header = Detail->Header;

		unsigned char* NextPtr = (unsigned char*)(Header + 1);
		for (int ChunkIdx = 0; ChunkIdx < Header->NumChunks; ChunkIdx++)
		{
			Detail->ChunkPtrs[ChunkIdx] = NextPtr;
			NextPtr += Header->ChunkLength;
		}
	}

	void SetAllReaderEvents()
	{
		for (int Idx = 0; Idx < Header->NumReaders; Idx++)
		{
			ReaderEvents[Idx].Set();
		}
	}
};




//// FComputeBuffer /////

FComputeBuffer::FComputeBuffer()
	: Detail(nullptr)
{
}

FComputeBuffer::FComputeBuffer(const FComputeBuffer& Buffer)
	: FComputeBuffer()
{
	if (Buffer.Detail != nullptr)
	{
		Detail = Buffer.Detail;
		Detail->AddRef();
	}
}

FComputeBuffer::FComputeBuffer(FComputeBuffer&& Buffer) noexcept
	: Detail(Buffer.Detail)
{
	Buffer.Detail = nullptr;
}

FComputeBuffer::~FComputeBuffer()
{
	Close();
}

bool FComputeBuffer::CreateNew(const FParams& Params)
{
	Detail = FComputeBufferDetail::CreateNew(Params);
	return Detail != nullptr;
}

bool FComputeBuffer::OpenExisting(const wchar_t* Name)
{
	Detail = FComputeBufferDetail::OpenExisting(Name);
	return Detail != nullptr;
}

void FComputeBuffer::Close()
{
	Detail.reset();
}

FComputeBufferReader& FComputeBuffer::GetReader()
{
	return Detail->Reader;
}

const FComputeBufferReader& FComputeBuffer::GetReader() const
{
	return Detail->Reader;
}

FComputeBufferWriter& FComputeBuffer::GetWriter()
{
	return Detail->Writer;
}

const FComputeBufferWriter& FComputeBuffer::GetWriter() const
{
	return Detail->Writer;
}







//// FComputeBufferReader /////

FComputeBufferReader::FComputeBufferReader()
	: Detail(nullptr)
	, ReaderIdx(-1)
{
}

FComputeBufferReader::FComputeBufferReader(std::shared_ptr<FComputeBufferDetail> Detail, int ReaderIdx)
	: Detail(std::move(Detail))
	, ReaderIdx(ReaderIdx)
{
}

FComputeBufferReader::~FComputeBufferReader()
{
}

void FComputeBufferReader::ForceComplete()
{
	Detail->MarkComplete();
}

bool FComputeBufferReader::IsComplete() const
{
	return Detail->IsComplete(ReaderIdx);
}

void FComputeBufferReader::AdvanceReadPosition(size_t Size)
{
	Detail->AdvanceReadPosition(ReaderIdx, Size);
}

size_t FComputeBufferReader::GetMaxReadSize() const
{
	return Detail->GetMaxReadSize(ReaderIdx);
}

size_t FComputeBufferReader::Read(void* Buffer, size_t MaxSize, int TimeoutMs)
{
	const unsigned char* RecvData = WaitToRead(1, TimeoutMs);
	if (RecvData == nullptr)
	{
		return 0;
	}

	size_t RecvSize = GetMaxReadSize();
	if (MaxSize < RecvSize)
	{
		RecvSize = MaxSize;
	}

	memcpy(Buffer, RecvData, RecvSize);
	AdvanceReadPosition(RecvSize);
	return RecvSize;
}

const unsigned char* FComputeBufferReader::WaitToRead(size_t MinSize, int TimeoutMs)
{
	return Detail->WaitToRead(ReaderIdx, MinSize, TimeoutMs);
}

const wchar_t* FComputeBufferReader::GetName() const
{
	return Detail->Name;
}





//// FComputeBufferWriter /////

FComputeBufferWriter::FComputeBufferWriter()
	: Detail(nullptr)
{
}

FComputeBufferWriter::FComputeBufferWriter(std::shared_ptr<FComputeBufferDetail> Detail)
	: Detail(std::move(Detail))
{
}

FComputeBufferWriter::~FComputeBufferWriter()
{
}

void FComputeBufferWriter::MarkComplete()
{
	Detail->MarkComplete();
}

void FComputeBufferWriter::AdvanceWritePosition(size_t Size)
{
	Detail->AdvanceWritePosition(Size);
}

size_t FComputeBufferWriter::GetMaxWriteSize() const
{
	return Detail->GetMaxWriteSize();
}

size_t FComputeBufferWriter::Write(const void* Buffer, size_t MaxSize, int TimeoutMs)
{
	unsigned char* SendData = WaitToWrite(1, TimeoutMs);
	if (SendData == nullptr)
	{
		return 0;
	}

	size_t SendSize = GetMaxWriteSize();
	if (MaxSize < SendSize)
	{
		SendSize = MaxSize;
	}

	memcpy(SendData, Buffer, SendSize);
	AdvanceWritePosition(SendSize);
	return SendSize;
}

unsigned char* FComputeBufferWriter::WaitToWrite(size_t MinSize, int TimeoutMs)
{
	return Detail->WaitToWrite(MinSize, TimeoutMs);
}

const wchar_t* FComputeBufferWriter::GetName() const
{
	return Detail->Name;
}
