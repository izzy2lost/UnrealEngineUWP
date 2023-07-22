// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeTransport.h"

bool FComputeTransport::SendMessage(const void* Data, size_t Size)
{
	const unsigned char* RemainingData = (const unsigned char*)Data;
	for (size_t RemainingSize = Size; RemainingSize > 0; )
	{
		size_t SentSize = Send(RemainingData, RemainingSize);
		if (SentSize == 0)
		{
			return false;
		}

		RemainingData += SentSize;
		RemainingSize -= SentSize;
	}
	return true;
}

bool FComputeTransport::RecvMessage(void* Data, size_t Size)
{
	unsigned char* RemainingData = (unsigned char*)Data;
	for (size_t RemainingSize = Size; RemainingSize > 0; )
	{
		size_t RecvSize = Recv(RemainingData, RemainingSize);
		if (RecvSize == 0)
		{
			return false;
		}

		RemainingData += RecvSize;
		RemainingSize -= RecvSize;
	}
	return true;
}

///////////////////////////////////

FBufferTransport::FBufferTransport(FComputeBuffer InSendBuffer, FComputeBuffer InRecvBuffer)
	: SendBuffer(std::move(InSendBuffer))
	, RecvBuffer(std::move(InRecvBuffer))
{
}

size_t FBufferTransport::Send(const void* Data, size_t Size)
{
	FComputeBufferWriter& Writer = SendBuffer.GetWriter();
	unsigned char* Buffer = Writer.WaitToWrite(1);

	size_t WriteSize = std::min(Size, SendBuffer.GetWriter().GetMaxWriteSize());
	memcpy(Buffer, Data, WriteSize);
	Writer.AdvanceWritePosition(WriteSize);

	return WriteSize;
}

size_t FBufferTransport::Recv(void* Data, size_t Size)
{
	FComputeBufferReader& Reader = RecvBuffer.GetReader();
	const unsigned char* Buffer = Reader.WaitToRead(1);

	size_t ReadSize = std::min(Size, Reader.GetMaxReadSize());
	memcpy(Data, Buffer, ReadSize);
	Reader.AdvanceReadPosition(ReadSize);

	return ReadSize;
}

void FBufferTransport::MarkComplete()
{
	SendBuffer.GetWriter().MarkComplete();
}

void FBufferTransport::Close()
{
	SendBuffer.GetWriter().MarkComplete();
	RecvBuffer.GetWriter().MarkComplete();
}
