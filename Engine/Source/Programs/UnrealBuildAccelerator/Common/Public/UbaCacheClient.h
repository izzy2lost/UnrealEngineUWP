// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaCompactTables.h"
#include "UbaLogger.h"

namespace uba
{
	class NetworkClient;
	class ProcessHandle;
	class RootPaths;
	class Session;
	class StorageImpl;
	struct ProcessStartInfo;

	class CacheClient
	{
	public:
		CacheClient(LogWriter& writer, StorageImpl& storage, NetworkClient& client, Session& session);
		~CacheClient();

		bool WriteToCache(const RootPaths& rootPaths, const ProcessHandle& process);
		bool FetchFromCache(const RootPaths& rootPaths, const ProcessStartInfo& info);

		bool WriteCacheSummary(const tchar* destinationFile, const tchar* filterString = nullptr);

		inline MutableLogger& GetLogger() { return m_logger; }
		inline NetworkClient& GetClient() { return m_client; }
		inline StorageImpl& GetStorage() { return m_storage; }

	private:
		bool SendPathTable(u32 requiredPathTableSize);
		bool SendCasTable(u32 requiredCasTableSize);
		bool SendCacheEntry(const RootPaths& rootPaths, const CasKey& cmdKey, const Map<u32, u32>& inputsStringToCasKey, const Map<u32, u32>& outputsStringToCasKey);
		bool FetchCasTable();

		CasKey GetCmdKey(const RootPaths& rootPaths, const ProcessStartInfo& info);

		bool GetLocalPathAndCasKey(const RootPaths& rootPaths, StringBufferBase& outPath, CasKey& outKey, CompactCasKeyTable& casKeyTable, CompactPathTable& pathTable, u32 offset);

		MutableLogger m_logger;
		StorageImpl& m_storage;
		NetworkClient& m_client;
		Session& m_session;

		Atomic<bool> m_connected;

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