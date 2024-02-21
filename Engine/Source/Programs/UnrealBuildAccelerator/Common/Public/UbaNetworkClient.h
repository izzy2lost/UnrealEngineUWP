// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaCrypto.h"
#include "UbaLogger.h"
#include "UbaNetwork.h"
#include "UbaWorkManager.h"
#include "UbaTimer.h"
#include "UbaEvent.h"

namespace uba
{
	class NetworkBackend;
	class NetworkBackendTcp;
	struct BinaryReader;
	struct BinaryWriter;
	struct CasKey;
	struct NetworkMessage;

	struct NetworkClientCreateInfo
	{
		NetworkClientCreateInfo(LogWriter& w = g_consoleLogWriter) : logWriter(w) {}
		LogWriter& logWriter;
		u32 sendSize = SendDefaultSize;
		u32 receiveTimeoutSeconds = 0;
		const u8* cryptoKey128 = nullptr;
	};

	class NetworkClient final : public WorkManagerImpl
	{
	public:
		NetworkClient(bool& outCtorSuccess, const NetworkClientCreateInfo& info = {}, const tchar* name = TC("UbaClient"));
		~NetworkClient();

		bool Connect(NetworkBackend& backend, const tchar* ip, u16 port = DefaultPort, bool* timedOut = nullptr);
		void Disconnect();

		bool StartListen(NetworkBackend& backend, u16 port = DefaultPort);
		void StopListen();
		bool SetConnectionCount(u32 count);
		bool SendKeepAlive();

		bool IsConnected(u32 waitTimeoutMs = 0);
		bool IsOrWasConnected(u32 waitTimeoutMs = 0);

		void PrintSummary(Logger& logger);

		using OnConnectedFunction = Function<void()>;
		void RegisterOnConnected(const OnConnectedFunction& function);

		using OnDisconnectedFunction = Function<void()>;
		void RegisterOnDisconnected(const OnDisconnectedFunction& function);

		using OnVersionMismatchFunction = Function<void(const CasKey& exeKey, const CasKey& dllKey)>;
		void RegisterOnVersionMismatch(const OnVersionMismatchFunction& function);
		void InvokeVersionMismatch(const CasKey& exeKey, const CasKey& dllKey);

		u64 GetMessageHeaderSize();
		u64 GetMessageMaxSize();
		u64 GetMessageReceiveHeaderSize();
		const Guid& GetUid() { return m_uid; }
		LogWriter& GetLogWriter() { return m_logWriter; }
		NetworkBackendTcp& GetTcpBackend() { return *m_tcpBackend; }
		u32 GetConnectionCount() { return m_connectionCount; }

	private:
		struct Connection
		{
			Connection(NetworkClient& o) : owner(o) {}
			NetworkClient& owner;
			void* backendConnection = nullptr;
			Atomic<u32> connected;
			Timer sendTimer;
			Atomic<u64> sendBytes;
			u64 recvBytes = 0;
			u32 recvCount = 0;
			NetworkBackend* backend = nullptr;
		};

		bool AddConnection(NetworkBackend& backend, void* backendConnection, bool* timedOut);
		void ConnectedCallback(NetworkBackend& backend, void* backendConnection);
		static bool ReceiveResponseHeader(void* context, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize);
		static bool ReceiveResponseBody(void* context, bool recvError, u8* headerData, void* bodyContext, u8* bodyData, u32 bodySize);
		void OnDisconnected(Connection& connection, bool calledFromReceive);
		bool Send(NetworkMessage& message, void* response, u32 responseCapacity, bool async);
		void ReturnMessageId(u16 id);
		const tchar* SetGetPrefix(const tchar* originalPrefix);

		LogWriter& m_logWriter;
		Guid m_uid;
		TString m_prefix;
		LoggerWithWriter m_logger;
		u32 m_sendSize;
		u32 m_receiveTimeoutSeconds;

		ReaderWriterLock m_serverUidLock;
		Guid m_serverUid;

		Event m_isConnected;
		Event m_isOrWasConnected;
		Atomic<u32> m_connectionCount;
		ReaderWriterLock m_onConnectedFunctionsLock;
		Vector<OnConnectedFunction> m_onConnectedFunctions;
		Vector<OnDisconnectedFunction> m_onDisconnectedFunctions;
		OnVersionMismatchFunction m_versionMismatchFunction;

		NetworkBackendTcp* m_tcpBackend; // Used by proxies
		NetworkBackend* m_listenBackend = nullptr;

		ReaderWriterLock m_connectionsLock;
		List<Connection> m_connections;
		ReaderWriterLock m_connectionsItLock;
		List<Connection>::iterator m_connectionsIt;


		ReaderWriterLock m_activeMessagesLock;
		u16 m_activeMessageIdMax = 1;
		Vector<u16> m_availableMessageIds;
		Vector<NetworkMessage*> m_activeMessages;

		CryptoKey m_cryptoKey = InvalidCryptoKey;
		Timer m_encryptTimer;
		Timer m_decryptTimer;

		friend NetworkMessage;
	};
}
