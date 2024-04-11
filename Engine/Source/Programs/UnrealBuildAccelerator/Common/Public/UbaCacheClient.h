// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaCompactTables.h"
#include "UbaLogger.h"

namespace uba
{
	class NetworkClient;
	class ProcessHandle;
	class Session;
	class StorageImpl;
	struct ProcessStartInfo;

	class CacheClient
	{
	public:
		CacheClient(LogWriter& writer, StorageImpl& storage, NetworkClient& client, Session& session);
		~CacheClient();

		bool RegisterRoot(const tchar* rootPath, bool includeInKey = true);
		bool RegisterSystemRoots();

		bool WriteToCache(const ProcessHandle& process);
		bool FetchFromCache(const ProcessStartInfo& info);

		bool WriteCacheSummary(const tchar* destinationFile, const tchar* filterString = nullptr);

		inline NetworkClient& GetClient() { return m_client; }
		inline StorageImpl& GetStorage() { return m_storage; }

	private:
		bool SendPathTable(u32 requiredPathTableSize);
		bool SendCasTable(u32 requiredCasTableSize);
		bool SendCacheEntry(const CasKey& cmdKey, const Map<u32, u32>& inputsStringToCasKey, const Map<u32, u32>& outputsStringToCasKey);
		bool FetchCasTable();

		CasKey GetCmdKey(const ProcessStartInfo& info);
		u32 FindRootIndex(const StringBufferBase& path);

		template<typename CharType, typename Func>
		bool NormalizeString(const CharType* str, u64 strLen, const Func& func, const tchar* hint);

		CasKey NormalizeAndHashFile(const tchar* rspFile);
		bool GetLocalPathAndCasKey(StringBufferBase& outPath, CasKey& outKey, CompactCasKeyTable& casKeyTable, CompactPathTable& pathTable, u32 offset);

		MutableLogger m_logger;
		StorageImpl& m_storage;
		NetworkClient& m_client;
		Session& m_session;

		Atomic<bool> m_connected;

		struct Root;
		Vector<Root> m_roots;
		u32 m_shortestRoot = 0;
		u32 m_longestRoot = 0;

		CompactPathTable m_serverPathTable;
		CompactCasKeyTable m_serverCasKeyTable;

		CompactPathTable m_sendPathTable;
		CompactCasKeyTable m_sendCasKeyTable;

		ReaderWriterLock m_pathTableNetworkLock;
		u32 m_pathTableSizeSent = 0;

		ReaderWriterLock m_casKeyTableNetworkLock;
		u32 m_casKeyTableSizeSent = 0;

		ReaderWriterLock m_sendOneAtTheTimeLock;
	};
}