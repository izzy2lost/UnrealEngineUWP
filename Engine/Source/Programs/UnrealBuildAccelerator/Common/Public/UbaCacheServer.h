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
	struct CacheEntry;
	struct CacheEntries;
	struct ConnectionInfo;

	class CacheServer
	{
	public:
		CacheServer(LogWriter& writer, const tchar* rootDir, NetworkServer& server, StorageServer& storage);
		~CacheServer();

		bool Load();
		bool Save();

		bool RunMaintenance(bool force, const Function<bool()>& shouldExit);

		bool ShouldShutdown();

	private:
		bool SaveNoLock();
		void OnDisconnected(u32 clientId);

		struct Connection;
		struct ConnectionBucket;
		struct Bucket;

		ConnectionBucket& GetConnectionBucket(const ConnectionInfo& connectionInfo, BinaryReader& reader);
		Bucket& GetBucket(BinaryReader& reader);

		bool HandleMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer);
		bool HandleStoreEntry(ConnectionBucket& bucket, BinaryReader& reader, BinaryWriter& writer);
		bool HandleFetchPathTable(BinaryReader& reader, BinaryWriter& writer);
		bool HandleFetchCasTable(BinaryReader& reader, BinaryWriter& writer);
		bool HandleFetchEntries(BinaryReader& reader, BinaryWriter& writer, u32 clientVersion);
		bool HandleReportUsedEntry(BinaryReader& reader, BinaryWriter& writer);
		bool HandleExecuteCommand(BinaryReader& reader, BinaryWriter& writer);

		MutableLogger m_logger;
		NetworkServer& m_server;
		StorageServer& m_storage;

		StringBuffer<MaxPath> m_rootDir;

		Atomic<u32> m_addsSinceMaintenance;
		Atomic<bool> m_isRunningMaintenance;

		ReaderWriterLock m_bucketsLock;
		Map<u64, Bucket> m_buckets;

		ReaderWriterLock m_connectionsLock;
		Map<u32, Connection> m_connections;

		Atomic<bool> m_shutdownRequested = false;

		u64 m_creationTime = 0;
		u64 m_startTime = 0;
		u64 m_longestMaintenance = 0;

		bool m_checkInputsForDeletedCas = true;

		bool m_shouldWipe = false;
		bool m_forceAllSteps = false;

		CacheServer(const CacheServer&) = delete;
		CacheServer& operator=(const CacheServer&) = delete;
	};
}