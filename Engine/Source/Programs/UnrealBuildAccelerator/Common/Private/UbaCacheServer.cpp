// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaCacheServer.h"
#include "UbaBinaryReaderWriter.h"
#include "UbaFileAccessor.h"
#include "UbaNetworkServer.h"
#include "UbaStorageServer.h"

namespace uba
{
	static constexpr u32 CacheFileVersion = 1;

	struct CacheServer::Connection
	{
		Connection() : m_pathTable(8*1024*1024), m_casKeyTable(8*1024*1024) {}
		CompactPathTable m_pathTable;
		CompactCasKeyTable m_casKeyTable;

		ReaderWriterLock cacheEntryLookupLock;
		UnorderedMap<CasKey, CacheEntry> cacheEntryLookup;
	};

	const tchar* ToString(CacheMessageType type)
	{
		switch (type)
		{
			#define UBA_CACHE_MESSAGE(x) case CacheMessageType_##x: return TC("")#x;
			UBA_CACHE_MESSAGES
			#undef UBA_CACHE_MESSAGE
		default:
			return TC("Unknown"); // Should never happen
		}
	}

	CacheServer::CacheServer(LogWriter& writer, const tchar* rootDir, NetworkServer& server, StorageServer& storage)
	:	m_logger(writer, TC("UbaCacheServer"))
	,	m_server(server)
	,	m_storage(storage)
	,	m_pathTable(CachePathTableMaxSize)
	,	m_casKeyTable(CacheCasKeyTableMaxSize)
	{
		m_rootDir.count = GetFullPathNameW(rootDir, m_rootDir.capacity, m_rootDir.data, NULL);
		m_rootDir.Replace('/', PathSeparator).EnsureEndsWithSlash();

		m_server.RegisterService(CacheServiceId,
			[this](const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				return HandleMessage(connectionInfo, messageInfo.type, reader, writer);
			},
			[](u8 messageType)
			{
				return ToString(CacheMessageType(messageType));
			}
		);

		m_server.RegisterOnClientDisconnected(CacheServiceId, [this](const Guid& clientUid, u32 clientId)
			{
				OnDisconnected(clientId);
			});
	}

	CacheServer::~CacheServer()
	{
	}

	bool CacheServer::Load()
	{
		u64 startTime = GetTime();

		StringBuffer<> fileName(m_rootDir);
		fileName.EnsureEndsWithSlash().Append(TC("cachedb"));

		FileAccessor file(m_logger, fileName.data);
		if (!file.OpenMemoryRead(0, false))
			return true;
		BinaryReader reader(file.GetData(), 0, file.GetSize());

		u32 version = reader.ReadU32();
		if (version != CacheFileVersion)
			return true;

		u32 pathTableSize = reader.ReadU32();
		if (pathTableSize)
		{
			BinaryReader pathTableReader(reader.GetPositionData(), 0, pathTableSize);
			m_pathTable.ReadMem(pathTableReader, true);
			reader.Skip(pathTableSize);
		}

		u32 casKeyTableSize = reader.ReadU32();
		if (casKeyTableSize)
		{
			BinaryReader casKeyTableReader(reader.GetPositionData(), 0, casKeyTableSize);
			m_casKeyTable.ReadMem(casKeyTableReader, true);
			reader.Skip(casKeyTableSize);
		}

		u32 entryLookupCount = reader.ReadU32();
		m_cacheEntryLookup.reserve(entryLookupCount);

		u64 totalCacheEntryCount = 0;
		while (reader.GetLeft())
		{
			auto insres = m_cacheEntryLookup.try_emplace(reader.ReadCasKey());
			UBA_ASSERT(insres.second);
			auto& cacheEntries = insres.first->second;
			u32 cacheEntryCount = reader.ReadU32();
			totalCacheEntryCount += cacheEntryCount;

			while (cacheEntryCount--)
			{
				auto& cacheEntry = cacheEntries.entries.emplace_back();

				cacheEntry.creationTime = reader.ReadU64();

				u32 inputSize = reader.ReadU32();
				cacheEntry.inputCasKeyOffsets.resize(inputSize);
				reader.ReadBytes(cacheEntry.inputCasKeyOffsets.data(), inputSize);

				u32 outputSize = reader.ReadU32();
				cacheEntry.outputCasKeyOffsets.resize(outputSize);
				reader.ReadBytes(cacheEntry.outputCasKeyOffsets.data(), outputSize);
			}
		}

		u64 duration = GetTime() - startTime;
		m_logger.Detail(TC("Database loaded from %s in %s (contained %s paths, %s keys, %llu cache entries)"), fileName.data, TimeToText(duration).str, BytesToText(pathTableSize).str, BytesToText(casKeyTableSize).str, totalCacheEntryCount);
		return true;
	}

