// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeChannel.h"
#include <string.h>
#include <stdio.h>
#include "ComputeSocket.h"

FComputeChannel::FComputeChannel()
{
}

FComputeChannel::FComputeChannel(FComputeBufferReader InReader, FComputeBufferWriter InWriter)
	: Reader(std::move(InReader))
	, Writer(std::move(InWriter))
{
}

FComputeChannel::~FComputeChannel()
{
}

bool FComputeChannel::IsValid() const
{
	return Reader.IsValid();
}

size_t FComputeChannel::Send(const void* Data, size_t Size, int TimeoutMs)
{
	return Writer.Write(Data, Size, TimeoutMs);
}

size_t FComputeChannel::Recv(void* Data, size_t Size, int TimeoutMs)
{
	return Reader.Read(Data, Size, TimeoutMs);
}

void FComputeChannel::MarkComplete()
{
	Writer.MarkComplete();
}
