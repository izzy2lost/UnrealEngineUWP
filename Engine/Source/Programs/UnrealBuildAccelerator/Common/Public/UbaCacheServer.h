// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaLogger.h"
#include "UbaStringBuffer.h"

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
		struct ConnectionBucket;
		struct CacheEntry;
		struct CacheEntries;
		struct Bucket;

		ConnectionBucket& GetConnectionBucket(const ConnectionInfo& connectionInfo, BinaryReader& reader);
		Bucket& GetBucket(BinaryReader& reader);

		bool HandleMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer);
		bool HandleStoreEntry(ConnectionBucket& bucket, BinaryReader& reader, BinaryWriter& writer);
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

		ReaderWriterLock m_bucketsLock;
		Map<u64, Bucket> m_buckets;

		ReaderWriterLock m_connectionsLock;
		Map<u32, Connection> m_connections;

		CacheServer(const CacheServer&) = delete;
		CacheServer& operator=(const CacheServer&) = delete;
	};
}