	bool CacheServer::Save()
	{
		if (m_addsSinceMaintenance == 0)
			return true;

		SCOPED_WRITE_LOCK(m_maintenanceLock, lock);
		return SaveNoLock();
	}

	bool CacheServer::SaveNoLock()
	{
		StringBuffer<MaxPath> fileName(m_rootDir);
		fileName.EnsureEndsWithSlash().Append(TC("cachedb"));

		StringBuffer<MaxPath> tempFileName;
		tempFileName.Append(fileName).Append(TC(".tmp"));

		FileAccessor file(m_logger, tempFileName.data);
		if (!file.CreateWrite())
			return false;


		constexpr u64 tempBufferSize = 1024*1024;
		u8* tempBuffer = (u8*)malloc(tempBufferSize);
		auto g = MakeGuard([tempBuffer](){ free(tempBuffer); });
		u64 tempBufferPos = 0;

		bool success = true;
		auto WriteBytes = [&](const void* data, u64 size)
			{
				u8* readPos = (u8*)data;
				u64 left = size;
				while (left)
				{
					if (tempBufferPos != tempBufferSize)
					{
						u64 toWrite = Min(tempBufferSize - tempBufferPos, left);
						memcpy(tempBuffer+tempBufferPos, readPos, toWrite);
						tempBufferPos += toWrite;
						left -= toWrite;
						readPos += toWrite;
					}
					else
					{
						success &= file.Write(tempBuffer, tempBufferPos);
						tempBufferPos = 0;
					}
				}
			};

		auto Write = [&](auto v) { WriteBytes(&v, sizeof(v)); };

		Write(CacheFileVersion);

		u32 pathTableSize = m_pathTable.GetSize();
		Write(pathTableSize);
		WriteBytes(m_pathTable.GetMemory(), pathTableSize);

		u32 casKeyTableSize = m_casKeyTable.GetSize();
		Write(casKeyTableSize);
		WriteBytes(m_casKeyTable.GetMemory(), casKeyTableSize);

		u32 entryLookupCount = u32(m_cacheEntryLookup.size());
		Write(entryLookupCount);

		for (auto& kv : m_cacheEntryLookup)
		{
			Write(kv.first);

			u32 cacheEntryCount = u32(kv.second.entries.size());
			Write(cacheEntryCount);

			for (CacheEntry& entry : kv.second.entries)
			{
				WriteBytes(&entry.creationTime, sizeof(entry.creationTime));

				u32 inputSize = u32(entry.inputCasKeyOffsets.size());
				Write(inputSize);
				WriteBytes(entry.inputCasKeyOffsets.data(), inputSize);

				u32 outputSize = u32(entry.outputCasKeyOffsets.size());
				Write(outputSize);
				WriteBytes(entry.outputCasKeyOffsets.data(), outputSize);
			}
		}

		success &= file.Write(tempBuffer, tempBufferPos);

		if (!success)
			return false;

		if (!file.Close())
			return false;

		if (!MoveFileExW(tempFileName.data, fileName.data, MOVEFILE_REPLACE_EXISTING))
			return m_logger.Error(TC("Can't move file from %s to %s (%s)"), tempFileName.data, fileName.data, LastErrorToText().data);

		return true;
	}

