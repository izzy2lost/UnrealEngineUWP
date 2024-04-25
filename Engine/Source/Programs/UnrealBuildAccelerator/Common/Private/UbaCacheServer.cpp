// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaCacheServer.h"
#include "UbaCompactTables.h"
#include "UbaBinaryReaderWriter.h"
#include "UbaFileAccessor.h"
#include "UbaNetworkServer.h"
#include "UbaStorageServer.h"
//#include <oodle2.h>

namespace uba
{
	static constexpr u32 CacheFileVersion = 2;

	bool IsCaseInsensitive(u64 id) { return (id & (1ull << 32)) == 0; }

	struct CacheServer::CacheEntry
	{
		u64 creationTime;
		Vector<u8> inputCasKeyOffsets;
		Vector<u8> outputCasKeyOffsets;
	};

	struct CacheServer::ConnectionBucket
	{
		ConnectionBucket(u64 i) : pathTable(CachePathTableMaxSize, CompactPathTable::V1, IsCaseInsensitive(i)), casKeyTable(CacheCasKeyTableMaxSize), id(i) {}
		CompactPathTable pathTable;
		CompactCasKeyTable casKeyTable;

		ReaderWriterLock cacheEntryLookupLock;
		UnorderedMap<CasKey, CacheEntry> cacheEntryLookup;
		u64 id;
	};

	struct CacheServer::Connection
	{
		UnorderedMap<u64, ConnectionBucket> buckets;
	};

	struct CacheServer::CacheEntries
	{
		ReaderWriterLock lock;
		List<CacheEntry> entries;
	};

	struct CacheServer::Bucket
	{
		Bucket(u64 id) : m_pathTable(CachePathTableMaxSize, CompactPathTable::V1, IsCaseInsensitive(id)), m_casKeyTable(CacheCasKeyTableMaxSize) {}
		ReaderWriterLock m_cacheEntryLookupLock;
		UnorderedMap<CasKey, CacheEntries> m_cacheEntryLookup;

		CompactPathTable m_pathTable;
		CompactCasKeyTable m_casKeyTable;

		u64 totalEntryCount = 0;
		u64 totalEntrySize = 0;
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
		{
			m_logger.Detail(TC("No database found. Starting a new one at %s"), fileName.data);
			return true;
		}
		BinaryReader reader(file.GetData(), 0, file.GetSize());

		u32 version = reader.ReadU32();
		if (version != CacheFileVersion)
			return true;

		u32 totalPathTableSize = 0;
		u32 totalCasKeyTableSize = 0;
		u64 totalCacheEntryCount = 0;

