// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeSocket.h"
#include "ComputePlatform.h"
#include <iostream>
#include <assert.h>
#include <uchar.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>

FComputeSocket::FComputeSocket()
{
}

FComputeSocket::~FComputeSocket()
{
}

FComputeChannel FComputeSocket::CreateChannel(int ChannelId)
{
	FComputeBuffer RecvBuffer;
	if (!RecvBuffer.CreateNew(FComputeBuffer::FParams()))
	{
		return FComputeChannel();
	}

	FComputeBuffer SendBuffer;
	if (!SendBuffer.CreateNew(FComputeBuffer::FParams()))
	{
		return FComputeChannel();
	}

	return CreateChannel(ChannelId, RecvBuffer, SendBuffer);
}

FComputeChannel FComputeSocket::CreateChannel(int ChannelId, FComputeBuffer RecvBuffer, FComputeBuffer SendBuffer)
{
	AttachRecvBuffer(ChannelId, RecvBuffer.GetWriter());
	AttachSendBuffer(ChannelId, SendBuffer.CreateReader());

	return FComputeChannel(RecvBuffer.CreateReader(), SendBuffer.GetWriter());
}

//////////////////////////////////////////////////////

const wchar_t* const FWorkerComputeSocket::IpcEnvVar = L"UE_HORDE_COMPUTE_IPC";

enum class FWorkerComputeSocket::EMessageType
{
	AttachRecvBuffer = 0,
	AttachSendBuffer = 1,
};

FWorkerComputeSocket::FWorkerComputeSocket()
{
}

FWorkerComputeSocket::~FWorkerComputeSocket()
{
	Close();
}

bool FWorkerComputeSocket::Open()
{
	wchar_t EnvVar[FComputeBuffer::MaxNameLength];
	if (!FComputePlatform::GetEnvironmentVariable(IpcEnvVar, EnvVar, sizeof(EnvVar) / sizeof(EnvVar[0])))
	{
		return false;
	}

	return Open(EnvVar);
}

bool FWorkerComputeSocket::Open(const wchar_t* CommandBufferName)
{
	return CommandBuffer.OpenExisting(CommandBufferName);
}

void FWorkerComputeSocket::Close()
{
	CommandBuffer.Close();
}

void FWorkerComputeSocket::AttachRecvBuffer(int ChannelId, FComputeBufferWriter Writer)
{
	AttachBuffer(ChannelId, EMessageType::AttachRecvBuffer, Writer.GetName());
}

void FWorkerComputeSocket::AttachSendBuffer(int ChannelId, FComputeBufferReader Reader)
{
	AttachBuffer(ChannelId, EMessageType::AttachSendBuffer, Reader.GetName());
}

void FWorkerComputeSocket::AttachBuffer(int ChannelId, EMessageType Type, const wchar_t* Name)
{
	FComputeBufferWriter& Writer = CommandBuffer.GetWriter();
	unsigned char* Data = Writer.WaitToWrite(1024);

	size_t Len = 0;
	Len += WriteVarUInt(Data + Len, (unsigned char)Type);
	Len += WriteVarUInt(Data + Len, (unsigned int)ChannelId);
	Len += WriteString(Data + Len, Name);

	Writer.AdvanceWritePosition(Len);
}

void FWorkerComputeSocket::RunServer(FComputeBufferReader& CommandBufferReader, FComputeSocket& Socket)
{
	const unsigned char* Message;
	while ((Message = CommandBufferReader.WaitToRead(1)) != nullptr)
	{
		size_t Len = 0;

		unsigned int Type;
		Len += ReadVarUInt(Message + Len, &Type);

		EMessageType MessageType = (EMessageType)*Message;
		switch (MessageType)
		{
		case EMessageType::AttachSendBuffer:
			{
				unsigned int ChannelId;
				Len += ReadVarUInt(Message + Len, &ChannelId);

				wchar_t Name[FComputeBuffer::MaxNameLength];
				Len += ReadString(Message + Len, Name, FComputeBuffer::MaxNameLength);

				FComputeBuffer Buffer;
				if (Buffer.OpenExisting(Name))
				{
					Socket.AttachSendBuffer(ChannelId, Buffer.CreateReader());
				}
				else
				{
					assert(false);
				}
			}
			break;
		case EMessageType::AttachRecvBuffer:
			{
				unsigned int ChannelId;
				Len += ReadVarUInt(Message + Len, &ChannelId);

				wchar_t Name[FComputeBuffer::MaxNameLength];
				Len += ReadString(Message + Len, Name, FComputeBuffer::MaxNameLength);

				FComputeBuffer Buffer;
				if (Buffer.OpenExisting(Name))
				{
					Socket.AttachRecvBuffer(ChannelId, Buffer.GetWriter());
				}
				else
				{
					assert(false);
				}
			}
			break;
		default:
			assert(false);
			return;
		}

		CommandBufferReader.AdvanceReadPosition(Len);
	}
}