	bool CacheServer::RunMaintenance(bool force)
	{
		SCOPED_WRITE_LOCK(m_maintenanceLock, lock);
		SCOPED_READ_LOCK(m_connectionsLock, lock2);
		if (!m_connections.empty())
			return true;
		lock2.Leave();

		if (m_addsSinceMaintenance == 0 && !force)
			return true;

		bool forceAllSteps = true;

		m_logger.Info(TC("Maintenance started after %u added cache entries"), m_addsSinceMaintenance.load());
		
		u64 startTime = GetTime();

		bool entriesAdded = m_addsSinceMaintenance != 0;
		m_addsSinceMaintenance = 0;

		Set<CasKey> deletedCasFiles;
		m_storage.HandleOverflow(&deletedCasFiles);
		u64 deletedCasCount = deletedCasFiles.size();

		struct CasFileInfo { u64 size; u64 refCount; };
		Map<CasKey, CasFileInfo> existingCas;
		u64 totalCasSize = 0;
		{
			u64 traverseStartTime = GetTime();
			m_storage.TraverseAllCasFiles([&](const CasKey& casKey, u64 size) { totalCasSize += size; existingCas.try_emplace(casKey, CasFileInfo{size, 0ull}); });
			m_logger.Detail(TC("  Found %llu cas files (%s)"), existingCas.size(), TimeToText(GetTime() - traverseStartTime).str);
		}
		u64 totalCasCount = existingCas.size();

		Vector<u64*> touchedCas;

		u64 now = GetSystemTimeAsFileTime();
		u64 oldest = now;


		u64 totalEntryCount = 0;
		u64 totalEntrySize = 0;
		u64 deleteEntryCount = 0;

		u64 deleteCacheEntriesStartTime = GetTime();
		do
		{
			oldest = now;
			totalEntryCount = 0;
			totalEntrySize = 0;

			for (auto li=m_cacheEntryLookup.begin(), le=m_cacheEntryLookup.end(); li!=le;)
			{
				// There is currently no idea saving more than 256kb worth of entries per lookup key (because that is what fetch max returns).. so let's wipe out
				// all the entries that overflow that number
				u64 capacityLeft = SendMaxSize - 32;

				CacheEntries& entries = li->second;
				for (auto i=entries.entries.begin(), e=entries.entries.end(); i!=e;)
				{
					auto& entry = *i;
					auto& inputs = entry.inputCasKeyOffsets;
					auto& outputs = entry.outputCasKeyOffsets;

					bool deleteEntry = false;

					u64 neededSize = Get7BitEncodedCount(inputs.size()) + inputs.size() + Get7BitEncodedCount(outputs.size()) + outputs.size();
					if (neededSize > capacityLeft)
					{
						deleteEntry = true;
						capacityLeft = 0;
					}
					else
					{
						// Traverse outputs and check if cas files exists for each output, if not, delete entry.
						touchedCas.clear();
						BinaryReader reader2(outputs.data(), 0, outputs.size());
						while (reader2.GetLeft())
						{
							u64 offset = reader2.Read7BitEncoded();
							CasKey casKey;
							m_casKeyTable.GetKey(casKey, offset);
							UBA_ASSERT(IsCompressed(casKey));
							auto findIt = existingCas.find(casKey);
							if (findIt == existingCas.end())
								deleteEntry = true;
							else
								touchedCas.push_back(&findIt->second.refCount);
						}
					}

					// Check if there are keys that use removed caskey as input and delete those too
					if (!deleteEntry && !deletedCasFiles.empty())
					{
						BinaryReader reader2(inputs.data(), 0, inputs.size());
						while (reader2.GetLeft())
						{
							u64 offset = reader2.Read7BitEncoded();
							CasKey casKey;
							m_casKeyTable.GetKey(casKey, offset);
							if (deletedCasFiles.find(casKey) != deletedCasFiles.end())
							{
								deleteEntry = true;
								break;
							}
						}
					}

					// Remove entry from entries list and skip increasing ref count of cas files
					if (deleteEntry)
					{
						++deleteEntryCount;
						i = entries.entries.erase(i);
						e = entries.entries.end();
						continue;
					}

					++totalEntryCount;
					totalEntrySize += neededSize;

					capacityLeft -= neededSize;

					if (!oldest || entry.creationTime < oldest)
						oldest = entry.creationTime;

					for (u64* v : touchedCas)
						++(*v);
					++i;
				}

				if (!entries.entries.empty())
				{
					++li;
					continue;
				}

				li = m_cacheEntryLookup.erase(li);
				le = m_cacheEntryLookup.end();
			}

			// Reset deleted cas files and update it again..
			deletedCasFiles.clear();

			for (auto i=existingCas.begin(), e=existingCas.end(); i!=e;)
			{
				if (i->second.refCount != 0)
				{
					i->second.refCount = 0;
					++i;
					continue;
				}
				deletedCasFiles.insert(i->first);
				++deletedCasCount;
				totalCasSize -= i->second.size;
				i = existingCas.erase(i);
				e = existingCas.end();
			}

			for (auto& casKey : deletedCasFiles)
				m_storage.DropCasFile(casKey, true, TC(""));
		}
		while (!deletedCasFiles.empty()); // if cas files are deleted we need to do another loop and check cache entry inputs to see if files were inputs

		m_logger.Detail(TC("  Deleted %llu cas files and %llu cache entries (%s)"), deletedCasCount, deleteEntryCount, TimeToText(GetTime() - deleteCacheEntriesStartTime).str);

		if ( deleteEntryCount || forceAllSteps)
		{
			UnorderedSet<u32> usedCasKeyOffsets;

			// Collect all caskeys that are used by cache entries.
			for (auto& kv : m_cacheEntryLookup)
			{
				for (auto& entry : kv.second.entries)
				{
					auto collectUsedCasKeyOffsets = [&](const Vector<u8>& offsets)
						{
							BinaryReader reader2(offsets.data(), 0, offsets.size());
							while (reader2.GetLeft())
								usedCasKeyOffsets.insert(u32(reader2.Read7BitEncoded()));
						};
					collectUsedCasKeyOffsets(entry.inputCasKeyOffsets);
					collectUsedCasKeyOffsets(entry.outputCasKeyOffsets);
				}
			}

			u64 recreatePathTableStart = GetTime();

			// Traverse all caskeys in caskey table and figure out which ones we can delete
			UnorderedSet<u32> usedPathOffsets;
			usedPathOffsets.reserve(usedCasKeyOffsets.size());

			for (u32 casKeyOffset : usedCasKeyOffsets)
			{
				BinaryReader reader2(m_casKeyTable.GetMemory(), casKeyOffset, m_casKeyTable.GetSize());
				u32 pathOffset = u32(reader2.Read7BitEncoded());
				usedPathOffsets.insert(pathOffset);
			}

			// Build new path table based on used offsets
			UnorderedMap<u32, u32> oldToNewPathOffset;
			u32 oldSize = m_pathTable.GetSize();
			{
				u64 reservedPathOffsets = usedPathOffsets.size()*2; // Trying to estimate offset count
				CompactPathTable newPathTable(CachePathTableMaxSize, reservedPathOffsets);
				oldToNewPathOffset.reserve(usedPathOffsets.size());

				for (u32 pathOffset : usedPathOffsets)
				{
					StringBuffer<> temp;
					m_pathTable.GetString(temp, pathOffset);
					auto res = oldToNewPathOffset.try_emplace(pathOffset, newPathTable.AddNoLock(temp.data, temp.count));
					UBA_ASSERT(res.second);(void)res;
				}
				m_pathTable.Swap(newPathTable);
			}
			m_logger.Detail(TC("  Recreated path table. %s -> %s (%s)"), BytesToText(oldSize).str, BytesToText(m_pathTable.GetSize()).str, TimeToText(GetTime() - recreatePathTableStart).str);


			// Build new caskey table based on used offsets
			u64 recreateCasKeyTableStart = GetTime();
			UnorderedMap<u32, u32> oldToNewCasKeyOffset;
			oldSize = m_casKeyTable.GetSize();
			{
				oldToNewCasKeyOffset.reserve(usedCasKeyOffsets.size());
				CompactCasKeyTable newCasKeyTable(CacheCasKeyTableMaxSize, usedCasKeyOffsets.size());
				for (u32 casKeyOffset : usedCasKeyOffsets)
				{
					BinaryReader reader2(m_casKeyTable.GetMemory(), casKeyOffset, m_casKeyTable.GetSize());
					u32 oldPathOffset = u32(reader2.Read7BitEncoded());
					CasKey casKey = reader2.ReadCasKey();
					auto findIt = oldToNewPathOffset.find(oldPathOffset);
					UBA_ASSERT(findIt != oldToNewPathOffset.end());
					auto res = oldToNewCasKeyOffset.try_emplace(casKeyOffset, newCasKeyTable.Add(casKey, findIt->second));
					UBA_ASSERT(res.second);(void)res;
				}
				m_casKeyTable.Swap(newCasKeyTable);
			}
			m_logger.Detail(TC("  Recreated caskey table. %s -> %s (%s)"), BytesToText(oldSize).str, BytesToText(m_casKeyTable.GetSize()).str, TimeToText(GetTime() - recreateCasKeyTableStart).str);


			// Update all casKeyOffsets
			u64 updateEntriesStart = GetTime();
			for (auto& kv : m_cacheEntryLookup)
			{
				for (auto& entry : kv.second.entries)
				{
					auto updateCasKeyOffsets = [&](Vector<u8>& offsets)
						{
							Set<u32> newOffsets; // Want these sorted since that is what we get from uploads
							u32 newOffsetsSize = 0;
							BinaryReader reader2(offsets.data(), 0, offsets.size());
							while (reader2.GetLeft())
							{
								u32 oldOffset = u32(reader2.Read7BitEncoded());
								auto findIt = oldToNewCasKeyOffset.find(oldOffset);
								UBA_ASSERT(findIt != oldToNewCasKeyOffset.end());
								u32 newOffset = findIt->second;
								if (newOffsets.insert(newOffset).second)
									newOffsetsSize += Get7BitEncodedCount(newOffset);
								else
								{
									StringBuffer<> temp;
									CasKey key;
									m_casKeyTable.GetPathAndKey(temp, key, m_pathTable, newOffset);
									m_logger.Info(TC("Found duplicate of %s in cache entry"), temp.data);
								}
							}

							offsets.resize(newOffsetsSize);
							BinaryWriter writer2(offsets.data(), 0, newOffsetsSize);
							for (u32 offset : newOffsets)
								writer2.Write7BitEncoded(offset);
							UBA_ASSERT(writer2.GetPosition() == newOffsetsSize);
						};
					updateCasKeyOffsets(entry.inputCasKeyOffsets);
					updateCasKeyOffsets(entry.outputCasKeyOffsets);
				}
			}

			m_logger.Detail(TC("  Updated cache entries with new tables (%s)"), TimeToText(GetTime() - updateEntriesStart).str);
		}

		if (entriesAdded || deletedCasCount || deleteEntryCount || forceAllSteps)
		{
			u64 saveStart = GetTime();
			m_storage.SaveCasTable(false, false);
			SaveNoLock();
			m_logger.Detail(TC("  Saved to disk (%s)"), TimeToText(GetTime() - saveStart).str);
		}

		u64 oldestTime = MsToTime(GetFileTimeAsSeconds(now - oldest)*1000);
		m_logger.Info(TC("Maintenance done! (%s). CacheEntries: %llu (%s) CasFiles: %llu (%s) PathTable: %s CasTable: %s OldestEntry: %s"), TimeToText(GetTime() - startTime).str, totalEntryCount, BytesToText(totalEntrySize).str, totalCasCount, BytesToText(totalCasSize).str, BytesToText(m_pathTable.GetSize()).str, BytesToText(m_casKeyTable.GetSize()).str, TimeToText(oldestTime, true).str);

		return true;
	}

