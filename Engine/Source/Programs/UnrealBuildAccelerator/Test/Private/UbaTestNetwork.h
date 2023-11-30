// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBinaryReaderWriter.h"
#include "UbaPlatform.h"
#include "UbaNetworkClient.h"
#include "UbaNetworkServer.h"
#include "UbaNetworkMessage.h"
#include "UbaNetworkBackendTcp.h"

namespace uba
{
	bool TestSockets(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		static constexpr u16 port = 1346;
		u8 result = 0;
		NetworkBackendTcp tcp(logger.m_writer);
		Thread t([&]()
			{
				logger.Info(TC("Starting to Listen"));
				tcp.StartListen(logger, port, TC("127.0.0.1"), [&](void* connection, const sockaddr& remoteSocketAddr)
					{
						logger.Info(TC("Listen got connection"));
						tcp.SetRecvCallbacks(connection, &result, 1,
							[](void* context, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
							{
								*(u8*)context = *headerData;
								//wprintf(TC("Listen got data from peer: %u\n"), *headerData);
								return true;
							},
							nullptr, TC(""));
						return true;
					});
				return 0;
			});

		Sleep(100);
		logger.Info(TC("Starting to Connect"));
		tcp.Connect(logger, TC("127.0.0.1"), [&](void* connection, const sockaddr& remoteSocketAddr, bool* timedOut)
			{
				tcp.SetRecvCallbacks(connection, &tcp, 1, [](void* context, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize) { return true; }, nullptr, TC(""));
				u8 b = 42;
				NetworkBackend::SendContext sc;
				tcp.Send(logger, connection, &b, 1, sc);
				return true;
			}, port);

		Sleep(200);
		t.Wait();

		if (result != 42)
			return logger.Error(TC("Failed to receive data"));

		return true;
	}

	bool TestClientServer(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		auto& logWriter = logger.m_writer;
		NetworkBackendTcp serverTcp(logWriter, TC("ServerTcp"));
		NetworkBackendTcp clientTcp(logWriter, TC("ClientTcp"));

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, logWriter);
		NetworkClient client(ctorSuccess, logWriter);

		server.RegisterService(1, [&](const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
			{
				logger.Info(TC("Got ping!"));
				UBA_ASSERT(messageType == SessionMessageType_Ping);
				writer.WriteByte(42);
				return true;
			});

		if (!server.StartListen(serverTcp, 1234))
			return logger.Error(TC("Failed to listen"));
		Sleep(100);
		if (!client.Connect(clientTcp, TC("127.0.0.1"), 1234))
			return logger.Error(TC("Failed to connect"));


		StackBinaryWriter<128> writer;
		NetworkMessage msg(client, 1, SessionMessageType_Ping, writer);
		writer.WriteU32(32);
		StackBinaryReader<32> reader;
		if (!msg.Send(reader))
			return logger.Error(TC("Failed to get message"));

		u8 value = reader.ReadByte();
		logger.Info(TC("Got value %u"), value);
		return true;
	}

	bool TestClientServer2(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		auto& logWriter = logger.m_writer;
		NetworkBackendTcp serverTcp(logWriter, TC("ServerTcp"));
		NetworkBackendTcp clientTcp(logWriter, TC("ClientTcp"));

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, logWriter);
		NetworkClient client(ctorSuccess, logWriter);

		server.RegisterService(1, [&](const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
			{
				logger.Info(TC("Got ping!"));
				UBA_ASSERT(messageType == SessionMessageType_Ping);
				writer.WriteByte(42);
				return true;
			});

		if (!client.StartListen(clientTcp, 1239))
			return logger.Error(TC("Client failed to listen"));
		Sleep(100);
		if (!server.AddClient(serverTcp, TC("127.0.0.1"), 1239))
			return logger.Error(TC("Server failed to connect"));

		u64 time = GetTime();
		while (!client.GetConnectionCount())
		{
			if (TimeToMs(GetTime() - time) > 4000)
				return logger.Error(TC("Client failed to establish connection"));
			Sleep(100);
		}

		StackBinaryWriter<128> writer;
		NetworkMessage msg(client, 1, SessionMessageType_Ping, writer);
		writer.WriteU32(32);
		StackBinaryReader<32> reader;
		if (!msg.Send(reader))
			return logger.Error(TC("Failed to get message"));

		u8 value = reader.ReadByte();
		logger.Info(TC("Got value %u"), value);
		return true;
	}
}
