// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeBuffer.h"
#include "ComputeSocket.h"
#include <assert.h>
#include <iostream>

#define verify(x) ((x) || (throw #x, true))

void RunTests()
{
	void ComputeBufferTest();
	ComputeBufferTest();

	void ComputeSocketTest();
	ComputeSocketTest();
}

void ComputeBufferTest()
{
	char TestData[] = "hello world!";

	FComputeBuffer Buffer;
	verify(Buffer.CreateNew(FComputeBuffer::FParams()));

	FComputeBufferWriter& Writer = Buffer.GetWriter();
	unsigned char* WriteBuffer = Writer.WaitToWrite(sizeof(TestData));
	memcpy(WriteBuffer, TestData, sizeof(TestData));
	Writer.AdvanceWritePosition(sizeof(TestData));

	FComputeBufferReader Reader = Buffer.CreateReader();
	const unsigned char* ReadBuffer = Reader.WaitToRead(sizeof(TestData));
	verify(memcmp(ReadBuffer, TestData, sizeof(TestData)) == 0);
}

template<size_t TestDataSize> void CheckChannelSendRecv(FComputeChannel& SendChannel, FComputeChannel& RecvChannel, const char(&TestData)[TestDataSize])
{
	SendChannel.Send(TestData, TestDataSize);

	char RecvTestData[TestDataSize];
	verify(RecvChannel.Recv(RecvTestData, TestDataSize) == TestDataSize);

	verify(memcmp(TestData, RecvTestData, TestDataSize) == 0);
}

void ComputeSocketTest()
{
	// Buffers for transferring between client and server
	FComputeBuffer ClientToServerBuffer;
	verify(ClientToServerBuffer.CreateNew(FComputeBuffer::FParams()));

	FComputeBuffer ServerToClientBuffer;
	verify(ServerToClientBuffer.CreateNew(FComputeBuffer::FParams()));

	// Client transport
	std::unique_ptr<FComputeSocket> ClientSocket = CreateComputeSocket(std::make_unique<FBufferTransport>(ClientToServerBuffer, ServerToClientBuffer), EComputeSocketEndpoint::Local);
	FComputeChannel ClientChannel1 = ClientSocket->CreateChannel(1);
	FComputeChannel ClientChannel2 = ClientSocket->CreateChannel(2);

	// Server socket
	std::unique_ptr<FComputeSocket> ServerSocket = CreateComputeSocket(std::make_unique<FBufferTransport>(ServerToClientBuffer, ClientToServerBuffer), EComputeSocketEndpoint::Remote);
	FComputeChannel ServerChannel1 = ServerSocket->CreateChannel(1);
	FComputeChannel ServerChannel2 = ServerSocket->CreateChannel(2);

	// Send data over channel 1
	CheckChannelSendRecv(ClientChannel1, ServerChannel1, "Channel 1: Client -> Server");

	CheckChannelSendRecv(ServerChannel2, ClientChannel2, "Channel 2: Server -> Client");
	CheckChannelSendRecv(ClientChannel2, ServerChannel2, "Channel 2: Client -> Server");

	// Send data over channel 1 again
	CheckChannelSendRecv(ClientChannel1, ServerChannel1, "Channel 1: Client -> Server (2)");
	CheckChannelSendRecv(ServerChannel1, ClientChannel1, "Channel 1: Server -> Client (2)");
}