	void CacheServer::OnDisconnected(u32 clientId)
	{
		SCOPED_WRITE_LOCK(m_connectionsLock, lock);
		m_connections.erase(clientId);
		lock.Leave();
	}

	bool CacheServer::HandleMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
	{
		switch (messageType)
		{
		case CacheMessageType_Connect:
		{
			u32 clientVersion = reader.ReadU32();
			if (clientVersion != CacheNetworkVersion)
				return m_logger.Error(TC("Different network versions. Client: %u, Server: %u. Disconnecting"), clientVersion, CacheNetworkVersion);
			SCOPED_READ_LOCK(m_maintenanceLock, lock);
			SCOPED_WRITE_LOCK(m_connectionsLock, lock2);
			m_connections[connectionInfo.GetId()];
			return true;
		}
		case CacheMessageType_StorePathTable:
		{
			SCOPED_WRITE_LOCK(m_connectionsLock, lock);
			auto& connection = m_connections[connectionInfo.GetId()];
			lock.Leave();
			connection.m_pathTable.ReadMem(reader, false);
			return true;
		}
		case CacheMessageType_StoreCasTable:
		{
			SCOPED_WRITE_LOCK(m_connectionsLock, lock);
			auto& connection = m_connections[connectionInfo.GetId()];
			lock.Leave();
			connection.m_casKeyTable.ReadMem(reader, false);
			return true;
		}
		case CacheMessageType_StoreEntry:
		{
			SCOPED_WRITE_LOCK(m_connectionsLock, lock);
			auto& connection = m_connections[connectionInfo.GetId()];
			lock.Leave();
			return HandleStoreEntry(connection, reader, writer);
		}
		case CacheMessageType_StoreEntryDone:
		{
			SCOPED_WRITE_LOCK(m_connectionsLock, lock);
			auto& connection = m_connections[connectionInfo.GetId()];
			lock.Leave();
			CasKey cmdKey = reader.ReadCasKey();

			SCOPED_WRITE_LOCK(connection.cacheEntryLookupLock, lock2);
			auto findIt = connection.cacheEntryLookup.find(cmdKey);
			if (findIt != connection.cacheEntryLookup.end())
			{
				SCOPED_WRITE_LOCK(m_cacheEntryLookupLock, lock3);
				auto insres = m_cacheEntryLookup.try_emplace(cmdKey);
				auto& cacheEntries = insres.first->second;
				lock2.Leave();

				SCOPED_WRITE_LOCK(cacheEntries.lock, lock4);
				auto& cacheEntry = cacheEntries.entries.emplace_front(std::move(findIt->second));
				(void)cacheEntry;
				//m_logger.Info(TC("Added new cache entry"));
			}
			return true;
		}
		case CacheMessageType_FetchPathTable:
			return HandleFetchPathTable(reader, writer);

		case CacheMessageType_FetchCasTable:
			return HandleFetchCasTable(reader, writer);

		case CacheMessageType_FetchEntries:
			return HandleFetchEntries(reader, writer);

		case CacheMessageType_CreateStatusFile:
			return HandleCreateStatusFile(reader, writer);

		default:
			return false;
		}
	}