		u32 bucketCount = reader.ReadU32();
		while (bucketCount--)
		{
			u64 id = reader.ReadU64();
			Bucket& bucket = m_buckets.try_emplace(id, id).first->second;

			u32 pathTableSize = reader.ReadU32();
			if (pathTableSize)
			{
				BinaryReader pathTableReader(reader.GetPositionData(), 0, pathTableSize);
				bucket.m_pathTable.ReadMem(pathTableReader, true);
				reader.Skip(pathTableSize);
			}
			totalPathTableSize += pathTableSize;

			u32 casKeyTableSize = reader.ReadU32();
			if (casKeyTableSize)
			{
				BinaryReader casKeyTableReader(reader.GetPositionData(), 0, casKeyTableSize);
				bucket.m_casKeyTable.ReadMem(casKeyTableReader, true);
				reader.Skip(casKeyTableSize);
			}
			totalCasKeyTableSize += casKeyTableSize;

			u32 entryLookupCount = reader.ReadU32();
			bucket.m_cacheEntryLookup.reserve(entryLookupCount);

			while (entryLookupCount--)
			{
				auto insres = bucket.m_cacheEntryLookup.try_emplace(reader.ReadCasKey());
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
		}

		u64 duration = GetTime() - startTime;
		m_logger.Detail(TC("Database loaded from %s in %s (%llu bucket(s) containing %s paths, %s keys, %llu cache entries)"), fileName.data, TimeToText(duration).str, m_buckets.size(), BytesToText(totalPathTableSize).str, BytesToText(totalCasKeyTableSize).str, totalCacheEntryCount);
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

		Write(u32(m_buckets.size()));

		for (auto& kv : m_buckets)
		{
			Bucket& bucket = kv.second;

			Write(kv.first);

			u32 pathTableSize = bucket.m_pathTable.GetSize();
			Write(pathTableSize);
			WriteBytes(bucket.m_pathTable.GetMemory(), pathTableSize);

			u32 casKeyTableSize = bucket.m_casKeyTable.GetSize();
			Write(casKeyTableSize);
			WriteBytes(bucket.m_casKeyTable.GetMemory(), casKeyTableSize);

			u32 entryLookupCount = u32(bucket.m_cacheEntryLookup.size());
			Write(entryLookupCount);

			for (auto& kv2 : bucket.m_cacheEntryLookup)
			{
				Write(kv2.first);

				u32 cacheEntryCount = u32(kv2.second.entries.size());
				Write(cacheEntryCount);

				for (CacheEntry& entry : kv2.second.entries)
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
		u64 totalCasCount = existingCas.size() + deletedCasCount;

		u64 now = GetSystemTimeAsFileTime();
		u64 oldest = now;


		Atomic<u64> deleteEntryCount;

		u64 deleteCacheEntriesStartTime = GetTime();
		do
		{
			oldest = now;

			ReaderWriterLock existingCasLock;

			m_server.ParallelFor(16, m_buckets, [&](auto& it)
			{
				Vector<u64*> touchedCas;

				Bucket& bucket = it->second;
				bucket.totalEntryCount = 0;
				bucket.totalEntrySize = 0;

				for (auto li=bucket.m_cacheEntryLookup.begin(), le=bucket.m_cacheEntryLookup.end(); li!=le;)
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
								bucket.m_casKeyTable.GetKey(casKey, offset);
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
								bucket.m_casKeyTable.GetKey(casKey, offset);
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

						++bucket.totalEntryCount;
						bucket.totalEntrySize += neededSize;

						capacityLeft -= neededSize;

						SCOPED_WRITE_LOCK(existingCasLock, l);
						if (!oldest || entry.creationTime < oldest)
							oldest = entry.creationTime;

						for (u64* v : touchedCas)
							++(*v);
						l.Leave();

						++i;
					}

					if (!entries.entries.empty())
					{
						++li;
						continue;
					}

					li = bucket.m_cacheEntryLookup.erase(li);
					le = bucket.m_cacheEntryLookup.end();
				}
			});

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

		m_logger.Detail(TC("  Deleted %llu cas files and %llu cache entries (%s)"), deletedCasCount, deleteEntryCount.load(), TimeToText(GetTime() - deleteCacheEntriesStartTime).str);

		Atomic<u32> bucketCounter;
		m_server.ParallelFor(16, m_buckets, [&](auto& it)
		{
			u64 bucketStartTime = GetTime();

			Bucket& bucket = it->second;
			u32 bucketIndex = bucketCounter++;

			if (deleteEntryCount || forceAllSteps)
			{
				UnorderedSet<u32> usedCasKeyOffsets;

				// Collect all caskeys that are used by cache entries.
				for (auto& kv2 : bucket.m_cacheEntryLookup)
				{
					for (auto& entry : kv2.second.entries)
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
					BinaryReader reader2(bucket.m_casKeyTable.GetMemory(), casKeyOffset, bucket.m_casKeyTable.GetSize());
					u32 pathOffset = u32(reader2.Read7BitEncoded());
					usedPathOffsets.insert(pathOffset);
				}

				// Build new path table based on used offsets
				UnorderedMap<u32, u32> oldToNewPathOffset;
				u32 oldSize = bucket.m_pathTable.GetSize();
				{
					CompactPathTable newPathTable(CachePathTableMaxSize, CompactPathTable::V1, bucket.m_pathTable.GetPathCount(), bucket.m_pathTable.GetSegmentCount());
					oldToNewPathOffset.reserve(usedPathOffsets.size());

					for (u32 pathOffset : usedPathOffsets)
					{
						StringBuffer<> temp;
						bucket.m_pathTable.GetString(temp, pathOffset);
						u32 newOffset = newPathTable.AddNoLock(temp.data, temp.count);

						#if 0
						StringBuffer<> test;
						newPathTable.GetString(test, newOffset);
						UBA_ASSERT(test.Equals(temp.data));
						#endif

						auto res = oldToNewPathOffset.try_emplace(pathOffset, newOffset);
						UBA_ASSERT(res.second);(void)res;
					}
					bucket.m_pathTable.Swap(newPathTable);
				}
				m_logger.Detail(TC("    Bucket %u Recreated path table. %s -> %s (%s)"), bucketIndex, BytesToText(oldSize).str, BytesToText(bucket.m_pathTable.GetSize()).str, TimeToText(GetTime() - recreatePathTableStart).str);


				// Build new caskey table based on used offsets
				u64 recreateCasKeyTableStart = GetTime();
				UnorderedMap<u32, u32> oldToNewCasKeyOffset;
				oldSize = bucket.m_casKeyTable.GetSize();
				{
					oldToNewCasKeyOffset.reserve(usedCasKeyOffsets.size());
					CompactCasKeyTable newCasKeyTable(CacheCasKeyTableMaxSize, usedCasKeyOffsets.size());
					for (u32 casKeyOffset : usedCasKeyOffsets)
					{
						BinaryReader reader2(bucket.m_casKeyTable.GetMemory(), casKeyOffset, bucket.m_casKeyTable.GetSize());
						u32 oldPathOffset = u32(reader2.Read7BitEncoded());
						CasKey casKey = reader2.ReadCasKey();
						auto findIt = oldToNewPathOffset.find(oldPathOffset);
						UBA_ASSERT(findIt != oldToNewPathOffset.end());
						auto res = oldToNewCasKeyOffset.try_emplace(casKeyOffset, newCasKeyTable.Add(casKey, findIt->second));
						UBA_ASSERT(res.second);(void)res;
					}
					bucket.m_casKeyTable.Swap(newCasKeyTable);
				}
				m_logger.Detail(TC("    Bucket %u Recreated caskey table. %s -> %s (%s)"), bucketIndex, BytesToText(oldSize).str, BytesToText(bucket.m_casKeyTable.GetSize()).str, TimeToText(GetTime() - recreateCasKeyTableStart).str);


				// Update all casKeyOffsets
				u64 updateEntriesStart = GetTime();

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
								bucket.m_casKeyTable.GetPathAndKey(temp, key, bucket.m_pathTable, newOffset);
								m_logger.Info(TC("Found duplicate of %s in cache entry"), temp.data);
							}
						}

						offsets.resize(newOffsetsSize);
						BinaryWriter writer2(offsets.data(), 0, newOffsetsSize);
						for (u32 offset : newOffsets)
							writer2.Write7BitEncoded(offset);
						UBA_ASSERT(writer2.GetPosition() == newOffsetsSize);
					};

				m_server.ParallelFor(16, bucket.m_cacheEntryLookup, [&](auto& it)
					{
						CacheEntries& entries = it->second;(void)entries;
						for (auto& entry : entries.entries)
						{
							updateCasKeyOffsets(entry.inputCasKeyOffsets);
							updateCasKeyOffsets(entry.outputCasKeyOffsets);
						}
					});

				#if 0
				u8* mem = bucket.m_pathTable.GetMemory();
				u64 memLeft = bucket.m_pathTable.GetSize();
				while (memLeft)
				{
					u8 buffer[256*1024];
					auto compressor = OodleLZ_Compressor_Kraken;
					auto compressionLevel = OodleLZ_CompressionLevel_SuperFast;
					u64 toCompress = Min(memLeft, u64(256*1024 - 128));
					auto compressedBlockSize = OodleLZ_Compress(compressor, mem, (OO_SINTa)toCompress, buffer, compressionLevel);
					(void)compressedBlockSize;
					memLeft -= toCompress;
				}
				#endif


				m_logger.Detail(TC("    Bucket %u Updated cache entries with new tables (%s)"), bucketIndex, TimeToText(GetTime() - updateEntriesStart).str);
			}

			m_logger.Info(TC("    Bucket %u Done (%s). CacheEntries: %llu (%s) PathTable: %s CasTable: %s"), bucketIndex, TimeToText(GetTime() - bucketStartTime).str, bucket.totalEntryCount, BytesToText(bucket.totalEntrySize).str, BytesToText(bucket.m_pathTable.GetSize()).str, BytesToText(bucket.m_casKeyTable.GetSize()).str);
		});