size_t FWorkerComputeSocket::ReadVarUInt(const unsigned char* Pos, unsigned int* OutValue)
{
	size_t ByteCount = FComputePlatform::CountLeadingZeros((unsigned char)(~*static_cast<const unsigned char*>(Pos))) - 23;

	unsigned int Value = *Pos++ & (unsigned char)(0xff >> ByteCount);
	switch (ByteCount - 1)
	{
	case 8: Value <<= 8; Value |= *Pos++;
	case 7: Value <<= 8; Value |= *Pos++;
	case 6: Value <<= 8; Value |= *Pos++;
	case 5: Value <<= 8; Value |= *Pos++;
	case 4: Value <<= 8; Value |= *Pos++;
	case 3: Value <<= 8; Value |= *Pos++;
	case 2: Value <<= 8; Value |= *Pos++;
	case 1: Value <<= 8; Value |= *Pos++;
	default:
		break;
	}

	*OutValue = Value;
	return ByteCount;
}

size_t FWorkerComputeSocket::ReadString(const unsigned char* Pos, wchar_t* OutText, size_t OutTextMaxLen)
{
	unsigned int TextLen;
	size_t Len = ReadVarUInt(Pos, &TextLen);

	FComputePlatform::Utf8ToWchar((const char*)Pos + Len, TextLen, OutText, OutTextMaxLen);

	return Len + TextLen;
}

size_t FWorkerComputeSocket::WriteVarUInt(unsigned char* Pos, unsigned int Value)
{
	// Use BSR to return the log2 of the integer
	// return 0 if value is 0
	unsigned int ByteCount = (unsigned int)(int(FComputePlatform::FloorLog2(Value)) / 7 + 1);

	unsigned char* OutBytes = Pos + ByteCount - 1;
	switch (ByteCount - 1)
	{
	case 4: *OutBytes-- = (unsigned char)(Value); Value >>= 8;
	case 3: *OutBytes-- = (unsigned char)(Value); Value >>= 8;
	case 2: *OutBytes-- = (unsigned char)(Value); Value >>= 8;
	case 1: *OutBytes-- = (unsigned char)(Value); Value >>= 8;
	default: break;
	}
	*OutBytes = (unsigned char)(0xff << (9 - ByteCount)) | (unsigned char)(Value);

	return ByteCount;
}

size_t FWorkerComputeSocket::WriteString(unsigned char* Pos, const wchar_t* Text)
{
	size_t TextLen = wcslen(Text);

	size_t EncodedLen = FComputePlatform::WcharToUtf8(Text, TextLen, nullptr, 0);

	size_t Len = WriteVarUInt(Pos, (int)EncodedLen);
	FComputePlatform::WcharToUtf8(Text, TextLen, (char*)Pos + Len, EncodedLen);

	return Len + EncodedLen;
}

//////////////////////////////////////////////////////

class FRemoteComputeSocket : public FComputeSocket
{
public:
	enum class EControlMessageType
	{
		Detach = -2,
	};

	struct FFrameHeader
	{
		int Channel;
		int Size;
	};

	std::unique_ptr<FComputeTransport> Transport;
	const EComputeSocketEndpoint Endpoint;
	std::mutex CriticalSection;

	std::thread RecvThread;

	std::unordered_map<int, FComputeBufferWriter> Writers;
	std::vector<FComputeBufferReader> Readers;
	std::unordered_map<int, std::thread> SendThreads;

