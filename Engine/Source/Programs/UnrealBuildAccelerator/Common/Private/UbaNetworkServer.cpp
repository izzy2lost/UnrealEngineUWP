// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaNetworkServer.h"
#include "UbaCrypto.h"
#include "UbaBinaryReaderWriter.h"
#include "UbaPlatform.h"

namespace uba
{
	class NetworkServer::Worker
	{
	public:
		Worker() : m_workAvailable(false) {}
		~Worker()
		{
			m_loop = false;
			m_workAvailable.Set();
		}

		void Start(NetworkServer& server)
		{
			m_loop = true;
			m_thread.Start([&]() { ThreadWorker(server); return 0; });
		}

		void ThreadWorker(NetworkServer& server);
		void DoAdditionalWorkAndSignalAvailable(NetworkServer& server);

		Worker* m_nextWorker = nullptr;
		Worker* m_prevWorker = nullptr;

		Vector<u8> m_buffer;
		Event m_workAvailable;
		Connection* m_connection = nullptr;
		u32 m_dataSize = 0;
		u8 m_serviceId = 0;
		u8 m_messageType = 0;
		u16 m_id = 0;
		bool m_loop = false;
		Thread m_thread;

		Worker(const Worker&) = delete;
	};


	class NetworkServer::Connection
	{
	public:
		Connection(NetworkServer& server, NetworkBackend& backend, void* backendConnection, const sockaddr& remoteSockAddr, CryptoKey cryptoKey)
		:	m_server(server)
		,	m_backend(backend)
		,	m_remoteSockAddr(remoteSockAddr)
		,	m_cryptoKey(cryptoKey)
		,	m_backendConnection(backendConnection)
		{
		}

		~Connection()
		{
			Stop();
			if (m_backendConnection)
			{
				m_backend.SetDisconnectCallback(m_backendConnection, nullptr, nullptr);
				m_backend.Close(m_backendConnection);
			}
			if (m_cryptoKey)
				Crypto::DestroyKey(m_cryptoKey);
		}

		void Start()
		{
			m_activeWorkerCount = 1;

			m_backend.SetDisconnectCallback(m_backendConnection, this, [](void* context, void* connection)
				{
					auto& conn = *(Connection*)context;
					conn.Disconnect();
				});

			m_backend.SetDataSentCallback(m_backendConnection, this, [](void* context, u32 bytes)
				{
					auto& conn = *(Connection*)context;
					if (auto c = conn.m_client)
						c->sendBytes += bytes;
					conn.m_server.m_sendBytes += bytes;
				});

			m_backend.SetRecvTimeout(m_backendConnection, m_server.m_receiveTimeoutMs);

			if (m_cryptoKey)
				m_backend.SetRecvCallbacks(m_backendConnection, this, 0, ReceiveHandshakeHeader, ReceiveHandshakeBody, TC("ReceiveHandshake"));
			else
				m_backend.SetRecvCallbacks(m_backendConnection, this, 4, ReceiveVersion, nullptr, TC("ReceiveVersion"));
		}

		void Disconnect()
		{
			if (m_disconnectCalled.fetch_add(1) != 0)
				return;
			SetShouldDisconnect();
			if (--m_activeWorkerCount == 0) // Will disconnect in send if there are active workers
				TestDisconnect();
		}

		void Stop()
		{
			Disconnect();
				
			u64 startTimer = GetTime();
			while (m_activeWorkerCount)
			{
				if (TimeToMs(GetTime() - startTimer) > 3000)
				{
					m_server.m_logger.Error(TC("Connection has waited 3 seconds to stop... something is stuck"));
					break;
				}
				Sleep(1);
			}
		}

		bool SendAsync(u8 value)
		{
			NetworkBackend::SendContext context(NetworkBackend::SendFlags_Async);
			return m_backend.Send(m_server.m_logger, m_backendConnection, &value, 1, context);
		}

