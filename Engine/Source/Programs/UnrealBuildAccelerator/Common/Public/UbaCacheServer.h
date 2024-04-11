// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaCompactTables.h"
#include "UbaHash.h"
#include "UbaLogger.h"

namespace uba
{
	class NetworkServer;
	class StorageServer;
	struct BinaryReader;
	struct BinaryWriter;
	struct ConnectionInfo;

	class CacheServer
	{
	public:
		CacheServer(LogWriter& writer, const tchar* rootDir, NetworkServer& server, StorageServer& storage);
		~CacheServer();

		bool Load();
		bool Save();

		bool RunMaintenance(bool force);

	private:
		bool SaveNoLock();
		void OnDisconnected(u32 clientId);

		struct Connection;
		bool HandleMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer);
		bool HandleStoreEntry(Connection& connection, BinaryReader& reader, BinaryWriter& writer);
		bool HandleFetchPathTable(BinaryReader& reader, BinaryWriter& writer);
		bool HandleFetchCasTable(BinaryReader& reader, BinaryWriter& writer);
		bool HandleFetchEntries(BinaryReader& reader, BinaryWriter& writer);
		bool HandleCreateStatusFile(BinaryReader& reader, BinaryWriter& writer);

		MutableLogger m_logger;
		NetworkServer& m_server;
		StorageServer& m_storage;

		StringBuffer<MaxPath> m_rootDir;

		ReaderWriterLock m_maintenanceLock;
		Atomic<u32> m_addsSinceMaintenance;

		struct CacheEntry
		{
			u64 creationTime;
			Vector<u8> inputCasKeyOffsets;
			Vector<u8> outputCasKeyOffsets;
		};

		struct CacheEntries
		{
			ReaderWriterLock lock;
			List<CacheEntry> entries;
		};

		ReaderWriterLock m_cacheEntryLookupLock;
		UnorderedMap<CasKey, CacheEntries> m_cacheEntryLookup;

		CompactPathTable m_pathTable;
		CompactCasKeyTable m_casKeyTable;

		ReaderWriterLock m_connectionsLock;
		Map<u32, Connection> m_connections;
	};
}