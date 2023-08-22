// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeBuffer.h"
#include "ComputeChannel.h"
#include "ComputeTransport.h"
#include <vector>

//
// Connection to a remote machine that multiplexes data into and out-of multiple buffers
// attached to different channel numbers.
//
class FComputeSocket
{
public:
	FComputeSocket();
	virtual ~FComputeSocket();

	FComputeSocket(const FComputeSocket&) = delete;
	FComputeSocket& operator=(const FComputeSocket&) = delete;

	// Attaches a new buffer for receiving data
	virtual void AttachRecvBuffer(int ChannelId, FComputeBuffer RecvBuffer) = 0;

	// Attaches a new buffer for sending data */
	virtual void AttachSendBuffer(int ChannelId, FComputeBuffer SendBuffer) = 0;

	// Attaches a channel to this socket
	FComputeChannel CreateChannel(int ChannelId);

	// Attaches a channel to this socket
	FComputeChannel CreateChannel(int ChannelId, FComputeBuffer RecvBuffer, FComputeBuffer SendBuffer);
};

//
// Socket used by a worker process to communicate with a host running on the same machine
// using shared memory to attach new buffers.
//
class FWorkerComputeSocket final : public FComputeSocket
{
public:
	static const char* const IpcEnvVar;

	FWorkerComputeSocket();
	~FWorkerComputeSocket();

	// Opens a connection to the agent process using a command buffer read from an environment variable (EnvVarName)
	bool Open();

	// Opens a connection to the agent process using a specific command buffer name
	bool Open(const char* CommandBufferName);

	// Close the current connection
	void Close();

	// Attaches a new buffer for receiving data
	virtual void AttachRecvBuffer(int ChannelId, FComputeBuffer RecvBuffer) override;

	// Attaches a new buffer for sending data
	virtual void AttachSendBuffer(int ChannelId, FComputeBuffer SendBuffer) override;

	// Reads and handles a command from the command buffer
	static void RunServer(FComputeBufferReader& CommandBufferReader, FComputeSocket& Socket);

private:
	enum class EMessageType;

	FComputeBufferWriter CommandBufferWriter;
	std::vector<FComputeBuffer> Buffers;

	void AttachBuffer(int ChannelId, EMessageType Type, const char* Name);

	static size_t ReadVarUInt(const unsigned char* Pos, unsigned int* OutValue);
	static size_t ReadString(const unsigned char* Pos, char* OutText, size_t OutTextMaxLen);

	static size_t WriteVarUInt(unsigned char* Pos, unsigned int Value);
	static size_t WriteString(unsigned char* Pos, const char* Text);
};

//
// Enum identifying which end of the socket a particular machine is
//
enum class EComputeSocketEndpoint
{
	// The initiating machine
	Local,

	// The remote machine
	Remote
};

// Creates a socket using a custom transport
std::unique_ptr<FComputeSocket> CreateComputeSocket(std::unique_ptr<FComputeTransport> Transport, EComputeSocketEndpoint Endpoint);
