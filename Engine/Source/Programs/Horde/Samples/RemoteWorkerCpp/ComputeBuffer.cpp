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

		// Read the state value from memory
		void Set(FChunkState NewState)
		{
			FComputePlatform::AtomicWrite64(&State.Value, NewState.Value);
		}

		// Attempt to update the chunk state
		bool TryUpdate(FChunkState PrevState, FChunkState NextState)
		{
			return FComputePlatform::AtomicCompareExchange64(&State.Value, NextState.Value, PrevState.Value);
		}

		// Append data to the chunk
		void Append(long long Length)
		{
			FComputePlatform::AtomicAdd64(&State.Value, Length);
		}

		// Mark this chunk as the end of the stream
		void MarkComplete()
		{
			FComputePlatform::AtomicOr64(&State.Value, FChunkState(EWriteState::Complete, 0, 0).Value);
		}

		// Start reading the chunk with the given reader
		void StartReading(int ReaderIdx)
		{
			FComputePlatform::AtomicOr64(&State.Value, FChunkState((EWriteState)0, 1 << ReaderIdx, 0).Value);
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

	struct FReaderState
	{
		const int ReaderIdx;

		int Offset;
		int ChunkIdx;
		bool Detached;

		FReaderState(int InReaderIdx)
			: ReaderIdx(InReaderIdx)
			, Offset(0)
			, ChunkIdx(0)
			, Detached(false)
		{ }
	};

	union FWriterState
	{
		long long Value;

		FWriterState(long long InValue) : Value(InValue) { }
		FWriterState(int ChunkIdx, int ReaderFlags, bool HasWrapped) : Value(ChunkIdx | ((long long)ReaderFlags << 32) | (HasWrapped? (1ULL << 63) : 0)) { }

		int GetReaderFlags() const { return ((unsigned long long)Value >> 32) & 0xffff; }
		int GetChunkIdx() const { return Value & 0x7fffffff; }
		bool HasWrapped() const { return (Value & (1ULL << 63)) != 0; }

	private:
		struct // For debugging purposes only; non-portable assumption of bitfield layout
		{
			int ChunkIdx : 32;
			int ReaderFlags : 31;
			int Wrapped : 1;
		};
	};

	struct FWriterStatePtr
	{
	public:
		FWriterState Get() const
		{
			return FWriterState(FComputePlatform::AtomicRead64(&State.Value));
		}

		void Set(FWriterState State)
		{
			FComputePlatform::AtomicWrite64(&State.Value, State.Value);
		}

		bool TryUpdate(FWriterState PrevValue, FWriterState NextValue)
		{
			return FComputePlatform::AtomicCompareExchange64(&State.Value, NextValue.Value, PrevValue.Value);
		}

	private:
		volatile FWriterState State;
	};

	struct FHeader
	{
		int NumReaders;
		int NumChunks;
		int ChunkLength;
		long AllocatedReaders;
		FWriterStatePtr Writer;
		FChunkStatePtr Chunks[FComputeBuffer::MaxChunks];
	};

	wchar_t Name[260];

	FHeader* Header;
	unsigned char* ChunkPtrs[FComputeBuffer::MaxChunks];

	FComputeManualResetEvent WriterEvent;
	FComputeManualResetEvent ReaderEvents[FComputeBuffer::MaxReaders];

	FComputeBufferWriter Writer;

	FComputeBufferDetail(const wchar_t* Name)
		: Header(nullptr)
		, ChunkPtrs{ nullptr, }
	{
		static_assert(sizeof(FChunkStatePtr) == sizeof(long long), "Incorrect size of FChunkStatePtr; check union is declared correctly.");
		static_assert(sizeof(FWriterStatePtr) == sizeof(long long), "Incorrect size of FWriterStatePtr; check union is declared correctly.");

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
		Detail->Header->Chunks[0].Set(FChunkState(EWriteState::Writing, 0, 0));

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

	bool IsComplete(const FReaderState& ReaderState) const
	{
		if (ReaderState.Detached)
		{
			return true;
		}

		FChunkState ChunkState = Header->Chunks[ReaderState.ChunkIdx].Get();
		return ChunkState.GetWriteState() == EWriteState::Complete && ReaderState.Offset == ChunkState.GetLength();
	}

	int AllocateReader(int ChunkIdx, int Offset)
	{
		for (;;)
		{
			int AllocatedReaders = Header->AllocatedReaders;
			int ReaderFlag = (AllocatedReaders + 1) ^ AllocatedReaders;

			int ReaderIdx = FComputePlatform::FloorLog2(ReaderFlag);
			if (ReaderIdx >= Header->NumReaders)
			{
				return -1;
			}

			if (FComputePlatform::AtomicCompareExchange(&Header->AllocatedReaders, AllocatedReaders | ReaderFlag, AllocatedReaders))
			{
				for (;;)
				{
					FWriterState State = Header->Writer.Get();
					if (Header->Writer.TryUpdate(State, FWriterState(State.GetChunkIdx(), State.GetReaderFlags() | ReaderFlag, State.HasWrapped())))
					{
						for (int WriteChunkIdx = 0; WriteChunkIdx <= State.GetChunkIdx(); WriteChunkIdx++)
						{
							Header->Chunks[WriteChunkIdx].StartReading(ReaderIdx);
						}
						return ReaderIdx;
					}
				}
			}
		}
	}

	void ReleaseReader(int ReaderIdx)
	{
		int ReaderFlag = 1 << ReaderIdx;
		for (int Idx = 0; Idx < Header->NumChunks; Idx++)
		{
			Header->Chunks[Idx].FinishReading(ReaderFlag);
		}
		FComputePlatform::AtomicAnd(&Header->AllocatedReaders, (long)~ReaderFlag);
	}

	size_t GetMaxReadSize(const FReaderState& ReaderState) const
	{
		if (ReaderState.Detached)
		{
			return 0;
		}

		const FChunkStatePtr& ChunkStatePtr = Header->Chunks[ReaderState.ChunkIdx];
		FChunkState ChunkState = ChunkStatePtr.Get();

		if (!ChunkState.HasReaderFlag(ReaderState.ReaderIdx))
		{
			return 0;
		}

		return ChunkState.GetLength() - ReaderState.Offset;
	}

	const unsigned char* WaitToRead(FReaderState& ReaderState, size_t MinSize, int TimeoutMs)
	{
		int ReaderIdx = ReaderState.ReaderIdx;
		for (; ; )
		{
			if (ReaderState.Detached)
			{
				return nullptr;
			}

			FChunkStatePtr& ChunkStatePtr = Header->Chunks[ReaderState.ChunkIdx];
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
			else if (ReaderState.Offset + MinSize <= ChunkState.GetLength())
			{
				// We have enough data in the chunk to be able to read a message
				return ChunkPtrs[ReaderState.ChunkIdx] + ReaderState.Offset;
			}
			else if (ChunkState.GetWriteState() == EWriteState::Writing)
			{
				// Wait until there is more data in the chunk
				ReaderEvents[ReaderIdx].Reset();
				if (Header->Chunks[ReaderState.ChunkIdx].Get().Value == ChunkState.Value && !ReaderEvents[ReaderIdx].Wait(TimeoutMs))
				{
					return nullptr;
				}
			}
			else if (ReaderState.Offset < ChunkState.GetLength() || ChunkState.GetWriteState() == EWriteState::Complete)
			{
				// Cannot read the requested amount of data from this chunk.
				return nullptr;
			}
			else if (ChunkState.GetWriteState() == EWriteState::MovedToNext)
			{
				// Move to the next chunk
				ChunkStatePtr.FinishReading(ReaderIdx);
				WriterEvent.Set();

				if (++ReaderState.ChunkIdx == Header->NumChunks)
				{
					ReaderState.ChunkIdx = 0;
				}

				ReaderState.Offset = 0;
			}
			else
			{
				assert(false);
			}
		}
	}

	void MarkComplete()
	{
		FWriterState State = Header->Writer.Get();
		Header->Chunks[State.GetChunkIdx()].MarkComplete();
		SetAllReaderEvents();
	}

	void AdvanceWritePosition(size_t Size)
	{
		FWriterState State = Header->Writer.Get();
		Header->Chunks[State.GetChunkIdx()].Append(Size);
		SetAllReaderEvents();
	}

	size_t GetMaxWriteSize() const
	{
		FWriterState State = Header->Writer.Get();

		FChunkState Value = Header->Chunks[State.GetChunkIdx()].Get();
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

		// Get the current chunk we're writing to
		FWriterState WriterState = Header->Writer.Get();
		int WriteChunkIdx = WriterState.GetChunkIdx();

		FChunkStatePtr& WriteChunkStatePtr = Header->Chunks[WriteChunkIdx];

		// Check if we can append to this chunk
		FChunkState ChunkState = WriteChunkStatePtr.Get();
		if (ChunkState.GetWriteState() == EWriteState::Writing)
		{
			int Length = ChunkState.GetLength();
			if (Length + MinSize <= Header->ChunkLength)
			{
				return ChunkPtrs[WriteChunkIdx] + Length;
			}

			WriteChunkStatePtr.FinishWriting(); // STATE CHANGE
			SetAllReaderEvents();
		}

		if (ChunkState.GetWriteState() == EWriteState::Complete)
		{
			return nullptr;
		}

		// Otherwise get the next chunk to write to
		int NextWriteChunkIdx = WriteChunkIdx + 1;
		if (NextWriteChunkIdx == Header->NumChunks)
		{
			NextWriteChunkIdx = 0;
		}

		// Wait until all readers have finished with the chunk, and we can update the writer to match
		FChunkStatePtr& NextWriteChunkStatePtr = Header->Chunks[NextWriteChunkIdx];
		for (;;)
		{
			FChunkState NextWriteChunkState = NextWriteChunkStatePtr.Get();
			if (NextWriteChunkState.GetReaderFlags() != 0)
			{
				if (!WriterEvent.Wait(TimeoutMs))
				{
					return nullptr;
				}
				WriterEvent.Reset();
			}
			else if (NextWriteChunkStatePtr.TryUpdate(NextWriteChunkState, FChunkState(EWriteState::Writing, WriterState.GetReaderFlags(), 0)))
			{
				if (Header->Writer.TryUpdate(WriterState, FWriterState(NextWriteChunkIdx, WriterState.GetReaderFlags(), NextWriteChunkIdx == 0)))
				{
					break;
				}
				else
				{
					WriterState = Header->Writer.Get();
				}
			}
		}
		return ChunkPtrs[NextWriteChunkIdx];
	}

private:
	FComputeMemoryMappedFile MemoryMappedFile;

	static void InitShared(std::shared_ptr<FComputeBufferDetail> Detail)
	{
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




//// FComputeBufferReaderDetail /////

struct FComputeBufferReaderDetail
{
	std::shared_ptr<FComputeBufferDetail> Buffer;
	FComputeBufferDetail::FReaderState ReaderState;

	FComputeBufferReaderDetail(std::shared_ptr<FComputeBufferDetail> InBuffer, int InReaderIdx)
		: Buffer(InBuffer)
		, ReaderState(InReaderIdx)
	{
	}

	~FComputeBufferReaderDetail()
	{
		Buffer->ReleaseReader(ReaderState.ReaderIdx);
	}

	bool IsComplete() const
	{
		return Buffer->IsComplete(ReaderState);
	}

	void Detach()
	{
		ReaderState.Detached = true;
		Buffer->ReaderEvents[ReaderState.ReaderIdx].Set();
	}

	void AdvanceReadPosition(size_t Size)
	{
		ReaderState.Offset += (long)Size;
	}

	size_t GetMaxReadSize() const
	{
		return Buffer->GetMaxReadSize(ReaderState);
	}

	const unsigned char* WaitToRead(size_t MinSize, int TimeoutMs)
	{
		return Buffer->WaitToRead(ReaderState, MinSize, TimeoutMs);
	}
};





//// FComputeBuffer /////

FComputeBuffer::FComputeBuffer()
	: Detail(nullptr)
{
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

FComputeBufferReader FComputeBuffer::CreateReader()
{
	int ReaderIdx = Detail->AllocateReader(0, 0);
	if (ReaderIdx == -1)
	{
		assert(false);
		return FComputeBufferReader();
	}
	return FComputeBufferReader(std::make_shared<FComputeBufferReaderDetail>(Detail, ReaderIdx));
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
{
}

FComputeBufferReader::FComputeBufferReader(std::shared_ptr<FComputeBufferReaderDetail> Detail)
	: Detail(std::move(Detail))
{
}

FComputeBufferReader::~FComputeBufferReader()
{
	Close();
}

void FComputeBufferReader::Close()
{
	Detail.reset();
}

void FComputeBufferReader::Detach()
{
	Detail->Detach();
}

bool FComputeBufferReader::IsComplete() const
{
	return Detail->IsComplete();
}

void FComputeBufferReader::AdvanceReadPosition(size_t Size)
{
	Detail->AdvanceReadPosition(Size);
}

size_t FComputeBufferReader::GetMaxReadSize() const
{
	return Detail->GetMaxReadSize();
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
	return Detail->WaitToRead(MinSize, TimeoutMs);
}

const wchar_t* FComputeBufferReader::GetName() const
{
	return Detail->Buffer->Name;
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