		static bool ReceiveHandshakeHeader(void* context, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
		{
			u8* handshakeData = new u8[sizeof(EncryptionHandshakeString)];
			outBodyData = handshakeData;
			outBodySize = sizeof(EncryptionHandshakeString);
			return true;
		}

		static bool ReceiveHandshakeBody(void* context, bool recvError, u8* headerData, void* bodyContext, u8* bodyData, u32 bodySize)
		{
			auto& conn = *(Connection*)context;
			u8* handshakeData = bodyData;
			auto g = MakeGuard([handshakeData]() { delete[] handshakeData; });

			if (!Crypto::Decrypt(conn.m_server.m_logger, conn.m_cryptoKey, handshakeData, sizeof(EncryptionHandshakeString)))
				return false;

			if (memcmp(handshakeData, EncryptionHandshakeString, sizeof(EncryptionHandshakeString)) != 0)
				return conn.m_server.m_logger.Error(TC("Crypto mismatch..."));

			conn.m_backend.SetRecvCallbacks(conn.m_backendConnection, &conn, 4, ReceiveVersion, nullptr, TC("ReceiveVersion"));

			return true;
		}

		static bool ReceiveVersion(void* context, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
		{
			auto& conn = *(Connection*)context;
			u32 clientVersion = *(u32*)headerData;
			if (clientVersion != SystemNetworkVersion)
			{
				conn.SendAsync(1);
				return false;
			}

			conn.m_backend.SetRecvCallbacks(conn.m_backendConnection, &conn, sizeof(Guid), ReceiveClientUid, nullptr, TC("ReceiveClientUid"));

			return true;
		}

		static bool ReceiveClientUid(void* context, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
		{
			auto& conn = *(Connection*)context;
			auto& server = conn.m_server;

			Guid clientUid = *(Guid*)headerData;

			if (!server.m_allowNewClients)
			{
				ScopedReadLock clientsLock(server.m_clientsLock);
				bool found = false;
				for (auto& kv : server.m_clients)
					found |= kv.second.uid == clientUid;
				if (!found)
				{
					conn.SendAsync(3);
					return false;
				}
			}

			constexpr u32 HeaderSize = 6;
			conn.m_backend.SetRecvCallbacks(conn.m_backendConnection, &conn, HeaderSize, ReceiveMessageHeader, ReceiveMessageBody, TC("ReceiveMessage"));

			if (!conn.SendAsync(0))
				return false;
			
			ScopedWriteLock clientsLock(server.m_clientsLock);
			u32 clientId = u32(server.m_clients.size() + 1);
			for (auto& kv : server.m_clients)
				if (kv.second.uid == clientUid)
					clientId = kv.second.id;
			Client& client = server.m_clients.try_emplace(clientId, clientUid, clientId).first->second;
			clientsLock.Leave();

			conn.m_client = &client;

			Guid connectionUid;
			CreateGuid(connectionUid);

			if (client.connectionCount.fetch_add(1) == 0)
			{
				if (server.m_onConnectionFunction)
					server.m_onConnectionFunction(clientUid, clientId);
				server.m_logger.Info(TC("Client %s connected on connection %s"), GuidToString(clientUid).str, GuidToString(connectionUid).str);
			}
			else
				server.m_logger.Info(TC("Client %s additional connection %s connected"), GuidToString(clientUid).str, GuidToString(connectionUid).str);


			return true;
		}

		static bool ReceiveMessageHeader(void* context, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
		{
			auto& conn = *(Connection*)context;

			u8 serviceIdAndMessageType = headerData[0];
			u8 serviceId = serviceIdAndMessageType >> 6;
			u8 messageType = serviceIdAndMessageType & 0b111111;
			u16 messageId = u16(headerData[1] << 8) | u16((*(u32*)(headerData + 2) & 0xff000000) >> 24);
			u32 messageSize = *(u32*)(headerData + 2) & 0x00ffffff;
			UBA_ASSERT(messageSize <= SendMaxSize);

			//m_logger.Debug(TC("Recv: %u, %u, %u, %u"), serviceId, messageType, id, size);
			Worker* worker = conn.m_server.PopWorker();
			worker->m_id = messageId;
			worker->m_serviceId = serviceId;
			worker->m_messageType = messageType;
			worker->m_dataSize = messageSize;
			worker->m_connection = &conn;// this;
			if (worker->m_buffer.size() < messageSize)
				worker->m_buffer.resize(size_t(Min(messageSize + 1024u, SendMaxSize)));
			outBodyContext = worker;
			outBodyData = worker->m_buffer.data();
			outBodySize = messageSize;
			return true;
		}

		static bool ReceiveMessageBody(void* context, bool recvError, u8* headerData, void* bodyContext, u8* bodyData, u32 bodySize)
		{
			auto& conn = *(Connection*)context;
			auto worker = (Worker*)bodyContext;

			if (recvError)
			{
				conn.m_server.PushWorker(worker);
				return false;
			}

			conn.m_client->recvBytes += worker->m_dataSize;
			conn.m_server.m_recvBytes += worker->m_dataSize;
			++conn.m_server.m_recvCount;

			++conn.m_activeWorkerCount;
			worker->m_workAvailable.Set();
			return true;
		}

		void Send(const void* data, u32 bytes)
		{
			TimerScope ts(m_sendTimer);
			NetworkBackend::SendContext context;
			if (!m_backend.Send(m_server.m_logger, m_backendConnection, data, bytes, context))
				SetShouldDisconnect();
		}

		bool SetShouldDisconnect()
		{
			ScopedWriteLock lock(m_shutdownLock);
			bool isConnected = !m_shouldDisconnect;
			m_shouldDisconnect = true;
			return isConnected;
		}

		void Release()
		{
			if (--m_activeWorkerCount == 0)
				TestDisconnect();
		}

		void TestDisconnect()
		{
			ScopedWriteLock lock(m_shutdownLock);
			if (!m_shouldDisconnect)
				return;
			if (m_disconnected)
				return;
			m_backend.Shutdown(m_backendConnection);
			lock.Leave();
			if (m_client && m_client->connectionCount.fetch_sub(1) == 1)
			{
				for (auto& entry : m_server.m_onDisconnectFunctions)
					entry.function(m_client->uid, m_client->id);
				m_server.m_logger.Info(TC("Client %s disconnected"), GuidToString(m_client->uid).str);
			}
			m_disconnected = true;
		}

		NetworkServer& m_server;
		NetworkBackend& m_backend;
		ReaderWriterLock m_shutdownLock;
		Client* m_client = nullptr;
		sockaddr m_remoteSockAddr;
		CryptoKey m_cryptoKey;
		Atomic<int> m_activeWorkerCount;
		Atomic<int> m_disconnectCalled;
		bool m_shouldDisconnect = false;
		bool m_disconnected = false;
		void* m_backendConnection = nullptr;

		Timer m_sendTimer;
		Timer m_encryptTimer;
		Timer m_decryptTimer;

		Connection(const Connection& o) = delete;
		void operator=(const Connection& o) = delete;
	};

	const Guid& ConnectionInfo::GetUid() const
	{
		return ((NetworkServer::Connection*)internalData)->m_client->uid;
	}

	u32 ConnectionInfo::GetId() const
	{
		return ((NetworkServer::Connection*)internalData)->m_client->id;
	}

	bool ConnectionInfo::GetName(StringBufferBase& out) const
	{
		#if PLATFORM_WINDOWS
		auto& remoteSockAddr = ((NetworkServer::Connection*)internalData)->m_remoteSockAddr;
		if (!InetNtopW(AF_INET, &remoteSockAddr, out.data, out.capacity))
			return false;
		out.count = u32(wcslen(out.data));
		return true;
		#else
		UBA_ASSERT(false);
		return false;
		#endif
	}

	void NetworkServer::Worker::ThreadWorker(NetworkServer& server)
	{
		u32 writeMemSize = server.m_sendSize;
		u8* writeMem = new u8[writeMemSize];
		auto memGuard = MakeGuard([writeMem]() { delete[] writeMem; });

		while (true)
		{
			if (!m_workAvailable.IsSet())
				break;
			if (!m_loop)
				break;

			// This is only additional work
			if (!m_connection)
			{
				DoAdditionalWorkAndSignalAvailable(server);
				continue;
			}

			CryptoKey cryptoKey = m_connection->m_cryptoKey;
			if (cryptoKey)
			{
				TimerScope ts(m_connection->m_decryptTimer);
				if (!Crypto::Decrypt(server.m_logger, cryptoKey, m_buffer.data(), m_dataSize))
				{
					m_connection->SetShouldDisconnect();
					m_connection->Release();
					DoAdditionalWorkAndSignalAvailable(server);
					continue;
				}
			}

			BinaryReader reader(m_buffer.data(), 0, m_dataSize);


			constexpr u32 HeaderSize = 5; // 2 byte id, 3 bytes size
			constexpr u32 ErrorSize = 0xffffff;

			BinaryWriter writer(writeMem, 0, writeMemSize);
			u8* idAndSizePtr = writer.AllocWrite(HeaderSize);
			
			u32 size;
			WorkerRec& rec = server.m_workerFunctions[m_serviceId];

			u32 workIndex = 0;
			if (server.m_trackWork)
			{
				workIndex = server.m_workCounter++;
				server.m_startWork(workIndex, rec.toString(m_messageType));
			}

			if (!rec.func)
			{
				server.m_logger.Error(TC("WORKER FUNCTION NOT FOUND. id: %u, serviceid: %u type: %s"), m_id, m_serviceId, rec.toString(m_messageType));
				m_connection->SetShouldDisconnect();
				size = ErrorSize;
			}
			else if (!rec.func({m_connection}, m_messageType, reader, writer))
			{
				if (m_connection->SetShouldDisconnect())
					server.m_logger.Error(TC("WORKER FUNCTION FAILED. id: %u, serviceid: %u type: %s"), m_id, m_serviceId, rec.toString(m_messageType));
				size = ErrorSize;
			}
			else
			{
				size = u32(writer.GetPosition());
			}

			if (server.m_trackWork)
				server.m_endWork(workIndex);

			if (m_id)
			{
				UBA_ASSERT(size < (1 << 24));
				
				u32 bodySize = u32(size - HeaderSize);
				if (cryptoKey && size != ErrorSize && bodySize)
				{
					TimerScope ts(m_connection->m_encryptTimer);
					u8* bodyData = writer.GetData() + HeaderSize;
					if (!Crypto::Encrypt(server.m_logger, cryptoKey, bodyData, bodySize))
					{
						m_connection->SetShouldDisconnect();
						size = ErrorSize;
						bodySize = u32(size - HeaderSize);
					}
				}

				idAndSizePtr[0] = m_id >> 8;
				*(u32*)(idAndSizePtr + 1) = bodySize | u32(m_id << 24);

				// This can happen for proxy servers in a valid situation
				//if (size == ErrorSize)
				//	UBA_ASSERT(false);

				m_connection->Send(writer.GetData(), size == ErrorSize ? HeaderSize : size);
			}
			
			m_connection->Release();

			DoAdditionalWorkAndSignalAvailable(server);
		}
	}

	void NetworkServer::Worker::DoAdditionalWorkAndSignalAvailable(NetworkServer& server)
	{
		while (true)
		{
			while (true)
			{
				AdditionalWork work;
				ScopedWriteLock lock(server.m_additionalWorkLock);
				if (server.m_additionalWork.empty())
					break;
				work = server.m_additionalWork.front();
				server.m_additionalWork.pop_front();
				lock.Leave();

				u32 workIndex = 0;
				if (server.m_trackWork)
				{
					workIndex = server.m_workCounter++;
					server.m_startWork(workIndex, work.desc.c_str());
				}

				work.func();

				if (server.m_trackWork)
					server.m_endWork(workIndex);
			}

			// Both locks needs to be taken to verify if additional work
			// is present before making ourself available to avoid
			// a race where AddWork would not see this thread in the
			// available list after adding some work.
			ScopedWriteLock lock1(server.m_availableWorkersLock);
			ScopedReadLock lock2(server.m_additionalWorkLock);
			// Verify there is not additional work while we hold both lock
			// and only add ourself as available if no additional work is present.
			if (!server.m_additionalWork.empty())
				continue;
			server.PushWorkerNoLock(this);
			break;
		}
	}

	const tchar* g_typeStr[] = { TC("0"), TC("1"), TC("2"), TC("3"), TC("4"), TC("5"), TC("6"), TC("7"), TC("8"), TC("9"), TC("10"), TC("11"), TC("12") };

	static const tchar* GetMessageTypeToName(u8 type)
	{
		if (type <= 12)
			return g_typeStr[type];
		return TC("NUMBER HIGHER THAN 12");
	}

	NetworkServer::NetworkServer(bool& outCtorSuccess, const NetworkServerCreateInfo& info, const tchar* name)
	:	m_logger(info.logWriter, name)
	,	m_workerAvailable(false)
	{
		outCtorSuccess = true;

		u32 workerCount = Min(Max(info.workerCount, (u32)(1u)), (u32)(1024u));
		m_workerCount = workerCount;

		#if UBA_DEBUG
		m_logger.Info(TC("Created in DEBUG"));
		#endif

		u32 fixedSendSize = Max(info.sendSize, (u32)(4*1024));
		fixedSendSize = Min(fixedSendSize, (u32)(SendMaxSize));
		if (info.sendSize != fixedSendSize)
			m_logger.Detail(TC("Adjusted msg size to %u to stay inside limits"), fixedSendSize);
		m_sendSize = fixedSendSize;
		m_receiveTimeoutMs = info.receiveTimeoutSeconds * 1000;

		m_workerFunctions[SystemServiceId].toString = GetMessageTypeToName;
		m_workerFunctions[SystemServiceId].func = [this](const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
			{
				return HandleSystemMessage(connectionInfo, messageType, reader, writer);
			};
	}

	NetworkServer::~NetworkServer()
	{
		StopAll();
	}

	bool NetworkServer::StartListen(NetworkBackend& backend, u16 port, const tchar* ip, const u8* cryptoKey128)
	{
		UBA_ASSERT(!m_listenBackend);
		m_listenBackend = &backend;

		if (cryptoKey128)
		{
			m_listenCrypto = Crypto::CreateKey(m_logger, cryptoKey128);
			if (!m_listenCrypto)
				return false;
		}

		return backend.StartListen(m_logger, port, ip, [&](void* connection, const sockaddr& remoteSockAddr)
			{
				CryptoKey cryptoKey = InvalidCryptoKey;
				if (m_listenCrypto)
				{
					cryptoKey = Crypto::DuplicateKey(m_logger, m_listenCrypto);
					if (!cryptoKey)
						return false;
				}
				return AddConnection(backend, connection, remoteSockAddr, cryptoKey);
			});
	}

	void NetworkServer::StopListen()
	{
		if (m_listenBackend)
			m_listenBackend->StopListen();
		m_listenBackend = nullptr;
	}

	void NetworkServer::DisallowNewClients()
	{
		m_allowNewClients = false;
	}

	void NetworkServer::StopAll()
	{
		StopListen();

		{
			ScopedWriteLock lock(m_addConnectionsLock);
			m_addConnections.clear();
		}

		{
			ScopedWriteLock lock(m_connectionsLock);
			auto connections(std::move(m_connections));
			lock.Leave();

			for (auto& c : connections)
			{
				c.Stop();
				m_sendTimer.Add(c.m_sendTimer);
				m_encryptTimer.Add(c.m_encryptTimer);
				m_decryptTimer.Add(c.m_decryptTimer);
			}
		}

		auto deleteWorkers = [](Worker*& start)
		{
			Worker* worker = start;
			while (worker)
			{
				Worker* temp = worker;
				worker = worker->m_nextWorker;
				delete temp;
			}
			start = nullptr;
		};
		deleteWorkers(m_firstAvailableWorker);
		deleteWorkers(m_firstActiveWorker);
	}

	bool NetworkServer::AddClient(NetworkBackend& backend, const tchar* ip, u16 port, const u8* cryptoKey128)
	{
		ScopedWriteLock lock(m_addConnectionsLock);
		for (auto it = m_addConnections.begin(); it != m_addConnections.end();)
		{
			if (it->Wait(0))
				it = m_addConnections.erase(it);
			else
				++it;
		}

		CryptoKey cryptoKey = InvalidCryptoKey;
		if (cryptoKey128)
		{
			cryptoKey = Crypto::CreateKey(m_logger, cryptoKey128);
			if (cryptoKey == InvalidCryptoKey)
				return false;
		}

		m_addConnections.emplace_back([this, &backend, ip2 = TString(ip), port, cryptoKey]()
			{
				bool success = backend.Connect(m_logger, ip2.c_str(), [this, &backend, cryptoKey](void* connection, const sockaddr& remoteSocketAddr, bool* timedOut)
					{
						return AddConnection(backend, connection, remoteSocketAddr, cryptoKey);
					}, port, nullptr);
				if (!success)
					Crypto::DestroyKey(cryptoKey);
				return 0;
			});
		return true;
	}

	void NetworkServer::PrintSummary(Logger& logger)
	{
		if (!m_maxActiveConnections)
			return;

		StringBuffer<> workers;
		workers.Appendf(TC("%u/%u"), m_createdWorkerCount, m_workerCount);

		logger.Info(TC("  ----- Uba server stats summary ------"));
		logger.Info(TC("  MaxActiveConnections           %6u"), m_maxActiveConnections);
		logger.Info(TC("  SendTotal          %8u %9s"), m_sendTimer.count.load(), TimeToText(m_sendTimer.time).str);
		logger.Info(TC("     Bytes                    %9s"), BytesToText(m_sendBytes).str);
		logger.Info(TC("  RecvTotal          %8u %9s"), m_recvCount.load(), BytesToText(m_recvBytes.load()).str);
		if (m_encryptTimer.count || m_decryptTimer.count)
		{
			logger.Info(TC("  EncryptTotal       %8u %9s"), m_encryptTimer.count.load(), TimeToText(m_encryptTimer.time).str);
			logger.Info(TC("  DecryptTotal       %8u %9s"), m_decryptTimer.count.load(), TimeToText(m_decryptTimer.time).str);
		}
		logger.Info(TC("  WorkerCount                 %9s"), workers.data);
		logger.Info(TC("  SendSize Set/Max  %9s %9s"), BytesToText(m_sendSize).str, BytesToText(SendMaxSize).str);
		logger.Info(TC(""));
	}

	void NetworkServer::RegisterService(u8 serviceId, const WorkerFunction& function, TypeToNameFunction* typeToNameFunc)
	{
		UBA_ASSERTF(serviceId != 0, TC("ServiceId 0 is reserved by system"));
		WorkerRec& rec = m_workerFunctions[serviceId];
		UBA_ASSERT(!rec.func);
		rec.func = function;
		rec.toString = typeToNameFunc;
		if (!typeToNameFunc)
			rec.toString = GetMessageTypeToName;
	}

	void NetworkServer::UnregisterService(u8 serviceId)
	{
		ScopedWriteLock lock(m_connectionsLock);
		UBA_ASSERTF(!m_listenBackend, TC("Server still listens to new connections while service is unregistered"));
		UBA_ASSERTF(m_connections.empty(), TC("Unregistering service while still having live connections"));
		WorkerRec& rec = m_workerFunctions[serviceId];
		rec.func = {};
		//rec.toString = nullptr; // Keep this for now, we want to be able to output stats
	}

	void NetworkServer::RegisterOnClientConnected(u8 id, const OnConnectionFunction& func)
	{
		UBA_ASSERT(!m_onConnectionFunction);
		m_onConnectionFunction = func;
	}

	void NetworkServer::UnregisterOnClientConnected(u8 id)
	{
		ScopedWriteLock lock(m_connectionsLock);
		UBA_ASSERT(!m_listenBackend);
		UBA_ASSERT(m_connections.empty());
		m_onConnectionFunction = {};
	}

	void NetworkServer::RegisterOnClientDisconnected(u8 id, const OnDisconnectFunction& func)
	{
		m_onDisconnectFunctions.emplace_back(OnDisconnectEntry{id, func});
	}

	void NetworkServer::UnregisterOnClientDisconnected(u8 id)
	{
		for (auto it = m_onDisconnectFunctions.begin(); it != m_onDisconnectFunctions.end(); ++it)
		{
			if (it->id != id)
				continue;
			m_onDisconnectFunctions.erase(it);
			return;
		}
	}

	void NetworkServer::AddWork(const Function<void()>& work, u32 count, const tchar* desc)
	{
		ScopedWriteLock lock(m_additionalWorkLock);
		for (u32 i = 0; i != count; ++i)
		{
			m_additionalWork.push_back({ work });
			if (m_trackWork)
				m_additionalWork.back().desc = desc;
		}
		lock.Leave();

		ScopedWriteLock lock2(m_availableWorkersLock);
		while (count-- && m_createdWorkerCount < m_workerCount)
		{
			Worker* worker = PopWorkerNoLock();
			worker->m_connection = nullptr;
			worker->m_workAvailable.Set();
		}
	}

	u32 NetworkServer::GetWorkerCount()
	{
		return m_workerCount;
	}

	u64 NetworkServer::GetTotalSentBytes()
	{
		return m_sendBytes;
	}

	u64 NetworkServer::GetTotalRecvBytes()
	{
		return m_recvBytes;
	}

	void NetworkServer::GetClientStats(ClientStats& out, u32 clientId)
	{
		ScopedReadLock lock(m_clientsLock);
		auto findIt = m_clients.find(clientId);
		if (findIt == m_clients.end())
			return;
		Client& c = findIt->second;
		out.send += c.sendBytes;
		out.recv += c.recvBytes;
		out.connectionCount += c.connectionCount;
	}

	bool NetworkServer::DoAdditionalWork()
	{
		AdditionalWork work;
		ScopedWriteLock lock(m_additionalWorkLock);
		if (m_additionalWork.empty())
			return false;
		work = m_additionalWork.front();
		m_additionalWork.pop_front();
		lock.Leave();

		u32 workIndex = 0;
		if (m_trackWork)
		{
			workIndex = m_workCounter++;
			m_startWork(workIndex, work.desc.c_str());
		}

		work.func();

		if (m_trackWork)
			m_endWork(workIndex);
		return true;
	}

	void NetworkServer::SetWorkListener(const WorkBeginFunction& start, const WorkEndFunction& end)
	{
		m_startWork = start;
		m_endWork = end;
		m_trackWork = true;
	}

	void NetworkServer::ResetWorkListener()
	{
		m_trackWork = false;
		m_startWork = {};
		m_endWork = {};
	}

	NetworkServer::Worker* NetworkServer::PopWorker()
	{
		ScopedWriteLock lock(m_availableWorkersLock);
		return PopWorkerNoLock();
	}

	NetworkServer::Worker* NetworkServer::PopWorkerNoLock()
	{
		Worker* worker = m_firstAvailableWorker;
		if (worker)
		{
			m_firstAvailableWorker = worker->m_nextWorker;
			if (m_firstAvailableWorker)
				m_firstAvailableWorker->m_prevWorker = nullptr;
		}
		else
		{
			worker = new Worker();
			worker->Start(*this);
			++m_createdWorkerCount;
		}

		if (m_firstActiveWorker)
			m_firstActiveWorker->m_prevWorker = worker;
		worker->m_nextWorker = m_firstActiveWorker;
		m_firstActiveWorker = worker;

		return worker;
	}

	void NetworkServer::PushWorker(Worker* worker)
	{
		ScopedWriteLock lock(m_availableWorkersLock);
		PushWorkerNoLock(worker);
	}

	void NetworkServer::PushWorkerNoLock(Worker* worker)
	{
		if (worker->m_prevWorker)
			worker->m_prevWorker->m_nextWorker = worker->m_nextWorker;
		else
			m_firstActiveWorker = worker->m_nextWorker;
		if (worker->m_nextWorker)
			worker->m_nextWorker->m_prevWorker = worker->m_prevWorker;

		if (m_firstAvailableWorker)
			m_firstAvailableWorker->m_prevWorker = worker;
		worker->m_prevWorker = nullptr;
		worker->m_nextWorker = m_firstAvailableWorker;
		m_firstAvailableWorker = worker;
		m_workerAvailable.Set();
	}

	void NetworkServer::RemoveDisconnectedConnections()
	{
		for (auto it=m_connections.begin(); it!=m_connections.end();)
		{
			Connection& con = *it;
			if (!con.m_disconnected)
			{
				++it;
				continue;
			}
			m_sendTimer.Add(con.m_sendTimer);
			it = m_connections.erase(it);
		}
	}

	bool NetworkServer::HandleSystemMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
	{
		switch (messageType)
		{
			case SystemMessageType_SetConnectionCount:
			{
				u32 connectionCount = reader.ReadU32();

				ScopedReadLock lock(m_clientsLock);
				auto findIt = m_clients.find(connectionInfo.GetId());
				if (findIt == m_clients.end())
					return true;
				Client& c = findIt->second;
				lock.Leave();

				if (c.connectionCount >= connectionCount)
					return true;
				u32 toAdd = connectionCount - c.connectionCount;

				auto& conn = *(NetworkServer::Connection*)connectionInfo.internalData;
				auto remoteAddr = conn.m_remoteSockAddr;
				ScopedWriteLock lock2(m_addConnectionsLock);
				for (u32 i = 0; i != toAdd; ++i)
				{
					m_addConnections.emplace_back([this, &conn, remoteAddr]()
						{
							conn.m_backend.Connect(m_logger, remoteAddr, [this, &conn](void* connection, const sockaddr& remoteSocketAddr, bool* timedOut)
								{
									CryptoKey cryptoKey = InvalidCryptoKey;
									if (conn.m_cryptoKey)
									{
										cryptoKey = Crypto::DuplicateKey(m_logger, conn.m_cryptoKey);
										if (cryptoKey == InvalidCryptoKey)
											return false;
									}
									return AddConnection(conn.m_backend, connection, remoteSocketAddr, cryptoKey);
								}, nullptr);
							return 0;
						});
				}
				return true;
			}
			case SystemMessageType_KeepAlive:
			{
				// No-op
				return true;
			}
		}
		return false;
	}

	bool NetworkServer::AddConnection(NetworkBackend& backend, void* backendConnection, const sockaddr& remoteSocketAddr, CryptoKey cryptoKey)
	{
		ScopedWriteLock lock(m_connectionsLock);

		RemoveDisconnectedConnections();

		m_connections.emplace_back(*this, backend, backendConnection, remoteSocketAddr, cryptoKey).Start();
		m_maxActiveConnections = Max(m_maxActiveConnections, u32(m_connections.size()));
		return true;
	}
}
