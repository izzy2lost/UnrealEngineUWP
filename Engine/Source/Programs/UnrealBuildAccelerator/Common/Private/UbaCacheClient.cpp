// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaCacheClient.h"
#include "UbaFileAccessor.h"
#include "UbaNetworkMessage.h"
#include "UbaProcess.h"
#include "UbaStorage.h"
#include "UbaStorageUtils.h"

#if PLATFORM_WINDOWS
#include <shlobj_core.h>
#endif

namespace uba
{
	static constexpr u8 RootStartByte = ' ';

	struct CacheClient::Root
	{
		TString path;
		StringKey shortestPathKey;
		bool includeInKey;
	};

	CacheClient::CacheClient(LogWriter& writer, StorageImpl& storage, NetworkClient& client, Session& session)
	:	m_logger(writer, TC("UbaCacheClient"))
	,	m_storage(storage)
	,	m_client(client)
	,	m_session(session)
	,	m_serverPathTable(CachePathTableMaxSize)
	,	m_serverCasKeyTable(CacheCasKeyTableMaxSize)
	,	m_sendPathTable(8*1024*1024)
	,	m_sendCasKeyTable(8*1024*1024)
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

	bool CacheClient::RegisterRoot(const tchar* rootPath, bool includeInKey)
	{
		// Register rootPath both with single path separators and double path separators because text files store them with double path separators

		StringBuffer<> doubleSlash;
		for (const tchar* it=rootPath; *it; ++it)
		{
			doubleSlash.Append(*it);
			if (*it == PathSeparator)
				doubleSlash.Append(PathSeparator);
		}

		const tchar* rootPaths[] = { rootPath, doubleSlash.data };
		for (const tchar* rp : rootPaths)
		{
			if (m_roots.size() == '~' - ' ') // This is not really true.. as long as value is under 256 we're good
				return m_logger.Error(TC("Too many roots added (%llu)"), m_roots.size());

			auto& root = m_roots.emplace_back();
			root.path = rp;

			ToLower(root.path.data());
			if (root.path[root.path.size()-1] != PathSeparator)
				return m_logger.Error(TC("Root path must end with separator"));

			root.includeInKey = includeInKey;

			m_longestRoot = Max(u32(root.path.size()), m_longestRoot);

			if (!m_shortestRoot || root.path.size() < m_shortestRoot)
			{
				m_shortestRoot = u32(root.path.size());
				for (auto& r : m_roots)
					r.shortestPathKey = ToStringKeyNoCheck(r.path.data(), m_shortestRoot);
			}
			else
				root.shortestPathKey = ToStringKeyNoCheck(root.path.data(), m_shortestRoot);
		}
		return true;
	}