	bool CacheServer::HandleStoreEntry(Connection& connection, BinaryReader& reader, BinaryWriter& writer)
	{
		CasKey cmdKey = reader.ReadCasKey();

		u64 outputCount = reader.Read7BitEncoded();
		u64 index = 0;

		Set<u32> inputs;
		u64 bytesForInput = 0;

		u64 outputStartOffset = reader.GetPosition();

		while (reader.GetLeft())
		{
			u32 offset = u32(reader.Read7BitEncoded());
			bool isInput = index++ >= outputCount;
			if (!isInput)
				continue;

			CasKey casKey;
			StringBuffer<> path;
			connection.m_casKeyTable.GetPathAndKey(path, casKey, connection.m_pathTable, offset);
			UBA_ASSERT(path.count);

			u32 pathOffset = m_pathTable.Add(path.data, path.count);
			u32 casKeyOffset = m_casKeyTable.Add(casKey, pathOffset);
			inputs.insert(casKeyOffset);
			bytesForInput += Get7BitEncodedCount(casKeyOffset);

			//m_logger.Info(TC("%s - %s%s"), path.data, CasKeyString(casKey).str, isInput ? TC("") : TC(" (output)"));
		}

		Vector<u8> inputCasKeyOffsets;
		{
			inputCasKeyOffsets.resize(bytesForInput);
			BinaryWriter w2(inputCasKeyOffsets.data(), 0, inputCasKeyOffsets.size());
			for (u32 input : inputs)
				w2.Write7BitEncoded(input);
		}

		SCOPED_WRITE_LOCK(m_cacheEntryLookupLock, lock);
		auto insres = m_cacheEntryLookup.try_emplace(cmdKey);
		auto& cacheEntries = insres.first->second;
		lock.Leave();

		SCOPED_WRITE_LOCK(cacheEntries.lock, lock2);
		
		List<CacheEntry>::iterator matchingEntry = cacheEntries.entries.end();
		for (auto i=cacheEntries.entries.begin(), e=cacheEntries.entries.end(); i!=e; ++i)
		{
			if (i->inputCasKeyOffsets != inputCasKeyOffsets)
				continue;
			matchingEntry = i;
			break;
		}

		// Already exists
		if (matchingEntry != cacheEntries.entries.end())
		{
			bool shouldOverwrite = false;
			Map<TString, CasKey> existing;

			BinaryReader r2(matchingEntry->outputCasKeyOffsets.data(), 0, matchingEntry->outputCasKeyOffsets.size());
			while (r2.GetLeft())
			{
				u32 existingOffset = u32(r2.Read7BitEncoded());
				CasKey casKey;
				StringBuffer<> path;
				m_casKeyTable.GetPathAndKey(path, casKey, m_pathTable, existingOffset);
				existing.try_emplace(path.data, casKey);
			}

			reader.SetPosition(outputStartOffset);
			u64 left = outputCount;
			while (left--)
			{
				u32 outputOffset = u32(reader.Read7BitEncoded());
				CasKey casKey;
				StringBuffer<> path;
				connection.m_casKeyTable.GetPathAndKey(path, casKey, connection.m_pathTable, outputOffset);

				auto findIt = existing.find(path.data);
				if (findIt == existing.end())
				{
					m_logger.Warning(TC("Existing cache entry matches input but does not match output (output file %s did not exist in existing cache entry)"), path.data);
					cacheEntries.entries.erase(matchingEntry);
					shouldOverwrite = true;
					break;
				}
				if (findIt->second != casKey)
				{
					m_logger.Warning(TC("Existing cache entry matches input but does not match output (%s has different caskey)"), path.data);
					cacheEntries.entries.erase(matchingEntry);
					shouldOverwrite = true;
					break;
				}
			}
			if (!shouldOverwrite)
				return true;
		}

		// Add new entry
		CacheEntry newEntry;
		newEntry.inputCasKeyOffsets.swap(inputCasKeyOffsets);

		Set<u32> outputs;
		u64 bytesForOutput = 0;

		bool hasAllContent = true;
		reader.SetPosition(outputStartOffset);
		u64 left = outputCount;
		while (left--)
		{
			u32 outputOffset = u32(reader.Read7BitEncoded());
			CasKey casKey;
			StringBuffer<> path;
			connection.m_casKeyTable.GetPathAndKey(path, casKey, connection.m_pathTable, outputOffset);
			u32 pathOffset = m_pathTable.Add(path.data, path.count);
			u32 casKeyOffset = m_casKeyTable.Add(casKey, pathOffset);
			outputs.insert(casKeyOffset);
			bytesForOutput += Get7BitEncodedCount(casKeyOffset);

			if (!m_storage.EnsureCasFile(casKey, nullptr))
			{
				writer.Write7BitEncoded(outputOffset);
				hasAllContent = false;
			}
		}

		newEntry.outputCasKeyOffsets.resize(bytesForOutput);
		BinaryWriter w2(newEntry.outputCasKeyOffsets.data(), 0, newEntry.outputCasKeyOffsets.size());
		for (u32 output : outputs)
			w2.Write7BitEncoded(output);


		newEntry.creationTime = GetSystemTimeAsFileTime();

		// If cache server has all content we can put the new cache entry directly in the lookup.. otherwise we'll have to wait until client has uploaded content
		if (hasAllContent)
		{
			cacheEntries.entries.emplace_front(std::move(newEntry));
		}
		else
		{
			SCOPED_WRITE_LOCK(connection.cacheEntryLookupLock, lock3);
			bool res = connection.cacheEntryLookup.try_emplace(cmdKey, std::move(newEntry)).second;
			UBA_ASSERT(res);(void)res;
		}

		//m_logger.Info(TC("Added new cache entry (%u inputs and %u outputs)"), u32(inputs.size()), outputCount);

		++m_addsSinceMaintenance;

		return true;
	}