	FRemoteComputeSocket(std::unique_ptr<FComputeTransport> InTransport, EComputeSocketEndpoint InEndpoint)
		: Transport(std::move(InTransport))
		, Endpoint(InEndpoint)
		, CriticalSection()
		, RecvThread(&FRemoteComputeSocket::RecvThreadProc, this)
	{
	}

	~FRemoteComputeSocket()
	{
		for (FComputeBufferReader& Reader : Readers)
		{
			Reader.Detach();
		}

		for (std::pair<const int, std::thread>& Pair : SendThreads)
		{
			Pair.second.join();
		}

		Transport->Close();
		RecvThread.join();
	}

	void RecvThreadProc()
	{
		std::unordered_map<int, FComputeBufferWriter> CachedWriters;

		// Process messages from the remote
		FFrameHeader Header;
		while (Transport->RecvMessage(&Header, sizeof(Header)))
		{
			if (Header.Size >= 0)
			{
				ReadFrame(CachedWriters, Header.Channel, Header.Size);
			}
			else if (Header.Size == (int)EControlMessageType::Detach)
			{
				DetachRecvBuffer(CachedWriters, Header.Channel);
			}
			else
			{
				assert(false);
			}
		}
	}

	void SendThreadProc(int Channel, FComputeBufferReader Reader)
	{
		FFrameHeader Header;
		Header.Channel = Channel;

		const unsigned char* Data;
		while ((Data = Reader.WaitToRead(1)) != nullptr)
		{
			std::lock_guard<std::mutex> Lock(CriticalSection);
			Header.Size = (int)Reader.GetMaxReadSize();
			Transport->SendMessage(&Header, sizeof(Header));
			Transport->SendMessage(Data, Header.Size);
			Reader.AdvanceReadPosition(Header.Size);
		}

		if (Reader.IsComplete())
		{
			std::lock_guard<std::mutex> Lock(CriticalSection);
			Header.Size = (int)EControlMessageType::Detach;
			Transport->SendMessage(&Header, sizeof(Header));
		}
	}

	bool ReadFrame(std::unordered_map<int, FComputeBufferWriter>& CachedWriters, int Channel, int Size)
	{
		std::unordered_map<int, FComputeBufferWriter>::iterator Iter = CachedWriters.find(Channel);
		if (Iter == CachedWriters.end())
		{
			std::lock_guard<std::mutex> Lock(CriticalSection);

			Iter = Writers.find(Channel);
			if (Iter == Writers.end())
			{
				return false;
			}

			Iter = CachedWriters.insert(*Iter).first;
		}

		FComputeBufferWriter& Writer = Iter->second;

		unsigned char* Data = Writer.WaitToWrite(Size);
		if (!Transport->RecvMessage(Data, Size))
		{
			return false;
		}

		Writer.AdvanceWritePosition(Size);
		return true;
	}

	void AttachRecvBuffer(int ChannelId, FComputeBufferWriter Writer)
	{
		std::lock_guard<std::mutex> Lock(CriticalSection);
		Writers.insert(std::pair<int, FComputeBufferWriter>(ChannelId, std::move(Writer)));
	}

	void AttachSendBuffer(int ChannelId, FComputeBufferReader Reader)
	{
		std::lock_guard<std::mutex> Lock(CriticalSection);
		Readers.push_back(Reader);
		SendThreads.insert(std::make_pair(ChannelId, std::thread(&FRemoteComputeSocket::SendThreadProc, this, ChannelId, std::move(Reader))));
	}

	void DetachRecvBuffer(std::unordered_map<int, FComputeBufferWriter>& CachedWriters, int Channel)
	{
		CachedWriters.erase(Channel);

		std::lock_guard<std::mutex> Lock(CriticalSection);

		std::unordered_map<int, FComputeBufferWriter>::iterator Iter = Writers.find(Channel);
		if (Iter != Writers.end())
		{
			Iter->second.MarkComplete();
			Writers.erase(Iter);
		}
	}
};

std::unique_ptr<FComputeSocket> CreateComputeSocket(std::unique_ptr<FComputeTransport> Transport, EComputeSocketEndpoint Endpoint)
{
	return std::unique_ptr<FComputeSocket>(new FRemoteComputeSocket(std::move(Transport), Endpoint));
}