	bool CacheClient::RegisterSystemRoots()
	{
		#if PLATFORM_WINDOWS
		StringBuffer<MaxPath> dir;
		dir.count = GetSystemDirectory(dir.data, dir.capacity);
		RegisterRoot(dir.EnsureEndsWithSlash().data, false); // Ignore files from here.. we do expect them not to affect the output of a process
		
		dir.count = GetEnvironmentVariable(TC("ProgramW6432"), dir.Clear().data, dir.capacity);
		RegisterRoot(dir.EnsureEndsWithSlash().data, true);

		dir.count = GetEnvironmentVariable(TC("ProgramFiles(x86)"), dir.Clear().data, dir.capacity);
		RegisterRoot(dir.EnsureEndsWithSlash().data, true);

		dir.count = GetEnvironmentVariable(TC("ProgramFiles(x86)"), dir.Clear().data, dir.capacity);
		RegisterRoot(dir.EnsureEndsWithSlash().data, true);

		PWSTR path;
		if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &path)))
			return false;
		RegisterRoot(dir.Clear().Append(path).EnsureEndsWithSlash().data, true);
		CoTaskMemFree(path);

		#else
		UBA_ASSERT(false);
		#endif
		return true;
	}

	bool CacheClient::WriteToCache(const ProcessHandle& process)
	{
		if (!m_connected)
			return false;

		auto& si = process.GetStartInfo();
		if (!si.trackInputs)
			return false;

		CasKey cmdKey = GetCmdKey(si);
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
				casKey = AsCompressed(NormalizeAndHashFile(path.data), true);
			}
			else if (path[path.count-1] == ':')
			{
				m_logger.Info(TC("GOT UNKNOWN RELATIVE PATH: %s"), path.data);
				success = false;
				continue;
			}

			// Find root for path in order to be able to normalize it.
			u32 rootIndex = FindRootIndex(path);
			if (rootIndex == ~0u)
			{
				m_logger.Info(TC("FILE WITHOUT ROOT: %s"), path.data);
				success = false;
				continue;
			}

			auto& root = m_roots[rootIndex];
			if (!root.includeInKey)
				continue;

			u32 rootLen = u32(root.path.size());
			TString qualifiedPath = path.data + rootLen - 2;
			qualifiedPath[0] = tchar(RootStartByte + rootIndex);

			u32 pathOffset = m_sendPathTable.Add(qualifiedPath.c_str(), u32(qualifiedPath.size()), &requiredPathTableSize);

			if (!isOutput) // Output files should be removed from input files.. For example when cl.exe compiles pch it reads previous pch file and we don't want it to be input
				if (outputsStringToCasKey.find(pathOffset) != outputsStringToCasKey.end())
					continue;

			auto insres = (isOutput ? outputsStringToCasKey : inputsStringToCasKey).try_emplace(pathOffset);
			
			if (!insres.second)
				continue;

			// .dep.json contains absolute paths, need to normalize file
			if (isOutput && path.EndsWith(TC(".dep.json"))) // TODO: More data driven approach. Also, hash does not match content atm.
				casKey = AsCompressed(NormalizeAndHashFile(path.data), true);

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
			insres.first->second = m_sendCasKeyTable.Add(casKey, pathOffset, &requiredCasTableSize);
		}

		if (!success)
			return false;

		if (outputsStringToCasKey.empty())
			m_logger.Warning(TC("NO OUTPUTS FROM process %s"), process.GetStartInfo().description); 

		// Make sure server has enough of the path table to be able to resolve offsets from cache entry
		if (!SendPathTable(requiredPathTableSize))
			return false;

		// Make sure server has enough of the cas table to be able to resolve offsets from cache entry
		if (!SendCasTable(requiredCasTableSize))
			return false;

		// actual cache entry now when we know server has the needed tables
		if (!SendCacheEntry(cmdKey, inputsStringToCasKey, outputsStringToCasKey))
			return false;

		return true;
	}

	bool CacheClient::FetchFromCache(const ProcessStartInfo& info)
	{
		if (!m_connected)
			return false;

		CasKey cmdKey = GetCmdKey(info);
		if (cmdKey == CasKeyZero)
			return false;

		StackBinaryReader<SendMaxSize> reader;

		{
			// Fetch entries.. server will provide as many as fits. TODO: Should it be possible to ask for more entries?
			StackBinaryWriter<32> writer;
			NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_FetchEntries, writer);
			writer.WriteCasKey(cmdKey);
			if (!msg.Send(reader))
				return false;
		}


		UnorderedMap<u32, bool> offsetIsMatch;

		// Traverse entries and test inputs against local machine
		u32 entryCount = reader.ReadU16();
		for (u32 i=0; i!=entryCount; ++i)
		{
			bool isMatch = true;
			u64 inputSize = reader.Read7BitEncoded();
			const u8* inputEnd = reader.GetPositionData() + inputSize;
			while (reader.GetPositionData() != inputEnd)
			{
				u32 casKeyOffset = u32(reader.Read7BitEncoded());

				auto insres = offsetIsMatch.try_emplace(casKeyOffset);
				if (insres.second)
				{
					if (casKeyOffset >= m_serverCasKeyTable.GetSize())
						if (!FetchCasTable())
							return false;

					StringBuffer<MaxPath> path;
					CasKey cacheCasKey;
					if (!GetLocalPathAndCasKey(path, cacheCasKey, m_serverCasKeyTable, m_serverPathTable, casKeyOffset))
						return false;
					UBA_ASSERT(IsCompressed(cacheCasKey));

					CasKey localCasKey;
					if (path.EndsWith(TC(".rsp")) || path.EndsWith(TC(".dep.json"))) // Need to normalize caskey for these files since they contain absolute paths
					{
						localCasKey = AsCompressed(NormalizeAndHashFile(path.data), true);
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

			u64 outputSize = reader.Read7BitEncoded();

			// No match, test next entry
			if (!isMatch)
			{
				reader.Skip(outputSize);
				continue;
			}

			// Fetch output files from cache (and some files need to be "denormalized" before written to disk

			const u8* outputEnd = reader.GetPositionData() + outputSize;
			while (reader.GetPositionData() != outputEnd)
			{
				u32 casKeyOffset = u32(reader.Read7BitEncoded());
				if (casKeyOffset >= m_serverCasKeyTable.GetSize())
					if (!FetchCasTable())
						return false;

				StringBuffer<MaxPath> path;
				CasKey casKey;
				if (!GetLocalPathAndCasKey(path, casKey, m_serverCasKeyTable, m_serverPathTable, casKeyOffset))
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
						u8 rootIndex = fileStart[rootOffset] - RootStartByte;
						auto& root = m_roots[rootIndex];

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
						lastWritten = rootOffset + 2;
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

	bool CacheClient::SendPathTable(u32 requiredPathTableSize)
	{
		SCOPED_WRITE_LOCK(m_pathTableNetworkLock, lock);
		if (requiredPathTableSize <= m_pathTableSizeSent)
			return true;

		u32 left = requiredPathTableSize - m_pathTableSizeSent;
		while (left)
		{
			StackBinaryWriter<SendMaxSize> writer;
			NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_StorePathTable, writer);
			u32 toSend = Min(requiredPathTableSize - m_pathTableSizeSent, u32(m_client.GetMessageMaxSize() - 32));
			left -= toSend;
			writer.WriteBytes(m_sendPathTable.GetMemory() + m_pathTableSizeSent, toSend);
			m_pathTableSizeSent += toSend;

			StackBinaryReader<16> reader;
			if (!msg.Send(reader))
				return false;
		}
		return true;
	}

	bool CacheClient::SendCasTable(u32 requiredCasTableSize)
	{
		SCOPED_WRITE_LOCK(m_casKeyTableNetworkLock, lock);
		if (requiredCasTableSize <= m_casKeyTableSizeSent)
			return true;

		u32 left = requiredCasTableSize - m_casKeyTableSizeSent;
		while (left)
		{
			StackBinaryWriter<SendMaxSize> writer;
			NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_StoreCasTable, writer);
			u32 toSend = Min(requiredCasTableSize - m_casKeyTableSizeSent, u32(m_client.GetMessageMaxSize() - 32));
			left -= toSend;
			writer.WriteBytes(m_sendCasKeyTable.GetMemory() + m_casKeyTableSizeSent, toSend);
			m_casKeyTableSizeSent += toSend;

			StackBinaryReader<16> reader;
			if (!msg.Send(reader))
				return false;
		}
		return true;
	}

	bool CacheClient::SendCacheEntry(const CasKey& cmdKey, const Map<u32, u32>& inputsStringToCasKey, const Map<u32, u32>& outputsStringToCasKey)
	{
		StackBinaryReader<1024> reader;
		{
			StackBinaryWriter<SendMaxSize> writer;

			NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_StoreEntry, writer);
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
			if (!GetLocalPathAndCasKey(path, casKey, m_sendCasKeyTable, m_sendPathTable, casKeyOffset))
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

					if (!NormalizeString<char>((const char*)file.GetData(), file.GetSize(), handleString, path.data))
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
		writer.WriteCasKey(cmdKey);
		if (!msg.Send(reader))
			return false;

		return true;
	}

	bool CacheClient::FetchCasTable()
	{
		SCOPED_WRITE_LOCK(m_casKeyTableNetworkLock, lock);

		StackBinaryReader<SendMaxSize> reader;
		{
			//SCOPED_WRITE_LOCK(m_pathTableNetworkLock, lock);
			u32 targetSize = ~0u;
			while (m_serverCasKeyTable.GetSize() < targetSize)
			{
				StackBinaryWriter<16> writer;
				NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_FetchCasTable, writer);
				writer.WriteU32(m_serverCasKeyTable.GetSize());

				reader.Reset();
				if (!msg.Send(reader))
					return false;
				u32 size = reader.ReadU32();
				if (targetSize == ~0u)
					targetSize = size;

				m_serverCasKeyTable.ReadMem(reader, false);
			}
		}
		{
			u32 targetSize = ~0u;
			while (m_serverPathTable.GetSize() < targetSize)
			{
				StackBinaryWriter<16> writer;
				NetworkMessage msg(m_client, CacheServiceId, CacheMessageType_FetchPathTable, writer);
				writer.WriteU32(m_serverPathTable.GetSize());

				reader.Reset();
				if (!msg.Send(reader))
					return false;
				u32 size = reader.ReadU32();
				if (targetSize == ~0u)
					targetSize = size;

				m_serverPathTable.ReadMem(reader, false);
			}
		}
		return true;
	}

	CasKey CacheClient::GetCmdKey(const ProcessStartInfo& info)
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
		if (!NormalizeString(info.arguments, TStrlen(info.arguments), hashString, TC("")))
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
					CasKey rspCasKey = NormalizeAndHashFile(rsp.data);
					hasher.Update(&rspCasKey, sizeof(CasKey));
				}
			}
		}

		return ToCasKey(hasher, false);
	}

	u32 CacheClient::FindRootIndex(const StringBufferBase& path)
	{
		if (path.count < m_shortestRoot)
			return ~0u;

		StringBuffer<MaxPath> shortPath;
		shortPath.Append(path.data, m_shortestRoot).MakeLower();
		StringKey key = ToStringKeyNoCheck(shortPath.data, m_shortestRoot);
		for (u32 i=0, e=u32(m_roots.size()); i!=e; ++i)
		{
			auto& root = m_roots[i];
			if (key != root.shortestPathKey)
				continue;
			if (!path.StartsWith(root.path.c_str()))
				continue;
			return i;
		}
		return ~0u;
	}

	template<typename CharType, typename Func>
	bool CacheClient::NormalizeString(const CharType* str, u64 strLen, const Func& func, const tchar* hint)
	{
		auto strEnd = str + strLen;
		auto searchPos = str;

		u32 destPos = 0;

		while (true)
		{
			auto absPathChars = searchPos;
			CharType lastChar = 0;
			while (absPathChars < strEnd && !(lastChar == ':' && *absPathChars == '\\'))
			{
				lastChar = *absPathChars;
				++absPathChars;
			}
		
			if (absPathChars == strEnd)
			{
				func(searchPos, strEnd - searchPos, ~0u);
				return true;
			}

			auto pathStart = absPathChars - 2;

			auto pathEndOrMore = pathStart;
			while (pathEndOrMore < strEnd && *pathEndOrMore != '\n')
				++pathEndOrMore;

			u32 lenOrMore = u32(pathEndOrMore - pathStart);
			u32 toCopy = Min(lenOrMore, m_longestRoot);
			StringBuffer<512> path;
			path.Append(pathStart, toCopy);

			u32 rootIndex = FindRootIndex(path);
			if (rootIndex == ~0u)
			{
				m_logger.Info(TC("PATH WITHOUT ROOT: %s (inside file %s)"), path.data, hint);
				return false;
			}

			if (u32 len = u32(pathStart - searchPos))
			{
				destPos += len;
				func(searchPos, len, ~0u);
			}
			CharType temp = RootStartByte + CharType(rootIndex);
			func(&temp, 1, destPos);
			destPos += 1;

			searchPos = pathStart + u32(m_roots[rootIndex].path.size()) - 1;
		}
	}

	CasKey CacheClient::NormalizeAndHashFile(const tchar* filename)
	{
		FileAccessor file(m_logger, filename);
		if (!file.OpenMemoryRead())
			return CasKeyZero;

		CasKeyHasher hasher;
		auto hashString = [&](const char* str, u64 strLen, u32 rootPos) { hasher.Update(str, strLen); };
		if (!NormalizeString<char>((const char*)file.GetData(), file.GetSize(), hashString, filename))
			return CasKeyZero;

		return ToCasKey(hasher, false);
	}

	bool CacheClient::GetLocalPathAndCasKey(StringBufferBase& outPath, CasKey& outKey, CompactCasKeyTable& casKeyTable, CompactPathTable& pathTable, u32 offset)
	{
		if (!m_connected)
			return false;

		SCOPED_READ_LOCK(m_casKeyTableNetworkLock, lock); // TODO: Is this needed?

		StringBuffer<MaxPath> normalizedPath;
		casKeyTable.GetPathAndKey(normalizedPath, outKey, pathTable, offset);
		UBA_ASSERT(normalizedPath.count);

		StringBuffer<8> rootIndexStr;
		rootIndexStr.Append(normalizedPath.data, normalizedPath.First(PathSeparator) - normalizedPath.data);
		u32 rootIndex = rootIndexStr[0] - RootStartByte;
		auto& root = m_roots[rootIndex];

		StringBuffer<MaxPath> path;
		outPath.Append(root.path).Append(normalizedPath.data + rootIndexStr.count + 1);
		return true;
	}
}