	bool CacheServer::HandleFetchPathTable(BinaryReader& reader, BinaryWriter& writer)
	{
		u32 haveSize = reader.ReadU32();
		u32 size = m_pathTable.GetSize();
		writer.WriteU32(size);
		u32 toSend = Min(u32(writer.GetCapacityLeft()), size - haveSize);
		writer.WriteBytes(m_pathTable.GetMemory() + haveSize, toSend);
		return true;
	}

	bool CacheServer::HandleFetchCasTable(BinaryReader& reader, BinaryWriter& writer)
	{
		u32 haveSize = reader.ReadU32();
		u32 size = m_casKeyTable.GetSize();
		writer.WriteU32(size);
		u32 toSend = Min(u32(writer.GetCapacityLeft()), size - haveSize);
		writer.WriteBytes(m_casKeyTable.GetMemory() + haveSize, toSend);
		return true;
	}

	bool CacheServer::HandleFetchEntries(BinaryReader& reader, BinaryWriter& writer)
	{
		CasKey cmdKey = reader.ReadCasKey();

		u16& entryCount = *(u16*)writer.AllocWrite(2);
		entryCount = 0;

		SCOPED_READ_LOCK(m_cacheEntryLookupLock, lock);
		auto findIt = m_cacheEntryLookup.find(cmdKey);
		if (findIt == m_cacheEntryLookup.end())
			return true;
		auto& cacheEntries = findIt->second;
		lock.Leave();

		SCOPED_READ_LOCK(cacheEntries.lock, lock2);

		for (auto& entry : cacheEntries.entries)
		{
			auto& inputs = entry.inputCasKeyOffsets;
			auto& outputs = entry.outputCasKeyOffsets;

			u64 neededSize = Get7BitEncodedCount(inputs.size()) + inputs.size() + Get7BitEncodedCount(outputs.size()) + outputs.size();
			if (neededSize > writer.GetCapacityLeft())
				break;

			writer.Write7BitEncoded(inputs.size());
			writer.WriteBytes(inputs.data(), inputs.size());
			writer.Write7BitEncoded(outputs.size());
			writer.WriteBytes(outputs.data(), outputs.size());
			++entryCount;
		}
		return true;
	}

