// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/ComputeBuffer.h"
#include "Compute/ComputeSocket.h"
#include "Compute/ComputePlatform.h"
#include <assert.h>
#include <iostream>

void RunTests()
{
	void ComputeBufferTest();
	ComputeBufferTest();
	std::cout << "ComputeBufferTest ok" << std::endl;

	void ComputeSocketTest();
	ComputeSocketTest();
	std::cout << "ComputeSocketTest ok" << std::endl;
}

void ComputeBufferTest()
{
	char TestData[] = "hello world!";

	FComputeBuffer Buffer;
	UE_COMPUTE_VERIFY(Buffer.CreateNew(FComputeBuffer::FParams()));

	FComputeBufferWriter Writer = Buffer.CreateWriter();
	unsigned char* WriteBuffer = Writer.WaitToWrite(sizeof(TestData));
	memcpy(WriteBuffer, TestData, sizeof(TestData));
	Writer.AdvanceWritePosition(sizeof(TestData));

	FComputeBufferReader Reader = Buffer.CreateReader();
	const unsigned char* ReadBuffer = Reader.WaitToRead(sizeof(TestData));
	UE_COMPUTE_ASSERT(memcmp(ReadBuffer, TestData, sizeof(TestData)) == 0);
	UE_COMPUTE_ASSERT(!Reader.IsComplete());

	Reader.AdvanceReadPosition(sizeof(TestData));
	UE_COMPUTE_ASSERT(!Reader.IsComplete());

	Writer.MarkComplete();
	UE_COMPUTE_ASSERT(Reader.IsComplete());
}

template<size_t TestDataSize> void CheckChannelSendRecv(std::shared_ptr<FComputeChannel>& SendChannel, std::shared_ptr<FComputeChannel>& RecvChannel, const char(&TestData)[TestDataSize])
{
	SendChannel->Send(TestData, TestDataSize);

	char RecvTestData[TestDataSize];
	UE_COMPUTE_VERIFY(RecvChannel->Recv(RecvTestData, TestDataSize) == TestDataSize);

	UE_COMPUTE_VERIFY(memcmp(TestData, RecvTestData, TestDataSize) == 0);
}

void ComputeSocketTest()
{
	// Buffers for transferring between client and server
	FComputeBuffer ClientToServerBuffer;
	UE_COMPUTE_VERIFY(ClientToServerBuffer.CreateNew(FComputeBuffer::FParams()));

	FComputeBuffer ServerToClientBuffer;
	UE_COMPUTE_VERIFY(ServerToClientBuffer.CreateNew(FComputeBuffer::FParams()));

	// Client transport
	std::unique_ptr<FComputeSocket> ClientSocket = CreateComputeSocket(std::make_unique<FBufferTransport>(ClientToServerBuffer.CreateWriter(), ServerToClientBuffer.CreateReader()), EComputeSocketEndpoint::Local);
	std::shared_ptr<FComputeChannel> ClientChannel1 = ClientSocket->CreateChannel(1);
	std::shared_ptr<FComputeChannel> ClientChannel2 = ClientSocket->CreateChannel(2);

	// Server socket
	std::unique_ptr<FComputeSocket> ServerSocket = CreateComputeSocket(std::make_unique<FBufferTransport>(ServerToClientBuffer.CreateWriter(), ClientToServerBuffer.CreateReader()), EComputeSocketEndpoint::Remote);
	std::shared_ptr<FComputeChannel> ServerChannel1 = ServerSocket->CreateChannel(1);
	std::shared_ptr<FComputeChannel> ServerChannel2 = ServerSocket->CreateChannel(2);

	// Close the original buffers now that the sockets are set up
	ClientToServerBuffer.Close();
	ServerToClientBuffer.Close();

	// Send data over channel 1
	CheckChannelSendRecv(ClientChannel1, ServerChannel1, "Channel 1: Client -> Server");

	CheckChannelSendRecv(ServerChannel2, ClientChannel2, "Channel 2: Server -> Client");
	CheckChannelSendRecv(ClientChannel2, ServerChannel2, "Channel 2: Client -> Server");

	// Send data over channel 1 again
	CheckChannelSendRecv(ClientChannel1, ServerChannel1, "Channel 1: Client -> Server (2)");
	CheckChannelSendRecv(ServerChannel1, ClientChannel1, "Channel 1: Server -> Client (2)");
}