		if (entriesAdded || deletedCasCount || deleteEntryCount || forceAllSteps)
		{
			u64 saveStart = GetTime();
			m_storage.SaveCasTable(false, false);
			SaveNoLock();
			m_logger.Detail(TC("  Saved to disk (%s)"), TimeToText(GetTime() - saveStart).str);
		}

		u64 oldestTime = MsToTime(GetFileTimeAsSeconds(now - oldest)*1000);
		m_logger.Info(TC("Maintenance done! (%s) CasFiles: %llu (%s) OldestEntry: %s"), TimeToText(GetTime() - startTime).str, totalCasCount - deletedCasCount, BytesToText(totalCasSize).str, TimeToText(oldestTime, true).str);

		return true;
	}

	void CacheServer::OnDisconnected(u32 clientId)
	{
		SCOPED_WRITE_LOCK(m_connectionsLock, lock);
		m_connections.erase(clientId);
		lock.Leave();
	}

	CacheServer::ConnectionBucket& CacheServer::GetConnectionBucket(const ConnectionInfo& connectionInfo, BinaryReader& reader)
	{
		u64 id = reader.Read7BitEncoded();
		SCOPED_WRITE_LOCK(m_connectionsLock, lock);
		auto& connection = m_connections[connectionInfo.GetId()];
		return connection.buckets.try_emplace(id, id).first->second;
	}

	CacheServer::Bucket& CacheServer::GetBucket(BinaryReader& reader)
	{
		u64 id = reader.Read7BitEncoded();
		SCOPED_WRITE_LOCK(m_bucketsLock, bucketsLock);
		return m_buckets.try_emplace(id, id).first->second;
		
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
			m_connections.try_emplace(connectionInfo.GetId());
			return true;
		}
		case CacheMessageType_StorePathTable:
		{
			GetConnectionBucket(connectionInfo, reader).pathTable.ReadMem(reader, false);
			return true;
		}
		case CacheMessageType_StoreCasTable:
		{
			GetConnectionBucket(connectionInfo, reader).casKeyTable.ReadMem(reader, false);
			return true;
		}
		case CacheMessageType_StoreEntry:
		{
			auto& bucket = GetConnectionBucket(connectionInfo, reader);
			return HandleStoreEntry(bucket, reader, writer);
		}
		case CacheMessageType_StoreEntryDone:
		{
			auto& connectionBucket = GetConnectionBucket(connectionInfo, reader);
			CasKey cmdKey = reader.ReadCasKey();

			SCOPED_WRITE_LOCK(connectionBucket.cacheEntryLookupLock, lock2);
			auto findIt = connectionBucket.cacheEntryLookup.find(cmdKey);
			if (findIt != connectionBucket.cacheEntryLookup.end())
			{
				u64 id = connectionBucket.id;
				SCOPED_WRITE_LOCK(m_bucketsLock, bucketsLock);
				Bucket& bucket = m_buckets.try_emplace(id, id).first->second;
				bucketsLock.Leave();

				SCOPED_WRITE_LOCK(bucket.m_cacheEntryLookupLock, lock3);
				auto insres = bucket.m_cacheEntryLookup.try_emplace(cmdKey);
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

	bool CacheServer::HandleStoreEntry(ConnectionBucket& connectionBucket, BinaryReader& reader, BinaryWriter& writer)
	{
		CasKey cmdKey = reader.ReadCasKey();

		u64 outputCount = reader.Read7BitEncoded();
		u64 index = 0;

		Set<u32> inputs;
		u64 bytesForInput = 0;

		u64 outputStartOffset = reader.GetPosition();
		u64 id = connectionBucket.id;
		SCOPED_WRITE_LOCK(m_bucketsLock, bucketsLock);
		Bucket& bucket = m_buckets.try_emplace(id, id).first->second;
		bucketsLock.Leave();

		while (reader.GetLeft())
		{
			u32 offset = u32(reader.Read7BitEncoded());
			bool isInput = index++ >= outputCount;
			if (!isInput)
				continue;

			CasKey casKey;
			StringBuffer<> path;
			connectionBucket.casKeyTable.GetPathAndKey(path, casKey, connectionBucket.pathTable, offset);
			UBA_ASSERT(path.count);

			u32 pathOffset = bucket.m_pathTable.Add(path.data, path.count);

			#if 0
			StringBuffer<> test;
			bucket.m_pathTable.GetString(test, pathOffset);
			UBA_ASSERT(test.Equals(path.data));
			#endif

			u32 casKeyOffset = bucket.m_casKeyTable.Add(casKey, pathOffset);
			auto insres = inputs.insert(casKeyOffset);
			if (!insres.second)
			{
				m_logger.Warning(TC("Input file %s exists more than once in cache entry"), path.data);
				continue;
			}
			bytesForInput += Get7BitEncodedCount(casKeyOffset);

			//m_logger.Info(TC("%s - %s"), path.data, CasKeyString(casKey).str);
		}

		Vector<u8> inputCasKeyOffsets;
		{
			inputCasKeyOffsets.resize(bytesForInput);
			BinaryWriter w2(inputCasKeyOffsets.data(), 0, inputCasKeyOffsets.size());
			for (u32 input : inputs)
				w2.Write7BitEncoded(input);
		}

		SCOPED_WRITE_LOCK(bucket.m_cacheEntryLookupLock, lock);
		auto insres = bucket.m_cacheEntryLookup.try_emplace(cmdKey);
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
				bucket.m_casKeyTable.GetPathAndKey(path, casKey, bucket.m_pathTable, existingOffset);
				existing.try_emplace(path.data, casKey);
			}

			reader.SetPosition(outputStartOffset);
			u64 left = outputCount;
			while (left--)
			{
				u32 outputOffset = u32(reader.Read7BitEncoded());
				CasKey casKey;
				StringBuffer<> path;
				connectionBucket.casKeyTable.GetPathAndKey(path, casKey, connectionBucket.pathTable, outputOffset);

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
			connectionBucket.casKeyTable.GetPathAndKey(path, casKey, connectionBucket.pathTable, outputOffset);
			u32 pathOffset = bucket.m_pathTable.Add(path.data, path.count);

			#if 0
			StringBuffer<> test;
			bucket.m_pathTable.GetString(test, pathOffset);
			UBA_ASSERT(test.Equals(path.data));
			#endif

			u32 casKeyOffset = bucket.m_casKeyTable.Add(casKey, pathOffset);
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
			SCOPED_WRITE_LOCK(connectionBucket.cacheEntryLookupLock, lock3);
			bool res = connectionBucket.cacheEntryLookup.try_emplace(cmdKey, std::move(newEntry)).second;
			UBA_ASSERT(res);(void)res;
		}

		//m_logger.Info(TC("Added new cache entry (%u inputs and %u outputs)"), u32(inputs.size()), outputCount);

		++m_addsSinceMaintenance;

		return true;
	}

	bool CacheServer::HandleFetchPathTable(BinaryReader& reader, BinaryWriter& writer)
	{
		Bucket& bucket = GetBucket(reader);
		u32 haveSize = reader.ReadU32();
		u32 size = bucket.m_pathTable.GetSize();
		writer.WriteU32(size);
		u32 toSend = Min(u32(writer.GetCapacityLeft()), size - haveSize);
		writer.WriteBytes(bucket.m_pathTable.GetMemory() + haveSize, toSend);
		return true;
	}

	bool CacheServer::HandleFetchCasTable(BinaryReader& reader, BinaryWriter& writer)
	{
		Bucket& bucket = GetBucket(reader);
		u32 haveSize = reader.ReadU32();
		u32 size = bucket.m_casKeyTable.GetSize();
		writer.WriteU32(size);
		u32 toSend = Min(u32(writer.GetCapacityLeft()), size - haveSize);
		writer.WriteBytes(bucket.m_casKeyTable.GetMemory() + haveSize, toSend);
		return true;
	}

	bool CacheServer::HandleFetchEntries(BinaryReader& reader, BinaryWriter& writer)
	{
		Bucket& bucket = GetBucket(reader);
		CasKey cmdKey = reader.ReadCasKey();

		u16& entryCount = *(u16*)writer.AllocWrite(2);
		entryCount = 0;

		SCOPED_READ_LOCK(bucket.m_cacheEntryLookupLock, lock);
		auto findIt = bucket.m_cacheEntryLookup.find(cmdKey);
		if (findIt == bucket.m_cacheEntryLookup.end())
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

		SCOPED_READ_LOCK(m_bucketsLock, bucketsLock);
		for (auto& kv : m_buckets)
		{
			Bucket& bucket = kv.second;
			SCOPED_READ_LOCK(bucket.m_cacheEntryLookupLock, lock2);

			for (auto& kv2 : bucket.m_cacheEntryLookup)
			{
				CacheEntries& entries = kv2.second;
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
									bucket.m_casKeyTable.GetPathAndKey(path, casKey, bucket.m_pathTable, offset);
									if (path.Contains(filterString.data))
										return true;
									if (Contains(CasKeyString(casKey).str, filterString.data))
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
				writeLine(CasKeyString(kv2.first).str);
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
								bucket.m_casKeyTable.GetPathAndKey(path, casKey, bucket.m_pathTable, offset);
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
