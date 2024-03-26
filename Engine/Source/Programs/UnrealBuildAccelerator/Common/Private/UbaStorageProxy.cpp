// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaStorageProxy.h"
#include "UbaFileAccessor.h"
#include "UbaNetworkClient.h"
#include "UbaNetworkMessage.h"
#include "UbaNetworkServer.h"
#include "UbaStorageClient.h"

namespace uba
{
	StorageProxy::StorageProxy(NetworkServer& server, NetworkClient& client, const Guid& storageServerUid, const tchar* name, StorageImpl* localStorage)
	:	m_server(server)
	,	m_client(client)
	,	m_localStorage(localStorage)
	,	m_logger(client.GetLogWriter(), TC("StorageProxy"))
	,	m_storageServerUid(storageServerUid)
	,	m_name(name)
	,	m_hasActiveFetchesEvent(true)
	{
		m_hasActiveFetchesEvent.Set();

		m_server.RegisterOnClientDisconnected(0, [this](const Guid& clientUid, u32 clientId)
			{
				SCOPED_WRITE_LOCK(m_activeFetchesLock, lock);
				for (auto it=m_activeFetches.begin(); it!=m_activeFetches.end();)
				{
					if (it->second.clientUid != clientUid)
					{
						++it;
						continue;
					}
					PushId(it->first);
					it = m_activeFetches.erase(it);
				}
			});

		m_server.RegisterService(StorageServiceId,
			[this](const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				return HandleMessage(connectionInfo, messageInfo, reader, writer);
			},
			[](u8 messageType)
			{
				return ToString(StorageMessageType(messageType));
			}
		);

		m_client.RegisterOnDisconnected([this]() { m_logger.isMuted = true; });
	}

	StorageProxy::~StorageProxy()
	{
		m_server.StopAll();
		for (auto& kv : m_files)
			delete[] kv.second.memory;
	}

	bool StorageProxy::Disconnect(u32 timeoutMs)
	{
		// TODO: Send disconnect to server to make it transition proxy to another proxy
		//StackBinaryWriter<128> writer;
		//NetworkMessage msg(m_client, ServiceId, StorageMessageType_Disconnect, writer);
		//if (!msg.Send())
		//	return false;
		return m_hasActiveFetchesEvent.IsSet(timeoutMs);
	}

	void StorageProxy::PrintSummary()
	{
		LoggerWithWriter logger(m_logger.m_writer);
		logger.Info(TC("  -- Uba storage proxy stats summary --"));
		logger.Info(TC("  Total fetched           %6s"), BytesToText(0).str);
		logger.Info(TC("  Total provided          %6s"), BytesToText(0).str);
		logger.Info(TC(""));
	}

