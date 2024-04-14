// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaCacheClient.h"
#include "UbaCompactTables.h"
#include "UbaFileAccessor.h"
#include "UbaNetworkMessage.h"
#include "UbaProcess.h"
#include "UbaRootPaths.h"
#include "UbaStorage.h"
#include "UbaStorageUtils.h"

#define UBA_LOG_CACHE_INFO 0

namespace uba
{
	struct CacheClient::Bucket
	{
		Bucket(u32 id_)
		:	id(id_)
		,	serverPathTable(CachePathTableMaxSize, CompactPathTable::V1)
		,	serverCasKeyTable(CacheCasKeyTableMaxSize)
		,	sendPathTable(CachePathTableMaxSize, CompactPathTable::V1)
		,	sendCasKeyTable(CacheCasKeyTableMaxSize)
		{
		}

		u32 id = 0;

		CompactPathTable serverPathTable;
		CompactCasKeyTable serverCasKeyTable;

		CompactPathTable sendPathTable;
		CompactCasKeyTable sendCasKeyTable;

		ReaderWriterLock pathTableNetworkLock;
		u32 pathTableSizeSent = 0;

		ReaderWriterLock casKeyTableNetworkLock;
		u32 casKeyTableSizeSent = 0;
	};

	CacheClient::CacheClient(LogWriter& writer, StorageImpl& storage, NetworkClient& client, Session& session)
	:	m_logger(writer, TC("UbaCacheClient"))
	,	m_storage(storage)
	,	m_client(client)
	,	m_session(session)
	{
		m_client.RegisterOnConnected([this]()
			{
				StackBinaryWriter<1024> writer;
				NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_Connect, writer);
				writer.WriteU32(CacheNetworkVersion);
				StackBinaryReader<1024> reader;
				if (!msg.Send(reader))
				{
					m_logger.Error(TC("Failed to connect to cache server. Version mismatch?"));
					return;
				}
				m_connected = true;
			});

