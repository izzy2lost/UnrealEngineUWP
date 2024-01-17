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
	,	m_fileMapping(m_logger)
	{
		m_hasActiveFetchesEvent.Set();

		m_server.RegisterService(StorageServiceId,
			[this](const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
			{
				return HandleMessage(connectionInfo, messageType, reader, writer);
			},
			[](u8 messageType)
			{
				return ToString(StorageMessageType(messageType));
			}
		);

		m_fileMapping.AddTransient(TC("Files"));
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

	bool StorageProxy::HandleMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
	{
		LoggerWithWriter logger(g_consoleLogWriter, TC(""));

		StackBinaryWriter<1024> writer2;
		StackBinaryReader<SendMaxSize> reader2;

		switch (messageType)
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

				ScopedWriteLock filesLock(m_filesLock);
				auto insres = m_files.try_emplace(casKey);
				FileEntry& file = insres.first->second;
				filesLock.Leave();

				MappedView view;
				auto viewGuard = MakeGuard([&]() { m_fileMapping.UnmapView(view, hint.data); });

				ScopedWriteLock fileLock(file.lock);

				while (true)
				{
					if (file.view.size != 0)
					{
						fileLock.Leave();
						view = m_fileMapping.MapView(file.view.handle, file.view.offset, file.view.size, hint.data);
						break;
					}

					bool storeCompressed = true;

					if (m_localStorage && IsCompressed(casKey) && m_inProcessClientId && connectionInfo.GetId() != m_inProcessClientId)
					{
						// We need to leave this lock here since the in-process storage client might be asking for this file too and then we can end up in a deadlock
						fileLock.Leave();

						bool hasCas = m_localStorage->EnsureCasFile(casKey, nullptr);

						// Enter lock again, and also check if another thread might have already handled this file while we looked if it existed in local storage
						fileLock.Enter();
						if (file.view.size != 0)
							continue;

						if (hasCas)
						{

							StringBuffer<> casFile;
							if (m_localStorage->GetCasFileName(casFile, casKey))
							{
								FileAccessor sourceFile(m_logger, casFile.data);
								if (sourceFile.OpenMemoryRead())
								{
									u64 fileSize = sourceFile.GetSize();
									view = m_fileMapping.AllocAndMapView(MappedView_Transient, fileSize, 1, hint.data);
									if (!view.memory)
										return false;
									memcpy(view.memory, sourceFile.GetData(), fileSize);
								}
							}
						}
					}

					if (!view.handle.IsValid())
					{
						NetworkMessage msg(m_client, ServiceId, messageType, writer2);
						writer2.WriteBool(false); // Wants proxy
						writer2.WriteCasKey(casKey);
						writer2.WriteString(hint);
						writer2.WriteBytes(reader.GetPositionData(), reader.GetLeft());

						if (!msg.Send(reader2))
							return false;

						BinaryReader tempReader(reader2.GetPositionData(), 0, reader2.GetLeft());
						u32 sizeOfFirstMessage = u32(reader2.GetLeft());
						u16 fetchId = tempReader.ReadU16();
						if (fetchId == 0)
						{
							m_logger.Error(TC("FetchBegin failed for cas file %s (%s)"), CasKeyString(casKey).str, hint.data);
							writer.WriteU16(0);
							return true;
						}
						u64 fileSize = tempReader.Read7BitEncoded();
						view = m_fileMapping.AllocAndMapView(MappedView_Transient, fileSize, 1, hint.data);

						u8 flags = tempReader.ReadByte();
						storeCompressed = (flags >> 0) & 1;
						bool sendEnd = (flags >> 1) & 1;
						u64 fetchedSize = tempReader.GetLeft();

						u32 responseSize = u32(fetchedSize);

						u64 left = fileSize;
						u8* readBuffer = view.memory;

						memcpy(view.memory, tempReader.GetPositionData(), responseSize);
						left -= responseSize;
						readBuffer += responseSize;

						if (!StorageClient::SendAllSegments(m_client, fetchId, readBuffer, left, sizeOfFirstMessage))
							return false;

						if (sendEnd)
						{
							StackBinaryWriter<128> writer3;
							NetworkMessage msg2(m_client, ServiceId, StorageMessageType_FetchEnd, writer3);
							writer3.WriteCasKey(casKey);
							if (!msg2.Send())
								return false;
						}
					}
					file.storeCompressed = storeCompressed;
					file.view = view;
					fileLock.Leave();
					break;
				}

				u16 fetchId = u16(~0);

				u64 headerSize = sizeof(u16) + Get7BitEncodedCount(file.view.size) + sizeof(u8);
				u64 fetchedSize = Min(view.size, m_client.GetMessageMaxSize() - m_client.GetMessageReceiveHeaderSize() - headerSize);

				if (fetchedSize < view.size)
				{
					fetchId = PopId();
					ScopedWriteLock lock(m_activeFetchesLock);
					if (m_activeFetches.empty())
						m_hasActiveFetchesEvent.Reset();
					auto res = m_activeFetches.try_emplace(fetchId);
					UBA_ASSERT(res.second);
					ActiveFetch& fetch = res.first->second;
					lock.Leave();

					fetch.fetchedSize = fetchedSize;
					fetch.file = &file;
				}

				u8 flags = 0;
				flags |= u8(file.storeCompressed) << 0;

				writer.WriteU16(fetchId);
				writer.Write7BitEncoded(view.size);
				writer.WriteByte(flags);
				writer.WriteBytes(view.memory, fetchedSize);

				return true;
			}
		case StorageMessageType_FetchSegment:
			{
				u16 fetchId = reader.ReadU16();
				u32 fetchIndex = reader.ReadU32();

				ScopedReadLock activeLock(m_activeFetchesLock);
				auto findIt = m_activeFetches.find(fetchId);
				UBA_ASSERT(findIt != m_activeFetches.end());
				ActiveFetch& fetch = findIt->second;
				activeLock.Leave();

				FileEntry& file = *fetch.file;

				u64 headerSize = sizeof(u16) + Get7BitEncodedCount(file.view.size) + sizeof(u8);
				u64 firstFetchSize = m_client.GetMessageMaxSize() - m_client.GetMessageReceiveHeaderSize() - headerSize;

				u64 offset = firstFetchSize + writer.GetCapacityLeft() * (fetchIndex - 1);

				u64 segmentSize = writer.GetCapacityLeft();
				if (offset + segmentSize > file.view.size)
					segmentSize = file.view.size - offset;
				MappedView view = m_fileMapping.MapView(file.view.handle, file.view.offset + offset, segmentSize, TC(""));
				auto viewGuard = MakeGuard([&]() { m_fileMapping.UnmapView(view, TC("")); });

				writer.WriteBytes(view.memory, segmentSize);

				u64 fetchedSize = fetch.fetchedSize.fetch_add(segmentSize) + segmentSize;
				if (fetchedSize != file.view.size)
					return true;

				ScopedWriteLock activeLock2(m_activeFetchesLock);
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
				NetworkMessage msg(m_client, ServiceId, messageType, writer2);
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
		ScopedWriteLock lock(m_availableIdsLock);
		if (m_availableIds.empty())
			return m_availableIdsHigh++;
		u16 storeId = m_availableIds.back();
		m_availableIds.pop_back();
		return storeId;
	}

	void StorageProxy::PushId(u16 id)
	{
		ScopedWriteLock lock(m_availableIdsLock);
		m_availableIds.push_back(id);
	}
}