	bool StorageProxy::HandleMessage(const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
	{
		LoggerWithWriter logger(g_consoleLogWriter, TC(""));

		StackBinaryWriter<1024> writer2;
		StackBinaryReader<SendMaxSize> reader2;

		switch (messageInfo.type)
		{
		case StorageMessageType_Connect:
			{
				StringBuffer<> clientName;
				reader.ReadString(clientName);
				u32 clientVersion = reader.ReadU32();
				if (clientVersion != StorageNetworkVersion)
				{
					m_logger.Error(TC("Different network versions. Client: %u, Server: %u. Disconnecting"), clientVersion, StorageNetworkVersion);
					return false;
				}
				bool isInProcessClient = reader.ReadBool();
				if (isInProcessClient)
					m_inProcessClientId = connectionInfo.GetId();

				//m_logger.Info(TC("%s connected"), clientName.data);
				writer.WriteGuid(m_storageServerUid);
				return true;
			}

		case StorageMessageType_FetchBegin:
			{
				reader.ReadBool(); // Wants proxy

				CasKey casKey = reader.ReadCasKey();
				StringBuffer<> hint;
				reader.ReadString(hint);

				SCOPED_WRITE_LOCK(m_filesLock, filesLock);
				auto insres = m_files.try_emplace(casKey);
				FileEntry& file = insres.first->second;
				filesLock.Leave();

				SCOPED_WRITE_LOCK(file.lock, fileLock);

				bool hasAllSegments = false;
				while (true)
				{
					if (file.memory)
						break;

					constexpr bool useLocalStorage = true;
					bool storeCompressed = true;
					if (useLocalStorage && m_localStorage && IsCompressed(casKey) && m_inProcessClientId && connectionInfo.GetId() != m_inProcessClientId)
					{
						// We need to leave this lock here since the in-process storage client might be asking for this file too and then we can end up in a deadlock
						fileLock.Leave();

						bool hasCas = m_localStorage->EnsureCasFile(casKey, nullptr);
						StringBuffer<> casFile;
						hasCas = hasCas && m_localStorage->GetCasFileName(casFile, casKey);

						// Enter lock again, and also check if another thread might have already handled this file while we looked if it existed in local storage
						fileLock.Enter();
						if (file.memory)
							break;

						if (hasCas)
						{
							FileAccessor sourceFile(m_logger, casFile.data);
							if (sourceFile.OpenMemoryRead())
							{
								u64 fileSize = sourceFile.GetSize();
								file.memory = new u8[fileSize];
								if (!file.memory)
									return false;
								file.size = fileSize;
								memcpy(file.memory, sourceFile.GetData(), fileSize);
								hasAllSegments = true;
							}
						}
					}

					if (!file.memory)
					{
						NetworkMessage msg(m_client, ServiceId, messageInfo.type, writer2);
						writer2.WriteBool(false); // Wants proxy
						writer2.WriteCasKey(casKey);
						writer2.WriteString(hint);
						writer2.WriteBytes(reader.GetPositionData(), reader.GetLeft());

						if (!msg.Send(reader2))
						{
							file.error = true;
							return m_logger.Error(TC("FetchBegin failed for cas file %s (%s). Requested by %s"), CasKeyString(casKey).str, hint.data, GuidToString(connectionInfo.GetUid()).str);
						}

						BinaryReader tempReader(reader2.GetPositionData(), 0, reader2.GetLeft());
						u16 fetchId = tempReader.ReadU16();
						if (fetchId == 0)
						{
							file.error = true;
							m_logger.Error(TC("FetchBegin failed for cas file %s (%s). Requested by %s"), CasKeyString(casKey).str, hint.data, GuidToString(connectionInfo.GetUid()).str);
							writer.WriteU16(0);
							return true;
						}
						u64 fileSize = tempReader.Read7BitEncoded();
						file.memory = new u8[fileSize];
						file.size = fileSize;

						u8 flags = tempReader.ReadByte();
						storeCompressed = (flags >> 0) & 1;
						bool sendEnd = (flags >> 1) & 1;
						u64 fetchedSize = tempReader.GetLeft();

						memcpy(file.memory, tempReader.GetPositionData(), fetchedSize);

						if (sendEnd && fetchedSize == fileSize)
							SendEnd(casKey);

						file.received = fetchedSize;
						file.fetchId = fetchId;
						file.sendEnd = sendEnd;
					}

					file.storeCompressed = storeCompressed;
					file.casKey = casKey;

					if (file.received < file.size)
					{
						u64 segmentSize = m_client.GetMessageMaxSize() - 5; // This is server response size - header.. TODO: Should be taken from server
						u64 segmentCount = file.size / segmentSize + 1;
						u64 lookupByteCount = (segmentCount + 7) / 8;
						u8 initialValue = hasAllSegments ? 255 : 0;
						file.segmentsAvailable.resize(lookupByteCount, initialValue);
					}
					break;
				}

				if (file.error)
					return false;
				fileLock.Leave();

				u16 fetchId = u16(~0);

				u64 headerSize = sizeof(u16) + Get7BitEncodedCount(file.size) + sizeof(u8);
				u64 fetchedSize = Min(file.size, m_client.GetMessageMaxSize() - m_client.GetMessageReceiveHeaderSize() - headerSize);

				if (fetchedSize < file.size)
				{
					fetchId = PopId();
					SCOPED_WRITE_LOCK(m_activeFetchesLock, lock);
					if (m_activeFetches.empty())
						m_hasActiveFetchesEvent.Reset();
					auto res = m_activeFetches.try_emplace(fetchId);
					UBA_ASSERT(res.second);
					ActiveFetch& fetch = res.first->second;
					lock.Leave();

					fetch.clientUid = connectionInfo.GetUid();
					fetch.fetchedSize = fetchedSize;
					fetch.file = &file;
				}

				u8 flags = 0;
				flags |= u8(file.storeCompressed) << 0;

				writer.WriteU16(fetchId);
				writer.Write7BitEncoded(file.size);
				writer.WriteByte(flags);
				writer.WriteBytes(file.memory, fetchedSize);

				return true;
			}
		case StorageMessageType_FetchSegment:
			{
				u16 fetchId = reader.ReadU16();
				u32 fetchIndex = reader.ReadU32();

				SCOPED_READ_LOCK(m_activeFetchesLock, activeLock);
				auto findIt = m_activeFetches.find(fetchId);
				UBA_ASSERT(findIt != m_activeFetches.end());
				ActiveFetch& fetch = findIt->second;
				activeLock.Leave();

				FileEntry& file = *fetch.file;

				u64 headerSize = sizeof(u16) + Get7BitEncodedCount(file.size) + sizeof(u8);
				u64 firstFetchSize = m_client.GetMessageMaxSize() - m_client.GetMessageReceiveHeaderSize() - headerSize;
				u64 segmentSize = m_client.GetMessageMaxSize() - 5; // This is server response size - header.. TODO: Should be taken from server

				u64 offset = firstFetchSize + segmentSize * (fetchIndex - 1);
				if (offset + segmentSize > file.size)
					segmentSize = file.size - offset;
				
				SCOPED_WRITE_LOCK(file.lock, fileLock);
				if (file.error)
					return false;

				u64 byteIndex = fetchIndex / 8;
				u8 bitMask = u8(1 << (fetchIndex - byteIndex*8));
				
				u8& availableByte = file.segmentsAvailable[byteIndex];
				if (!(availableByte & bitMask))
				{
					SegmentInFlight* activeSegment = nullptr;
					auto asg = MakeGuard([&]
						{
							if (!--activeSegment->refCount)
							{
								if (auto next = activeSegment->next)
									next->prev = activeSegment->prev;
								else
									file.lastInFlight = activeSegment->prev;
								if (auto prev = activeSegment->prev)
									prev->next = activeSegment->next;
								else
									file.firstInFlight = activeSegment->next;
								delete activeSegment;
							}
						});

					for (auto it=file.firstInFlight; it; it=it->next)
					{
						if (it->segmentIndex != fetchIndex)
							continue;
						activeSegment = it;
						break;
					}

					if (!activeSegment)
					{
						activeSegment = new SegmentInFlight;
						activeSegment->segmentIndex = fetchIndex;
						activeSegment->refCount = 1;
						activeSegment->done.Create(true);

						activeSegment->next = nullptr;
						activeSegment->prev = file.lastInFlight;
						if (auto last = file.lastInFlight)
							last->next = activeSegment;
						file.lastInFlight = activeSegment;
						if (!file.firstInFlight)
							file.firstInFlight = activeSegment;

						fileLock.Leave();

						NetworkMessage msg(m_client, ServiceId, StorageMessageType_FetchSegment, writer2);
						writer2.WriteU16(file.fetchId);
						writer2.WriteU32(fetchIndex);

						if (!msg.Send(reader2))
						{
							file.error = true;
							activeSegment->done.Set();
							fileLock.Enter();
							return m_logger.Error(TC("FetchSegment failed. Requested by %s"), GuidToString(connectionInfo.GetUid()).str);
						}
						
						file.received += segmentSize;
						if (file.sendEnd && file.size == file.received)
							SendEnd(file.casKey);

						memcpy(file.memory + offset, reader2.GetPositionData(), segmentSize);

						activeSegment->done.Set();

						fileLock.Enter();
						availableByte |= bitMask;
					}
					else
					{
						++activeSegment->refCount;
						fileLock.Leave();
						bool success = activeSegment->done.IsSet(10*60*1000); // This should never happen.
						fileLock.Enter();
						if (!success)
							return m_logger.Error(TC("Connection %s timed out after 10 minutes waiting for segment %u on cas entry %s to be available in storage proxy"), fetchIndex, CasKeyString(file.casKey).str, GuidToString(connectionInfo.GetUid()).str);
						if (file.error)
							return false;
					}
				}
				fileLock.Leave();

				const u8* memory = file.memory + offset;

				writer.WriteBytes(memory, segmentSize);

				u64 fetchedSize = fetch.fetchedSize.fetch_add(segmentSize) + segmentSize;
				if (fetchedSize != file.size)
					return true;

				SCOPED_WRITE_LOCK(m_activeFetchesLock, activeLock2);
				m_activeFetches.erase(findIt);
				if (m_activeFetches.empty())
					m_hasActiveFetchesEvent.Set();
				activeLock2.Leave();

				PushId(fetchId);
				return true;
			}
		case StorageMessageType_FetchEnd:
			{
				return true;
			}
		default:
			{
				NetworkMessage msg(m_client, ServiceId, messageInfo.type, writer2);
				writer2.WriteBytes(reader.GetPositionData(), reader.GetLeft());
				if (!msg.Send(reader2))
					return false;
				writer.WriteBytes(reader2.GetPositionData(), reader2.GetLeft());
				return true;
			}
		}
	}

	u16 StorageProxy::PopId()
	{
		SCOPED_WRITE_LOCK(m_availableIdsLock, lock);
		if (m_availableIds.empty())
			return m_availableIdsHigh++;
		u16 storeId = m_availableIds.back();
		m_availableIds.pop_back();
		return storeId;
	}

	void StorageProxy::PushId(u16 id)
	{
		SCOPED_WRITE_LOCK(m_availableIdsLock, lock);
		m_availableIds.push_back(id);
	}

	bool StorageProxy::SendEnd(const CasKey& key)
	{
		StackBinaryWriter<128> writer;
		NetworkMessage msg(m_client, ServiceId, StorageMessageType_FetchEnd, writer);
		writer.WriteCasKey(key);
		return msg.Send();
	}

}