	bool CacheServer::HandleCreateStatusFile(BinaryReader& reader, BinaryWriter& writer)
	{
		StringBuffer<> filterString;
		reader.ReadString(filterString);

		SCOPED_READ_LOCK(m_maintenanceLock, lock);
		SCOPED_READ_LOCK(m_cacheEntryLookupLock, lock2);

		StringBuffer<> tempFile(m_storage.GetTempPath());
		Guid guid;
		CreateGuid(guid);
		tempFile.Append(GuidToString(guid).str);

		FileAccessor file(m_logger, tempFile.data);
		if (!file.CreateWrite())
			return false;

		bool success = true;
		auto Write = [&](const void* data, u64 size) { success &= file.Write(data, size); };

		u8 bom[] = {0xEF,0xBB,0xBF}; 
		Write(bom, sizeof(bom));

		auto writeLine = [&](const tchar* text)
			{
				u8 buffer[1024];
				BinaryWriter w(buffer, 0, sizeof(buffer));
				w.WriteUtf8String(text, TStrlen(text));
				w.WriteUtf8String(TC("\n"), 1);
				Write(buffer, w.GetPosition());
			};

		writeLine(TC("UbaCache server summary"));

		u64 now = GetSystemTimeAsFileTime();

		for (auto& kv : m_cacheEntryLookup)
		{
			CacheEntries& entries = kv.second;
			SCOPED_READ_LOCK(entries.lock, lock3);

			Set<u32> visibleIndices;
			if (filterString.count)
			{
				u32 index = 0;
				for (auto& entry : entries.entries)
				{
					auto findString = [&](const Vector<u8>& offsets)
						{
							BinaryReader reader2(offsets.data(), 0, offsets.size());
							while (reader2.GetLeft())
							{
								u64 offset = reader2.Read7BitEncoded();
								CasKey casKey;
								StringBuffer<> path;
								m_casKeyTable.GetPathAndKey(path, casKey, m_pathTable, offset);
								if (path.Contains(filterString.data))
									return true;
							}
							return false;
						};

					if (findString(entry.inputCasKeyOffsets) || findString(entry.outputCasKeyOffsets))
						visibleIndices.insert(index);
					++index;
				}
				if (visibleIndices.empty())
					continue;
			}


			StringBuffer<> line;
			writeLine(CasKeyString(kv.first).str);
			u32 index = 0;
			for (auto& entry : entries.entries)
			{
				if (!visibleIndices.empty() && visibleIndices.find(index) == visibleIndices.end())
				{
					++index;
					continue;
				}

				u64 age = MsToTime(GetFileTimeAsSeconds(now - entry.creationTime)*1000);
				writeLine(line.Clear().Appendf(TC("  #%u (%s ago)"), index, TimeToText(age, true).str).data);

				auto writeOffsets = [&](const Vector<u8>& offsets)
					{
						BinaryReader reader2(offsets.data(), 0, offsets.size());
						while (reader2.GetLeft())
						{
							u64 offset = reader2.Read7BitEncoded();
							CasKey casKey;
							StringBuffer<> path;
							m_casKeyTable.GetPathAndKey(path, casKey, m_pathTable, offset);
							writeLine(line.Clear().Appendf(TC("    %s - %s"), path.data, CasKeyString(casKey).str).data);
						}
					};

				writeLine(line.Clear().Append (TC("   Inputs:")).data);
				writeOffsets(entry.inputCasKeyOffsets);
				writeLine(line.Clear().Append (TC("   Outputs:")).data);
				writeOffsets(entry.outputCasKeyOffsets);
				++index;
			}
		}

		if (!success || !file.Close())
			return false;

		CasKey key;
		if (!m_storage.StoreCasFile(key, tempFile.data))
			return false;

		writer.WriteCasKey(key);

		DeleteFileW(tempFile.data);
		return true;
	}
}