		m_client.RegisterOnDisconnected([this]()
			{
				m_connected = false;
			});
	}

	CacheClient::~CacheClient() = default;

	bool CacheClient::WriteToCache(const RootPaths& rootPaths, u32 bucketId, const ProcessHandle& process)
	{
		if (!m_connected)
			return false;

		auto& si = process.GetStartInfo();
		if (!si.trackInputs)
			return false;

		CasKey cmdKey = GetCmdKey(rootPaths, si);
		if (cmdKey == CasKeyZero)
			return false;

		const Vector<u8>& inputs = process.GetTrackedInputs();
		BinaryReader inputsReader(inputs.data(), 0, inputs.size());

		const Vector<u8>& outputs = process.GetTrackedOutputs();
		BinaryReader outputsReader(outputs.data(), 0, outputs.size());

		Map<u32, u32> inputsStringToCasKey;
		Map<u32, u32> outputsStringToCasKey;
		u32 requiredPathTableSize = 0;
		u32 requiredCasTableSize = 0;
		bool success = true;

		SCOPED_WRITE_LOCK(m_bucketsLock, bucketsLock);
		Bucket& bucket = m_buckets.try_emplace(bucketId, bucketId).first->second;
		bucketsLock.Leave();

		// Traverse all inputs and outputs. to create cache entry that we can send to server
		while (true)
		{
			CasKey casKey;

			StringBuffer<512> path;
			bool isOutput = outputsReader.GetLeft();
			if (isOutput)
				outputsReader.ReadString(path);
			else if (inputsReader.GetLeft())
				inputsReader.ReadString(path);
			else
				break;
			
			TString temp2 = path.data;

			// For .exe and .dll we sometimes get relative paths so we need to expand them to full
			if (path[1] != ':' && (path.EndsWith(TC(".dll")) || path.EndsWith(TC(".exe"))))
			{
				tchar temp[512];
				bool res = SearchPathW(NULL, path.data, NULL, 512, temp, NULL);
				path.Clear().Append(temp);
				if (!res)
				{
					m_logger.Info(TC("Can't find file: %s"), path.data);
					return false;
				}
			}
			else if (path.EndsWith(TC(".rsp"))) // Paths can be absolute in rsp files so we need to normalize those paths
			{
				casKey = AsCompressed(rootPaths.NormalizeAndHashFile(m_logger, path.data), true);
			}
			else if (path[path.count-1] == ':')
			{
				m_logger.Info(TC("GOT UNKNOWN RELATIVE PATH: %s"), path.data);
				success = false;
				continue;
			}

			// Find root for path in order to be able to normalize it.
			auto root = rootPaths.FindRoot(path);
			if (!root)
			{
				m_logger.Info(TC("FILE WITHOUT ROOT: %s"), path.data);
				success = false;
				continue;
			}

			if (!root->includeInKey)
				continue;

			u32 rootLen = u32(root->path.size());
			TString qualifiedPath = path.data + rootLen - 1;
			qualifiedPath[0] = tchar(RootPaths::RootStartByte + root->index);

			u32 pathOffset = bucket.sendPathTable.Add(qualifiedPath.c_str(), u32(qualifiedPath.size()), &requiredPathTableSize);

			if (!isOutput) // Output files should be removed from input files.. For example when cl.exe compiles pch it reads previous pch file and we don't want it to be input
				if (outputsStringToCasKey.find(pathOffset) != outputsStringToCasKey.end())
					continue;

			auto insres = (isOutput ? outputsStringToCasKey : inputsStringToCasKey).try_emplace(pathOffset);
			
			if (!insres.second)
				continue;

			// .dep.json contains absolute paths, need to normalize file
			if (isOutput && path.EndsWith(TC(".dep.json"))) // TODO: More data driven approach. Also, hash does not match content atm.
				casKey = AsCompressed(rootPaths.NormalizeAndHashFile(m_logger, path.data), true);

			// Get file caskey using storage
			if (casKey == CasKeyZero)
			{
				bool deferCreation = true;
				if (!m_storage.StoreCasFile(casKey, path.data, CasKeyZero, deferCreation))
					return false;
				if (casKey == CasKeyZero) // If file is not found it was a temporary file that was deleted and is not really an output
					continue; // m_logger.Info(TC("This should never happen! (%s)"), path.data);
			}

			UBA_ASSERT(IsCompressed(casKey));
			insres.first->second = bucket.sendCasKeyTable.Add(casKey, pathOffset, &requiredCasTableSize);
		}

		if (!success)
			return false;

		if (outputsStringToCasKey.empty())
			m_logger.Warning(TC("NO OUTPUTS FROM process %s"), process.GetStartInfo().description); 

		// Make sure server has enough of the path table to be able to resolve offsets from cache entry
		if (!SendPathTable(bucket, requiredPathTableSize))
			return false;

		// Make sure server has enough of the cas table to be able to resolve offsets from cache entry
		if (!SendCasTable(bucket, requiredCasTableSize))
			return false;

		// actual cache entry now when we know server has the needed tables
		if (!SendCacheEntry(bucket, rootPaths, cmdKey, inputsStringToCasKey, outputsStringToCasKey))
			return false;


		#if UBA_LOG_CACHE_INFO
		m_logger.Info(TC("WRITECACHE: %s -> %u %s"), si.description, bucketId, CasKeyString(cmdKey).str);
		#endif

		return true;
	}

	bool CacheClient::FetchFromCache(const RootPaths& rootPaths, u32 bucketId, const ProcessStartInfo& info)
	{
		if (!m_connected)
			return false;

		CacheStats cacheStats;
		StorageStats storageStats;
		SystemStats systemStats;

		StorageStatsScope __(storageStats);
		SystemStatsScope _(systemStats);

		CasKey cmdKey = GetCmdKey(rootPaths, info);
		if (cmdKey == CasKeyZero)
			return false;

		u8 memory[SendMaxSize];

		u32 fetchId = m_session.CreateProcessId();
		m_session.GetTrace().CacheBeginFetch(fetchId, info.description);
		bool success = false;
		auto tg = MakeGuard([&]()
			{
				cacheStats.testEntries.time -= cacheStats.fetchCasTable.time;
				BinaryWriter writer(memory, 0, sizeof_array(memory));
				cacheStats.Write(writer);
				if (success)
				{
					storageStats.Write(writer);
					systemStats.Write(writer);
				}
				m_session.GetTrace().CacheEndFetch(fetchId, success, memory, writer.GetPosition());
			});

		BinaryReader reader(memory, 0, sizeof_array(memory));

		SCOPED_WRITE_LOCK(m_bucketsLock, bucketsLock);
		Bucket& bucket = m_buckets.try_emplace(bucketId, bucketId).first->second;
		bucketsLock.Leave();

		{
			TimerScope ts(cacheStats.fetchEntries);
			// Fetch entries.. server will provide as many as fits. TODO: Should it be possible to ask for more entries?
			StackBinaryWriter<32> writer;
			NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_FetchEntries, writer);
			writer.Write7BitEncoded(bucket.id);
			writer.WriteCasKey(cmdKey);
			if (!msg.Send(reader))
				return false;
		}


		UnorderedMap<u32, bool> offsetIsMatch;

		// Traverse entries and test inputs against local machine
		u32 entryCount = reader.ReadU16();

		#if UBA_LOG_CACHE_INFO
		auto mg = MakeGuard([&]() { m_logger.Info(TC("FETCHCACHE %s: %s -> %u %s (%u)"), success ? TC("SUCC") : TC("FAIL"), info.description, bucketId, CasKeyString(cmdKey).str, entryCount); });
		#endif

		for (u32 i=0; i!=entryCount; ++i)
		{
			u64 outputSize = 0;
			{
				TimerScope ts(cacheStats.testEntries);
				bool isMatch = true;
				u64 inputSize = reader.Read7BitEncoded();
				const u8* inputEnd = reader.GetPositionData() + inputSize;
				while (reader.GetPositionData() != inputEnd)
				{
					u32 casKeyOffset = u32(reader.Read7BitEncoded());

					auto insres = offsetIsMatch.try_emplace(casKeyOffset);
					if (insres.second)
					{
						if (casKeyOffset >= bucket.serverCasKeyTable.GetSize())
						{
							TimerScope ts2(cacheStats.fetchCasTable);
							if (!FetchCasTable(bucket))
								return false;
						}

						StringBuffer<MaxPath> path;
						CasKey cacheCasKey;
						if (!GetLocalPathAndCasKey(bucket, rootPaths, path, cacheCasKey, bucket.serverCasKeyTable, bucket.serverPathTable, casKeyOffset))
							return false;
						UBA_ASSERT(IsCompressed(cacheCasKey));

						CasKey localCasKey;
						if (path.EndsWith(TC(".rsp")) || path.EndsWith(TC(".dep.json"))) // Need to normalize caskey for these files since they contain absolute paths
						{
							localCasKey = AsCompressed(rootPaths.NormalizeAndHashFile(m_logger, path.data), true);
						}
						else
						{
							bool deferCreation = true;
							m_storage.StoreCasFile(localCasKey, path.data, CasKeyZero, deferCreation);
							UBA_ASSERT(localCasKey == CasKeyZero || IsCompressed(localCasKey));
						}

						insres.first->second = localCasKey == cacheCasKey;
					}

					if (!insres.first->second)
					{
						reader.Skip(inputEnd -  reader.GetPositionData());
						isMatch = false;
						break;
					}
				}

				outputSize = reader.Read7BitEncoded();

				// No match, test next entry
				if (!isMatch)
				{
					reader.Skip(outputSize);
					continue;
				}
			}

			// Fetch output files from cache (and some files need to be "denormalized" before written to disk

			const u8* outputEnd = reader.GetPositionData() + outputSize;
			while (reader.GetPositionData() != outputEnd)
			{
				u32 casKeyOffset = u32(reader.Read7BitEncoded());
				if (casKeyOffset >= bucket.serverCasKeyTable.GetSize())
					if (!FetchCasTable(bucket))
						return false;

				TimerScope ts(cacheStats.fetchOutput);

				StringBuffer<MaxPath> path;
				CasKey casKey;
				if (!GetLocalPathAndCasKey(bucket, rootPaths, path, casKey, bucket.serverCasKeyTable, bucket.serverPathTable, casKeyOffset))
					return false;
				UBA_ASSERT(IsCompressed(casKey));

				FileFetcher fetcher { m_storage.m_bufferSlots };

				if (path.EndsWith(TC(".dep.json")))
				{
					// Fetch into memory, file is in special format without absolute paths
					MemoryBlock normalizedBlock(1*1024*1024);
					if (!fetcher.RetrieveFile(m_logger, m_client, casKey, path.data, &normalizedBlock))
						return false;
					u32 rootOffsets = *(u32*)normalizedBlock.memory;
					char* fileStart = (char*)normalizedBlock.memory + sizeof(u32);

					// "denormalize" fetched file into another memory block that will be written to disk
					MemoryBlock localBlock(2*1024*1024);

					u64 lastWritten = 0;
					BinaryReader reader2(normalizedBlock.memory, rootOffsets, normalizedBlock.writtenSize);
					while (reader2.GetLeft())
					{
						u64 rootOffset = reader2.Read7BitEncoded();
						if (u64 toWrite = rootOffset - lastWritten)
							memcpy(localBlock.Allocate(toWrite, 1, TC("")), fileStart + lastWritten, toWrite);
						u8 rootIndex = fileStart[rootOffset] - RootPaths::RootStartByte;
						auto& root = rootPaths.GetRoot(rootIndex);

						#if PLATFORM_WINDOWS
						StringBuffer<> pathTemp;
						pathTemp.Append(root.path);
						char rootPath[512];
						u32 rootPathLen = pathTemp.Parse(rootPath, sizeof_array(rootPath));
						#else
						const char* rootPath = root.path.data();
						u32 rootPathLen = root.path.size();
						#endif

						if (u32 toWrite = rootPathLen - 1)
							memcpy(localBlock.Allocate(toWrite, 1, TC("")), rootPath, toWrite);
						lastWritten = rootOffset + 1;
					}

					u64 fileSize = rootOffsets - sizeof(u32);
					if (u64 toWrite = fileSize - lastWritten)
						memcpy(localBlock.Allocate(toWrite, 1, TC("")), fileStart + lastWritten, toWrite);

					FileAccessor destFile(m_logger, path.data);
					if (!destFile.CreateWrite())
						return false;
					if (!destFile.Write(localBlock.memory, localBlock.writtenSize))
						return false;
					if (!destFile.Close(&fetcher.m_lastWritten))
						return false;

					fetcher.m_size = fileSize;
					casKey = CalculateCasKey(localBlock.memory, localBlock.writtenSize, false, nullptr);
				}
				else
				{
					UBA_ASSERT(!m_session.ShouldStoreObjFilesCompressed()); // TODO: Implement Retrieve .obj that is not decompressing
					if (!fetcher.RetrieveFile(m_logger, m_client, casKey, path.data))
						return false;
				}
				if (!m_storage.FakeCopy(casKey, path.data, fetcher.m_size, fetcher.m_lastWritten, false))
					return false;
				if (!m_session.RegisterNewFile(path.data))
					return false;
			}

			success = true;
			return true;
		}

		return false;
	}

	bool CacheClient::WriteCacheSummary(const tchar* destinationFile, const tchar* filterString)
	{
		StackBinaryWriter<1024> writer;
		NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_CreateStatusFile, writer);
		writer.WriteString(filterString ? filterString : TC(""));
		StackBinaryReader<512> reader;
		if (!msg.Send(reader))
			return false;
		CasKey statusFileCasKey = reader.ReadCasKey();
		if (statusFileCasKey == CasKeyZero)
			return false;

		FileFetcher fetcher { m_storage.m_bufferSlots };
		if (!fetcher.RetrieveFile(m_logger, m_client, statusFileCasKey, destinationFile))
			return false;
		return true;
	}

	bool CacheClient::SendPathTable(Bucket& bucket, u32 requiredPathTableSize)
	{
		SCOPED_WRITE_LOCK(bucket.pathTableNetworkLock, lock);
		if (requiredPathTableSize <= bucket.pathTableSizeSent)
			return true;

		u32 left = requiredPathTableSize - bucket.pathTableSizeSent;
		while (left)
		{
			StackBinaryWriter<SendMaxSize> writer;
			NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_StorePathTable, writer);
			writer.Write7BitEncoded(bucket.id);
			u32 toSend = Min(requiredPathTableSize - bucket.pathTableSizeSent, u32(m_client.GetMessageMaxSize() - 32));
			left -= toSend;
			writer.WriteBytes(bucket.sendPathTable.GetMemory() + bucket.pathTableSizeSent, toSend);
			bucket.pathTableSizeSent += toSend;

			StackBinaryReader<16> reader;
			if (!msg.Send(reader))
				return false;
		}
		return true;
	}

	bool CacheClient::SendCasTable(Bucket& bucket, u32 requiredCasTableSize)
	{
		SCOPED_WRITE_LOCK(bucket.casKeyTableNetworkLock, lock);
		if (requiredCasTableSize <= bucket.casKeyTableSizeSent)
			return true;

		u32 left = requiredCasTableSize - bucket.casKeyTableSizeSent;
		while (left)
		{
			StackBinaryWriter<SendMaxSize> writer;
			NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_StoreCasTable, writer);
			writer.Write7BitEncoded(bucket.id);
			u32 toSend = Min(requiredCasTableSize - bucket.casKeyTableSizeSent, u32(m_client.GetMessageMaxSize() - 32));
			left -= toSend;
			writer.WriteBytes(bucket.sendCasKeyTable.GetMemory() + bucket.casKeyTableSizeSent, toSend);
			bucket.casKeyTableSizeSent += toSend;

			StackBinaryReader<16> reader;
			if (!msg.Send(reader))
				return false;
		}
		return true;
	}

	bool CacheClient::SendCacheEntry(Bucket& bucket, const RootPaths& rootPaths, const CasKey& cmdKey, const Map<u32, u32>& inputsStringToCasKey, const Map<u32, u32>& outputsStringToCasKey)
	{
		StackBinaryReader<1024> reader;
		{
			StackBinaryWriter<SendMaxSize> writer;

			NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_StoreEntry, writer);
			writer.Write7BitEncoded(bucket.id);
			writer.WriteCasKey(cmdKey);

			writer.Write7BitEncoded(outputsStringToCasKey.size());
			for (auto& kv : outputsStringToCasKey)
				writer.Write7BitEncoded(kv.second);

			for (auto& kv : inputsStringToCasKey)
				writer.Write7BitEncoded(kv.second);

			if (!msg.Send(reader))
				return false;
		}

		// Server has all content for caskeys.. upload is done
		if (!reader.GetLeft())
			return true;

		// There is content we need to upload to server
		while (reader.GetLeft())
		{
			u32 casKeyOffset = u32(reader.Read7BitEncoded());

			StringBuffer<MaxPath> path;
			CasKey casKey;
			if (!GetLocalPathAndCasKey(bucket, rootPaths, path, casKey, bucket.sendCasKeyTable, bucket.sendPathTable, casKeyOffset))
				return false;

			casKey = AsCompressed(casKey, true);

			StorageImpl::CasEntry* casEntry;
			if (m_storage.HasCasFile(casKey, &casEntry))
			{
				StringBuffer<> casKeyFileName;
				if (!m_storage.GetCasFileName(casKeyFileName, casKey))
					return false;

				const u8* fileData;
				u64 fileSize;

				MappedView mappedView;
				auto mapViewGuard = MakeGuard([&](){ m_storage.m_casDataBuffer.UnmapView(mappedView, path.data); });

				FileAccessor file(m_logger, casKeyFileName.data);

				if (casEntry->mappingHandle.IsValid()) // If file was created by helper it will be in the transient mapped memory
				{
					mappedView = m_storage.m_casDataBuffer.MapView(casEntry->mappingHandle, casEntry->mappingOffset, casEntry->mappingSize, path.data);
					fileData = mappedView.memory;
					fileSize = mappedView.size;
				}
				else
				{
					if (!file.OpenMemoryRead())
						return false;
					fileData = file.GetData();
					fileSize = file.GetSize();
				}

				if (!SendFile(m_logger, m_client, casKey, fileData, fileSize, casKeyFileName.data))
					return false;
			}
			else // If we don't have the cas key it should be one of the normalized files.... otherwise there is a bug
			{
				if (path.EndsWith(TC(".dep.json")))
				{
					FileAccessor file(m_logger, path.data);
					if (!file.OpenMemoryRead())
						return false;
					MemoryBlock block(AlignUp(file.GetSize(), 64*1024));
					u32& rootOffsetsStart = *(u32*)block.Allocate(sizeof(u32), 1, TC(""));
					rootOffsetsStart = 0;
					Vector<u32> rootOffsets;
					u32 rootOffsetsSize = 0;

					auto handleString = [&](const char* str, u64 strLen, u32 rootPos)
						{
							void* mem = block.Allocate(strLen, 1, TC(""));
							memcpy(mem, str, strLen);
							if (rootPos != ~0u)
							{
								rootOffsets.push_back(rootPos);
								rootOffsetsSize += Get7BitEncodedCount(rootPos);
							}
						};

					if (!rootPaths.NormalizeString<char>(m_logger, (const char*)file.GetData(), file.GetSize(), handleString, path.data))
						return false;

					if (!rootOffsets.empty())
					{
						u8* mem = (u8*)block.Allocate(rootOffsetsSize, 1, TC(""));
						rootOffsetsStart = u32(mem - block.memory);
						BinaryWriter writer(mem, 0, rootOffsetsSize);
						for (u32 rootOffset : rootOffsets)
							writer.Write7BitEncoded(rootOffset);
					}

					auto& s = m_storage;
					FileSender sender { m_logger, m_client, s.m_bufferSlots, s.Stats(), m_sendOneAtTheTimeLock, s.m_casCompressor, s.m_casCompressionLevel };
					if (!sender.SendFileCompressed(casKey, path.data, block.memory, block.writtenSize, TC("SendCacheEntry")))
						return m_logger.Error(TC("Failed to send cas content for file %s"), path.data);
				}
				else
				{
					return m_logger.Error(TC("Can't find output file %s to send to cache server"), path.data);
				}
			}

		}

		// Send done.. confirm to server
		StackBinaryWriter<SendMaxSize> writer;
		NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_StoreEntryDone, writer);
		writer.Write7BitEncoded(bucket.id);
		writer.WriteCasKey(cmdKey);
		if (!msg.Send(reader))
			return false;

		return true;
	}

	bool CacheClient::FetchCasTable(Bucket& bucket)
	{
		SCOPED_WRITE_LOCK(bucket.casKeyTableNetworkLock, lock);

		StackBinaryReader<SendMaxSize> reader;
		{
			//SCOPED_WRITE_LOCK(bucket.pathTableNetworkLock, lock);
			u32 targetSize = ~0u;
			while (bucket.serverCasKeyTable.GetSize() < targetSize)
			{
				StackBinaryWriter<16> writer;
				NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_FetchCasTable, writer);
				writer.Write7BitEncoded(bucket.id);
				writer.WriteU32(bucket.serverCasKeyTable.GetSize());

				reader.Reset();
				if (!msg.Send(reader))
					return false;
				u32 size = reader.ReadU32();
				if (targetSize == ~0u)
					targetSize = size;

				bucket.serverCasKeyTable.ReadMem(reader, false);
			}
		}
		{
			u32 targetSize = ~0u;
			while (bucket.serverPathTable.GetSize() < targetSize)
			{
				StackBinaryWriter<16> writer;
				NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_FetchPathTable, writer);
				writer.Write7BitEncoded(bucket.id);
				writer.WriteU32(bucket.serverPathTable.GetSize());

				reader.Reset();
				if (!msg.Send(reader))
					return false;
				u32 size = reader.ReadU32();
				if (targetSize == ~0u)
					targetSize = size;

				bucket.serverPathTable.ReadMem(reader, false);
			}
		}
		return true;
	}

	CasKey CacheClient::GetCmdKey(const RootPaths& rootPaths, const ProcessStartInfo& info)
	{
		CasKeyHasher hasher;

		// Add hash of application binary to key
		CasKey applicationCasKey;
		bool deferCreation = true;
		if (!m_storage.StoreCasFile(applicationCasKey, info.application, CasKeyZero, deferCreation))
			return CasKeyZero;
		hasher.Update(&applicationCasKey, sizeof(CasKey));

		// Add arguments list to key
		auto hashString = [&](const tchar* str, u64 strLen, u32 rootPos) { hasher.Update(str, strLen*sizeof(tchar)); };
		if (!rootPaths.NormalizeString(m_logger, info.arguments, TStrlen(info.arguments), hashString, TC("")))
			return CasKeyZero;

		// Add content of rsp file to key (This will cost a bit of perf since we need to normalize.. should this be part of key?)
		if (auto rspStart = TStrchr(info.arguments, '@'))
		{
			if (rspStart[1] == '"')
			{
				rspStart += 2;
				if (auto rspEnd = TStrchr(rspStart, '"'))
				{
					StringBuffer<> rsp;
					if (rspStart[1] != ':')
						rsp.Append(info.workingDir).EnsureEndsWithSlash();
					rsp.Append(rspStart, rspEnd - rspStart);
					CasKey rspCasKey = rootPaths.NormalizeAndHashFile(m_logger, rsp.data);
					hasher.Update(&rspCasKey, sizeof(CasKey));
				}
			}
		}

		return ToCasKey(hasher, false);
	}

	bool CacheClient::GetLocalPathAndCasKey(Bucket& bucket, const RootPaths& rootPaths, StringBufferBase& outPath, CasKey& outKey, CompactCasKeyTable& casKeyTable, CompactPathTable& pathTable, u32 offset)
	{
		if (!m_connected)
			return false;

		SCOPED_READ_LOCK(bucket.casKeyTableNetworkLock, lock); // TODO: Is this needed?

		StringBuffer<MaxPath> normalizedPath;
		casKeyTable.GetPathAndKey(normalizedPath, outKey, pathTable, offset);
		UBA_ASSERT(normalizedPath.count);

		u32 rootIndex = normalizedPath[0] - RootPaths::RootStartByte;
		auto& root = rootPaths.GetRoot(rootIndex);

		StringBuffer<MaxPath> path;
		outPath.Append(root.path).Append(normalizedPath.data + 1);
		return true;
	}
}
