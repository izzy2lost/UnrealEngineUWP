// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaLogger.h"
#include "UbaMemory.h"

namespace uba
{
	class CompactCasKeyTable;
	class CompactPathTable;
	class NetworkClient;
	class ProcessHandle;
	class RootPaths;
	class Session;
	class StorageImpl;
	struct CacheStats;
	struct CasKey;
	struct ProcessStartInfo;

	struct CacheClientCreateInfo
	{
		CacheClientCreateInfo(LogWriter& w, StorageImpl& st, NetworkClient& c, Session& se) : writer(w), storage(st), client(c), session(se) {}
		LogWriter& writer;
		StorageImpl& storage;
		NetworkClient& client;
		Session& session;
		bool reportMissReason = false;
	};

	class CacheClient
	{
	public:
		CacheClient(const CacheClientCreateInfo& info);
		~CacheClient();

		bool WriteToCache(const RootPaths& rootPaths, u32 bucketId, const ProcessStartInfo& info, const u8* inputs, u64 inputsSize, const u8* outputs, u64 outputsSize);
		bool FetchFromCache(const RootPaths& rootPaths, u32 bucketId, const ProcessStartInfo& info);
		bool RequestServerShutdown(const tchar* reason);

		bool WriteCacheSummary(const tchar* destinationFile, const tchar* filterString = nullptr);

		inline MutableLogger& GetLogger() { return m_logger; }
		inline NetworkClient& GetClient() { return m_client; }
		inline StorageImpl& GetStorage() { return m_storage; }

	private:
		struct Bucket;

		bool SendPathTable(Bucket& bucket, u32 requiredPathTableSize);
		bool SendCasTable(Bucket& bucket, u32 requiredCasTableSize);
		bool SendCacheEntry(Bucket& bucket, const RootPaths& rootPaths, const CasKey& cmdKey, const Map<u32, u32>& inputsStringToCasKey, const Map<u32, u32>& outputsStringToCasKey);
		bool FetchCasTable(Bucket& bucket, CacheStats& stats, u32 requiredCasTableOffset);

		CasKey GetCmdKey(const RootPaths& rootPaths, const ProcessStartInfo& info);
		bool ShouldNormalize(const StringBufferBase& path);

		bool GetLocalPathAndCasKey(Bucket& bucket, const RootPaths& rootPaths, StringBufferBase& outPath, CasKey& outKey, CompactCasKeyTable& casKeyTable, CompactPathTable& pathTable, u32 offset);

		MutableLogger m_logger;
		StorageImpl& m_storage;
		NetworkClient& m_client;
		Session& m_session;
		bool m_reportMissReason;

		Atomic<bool> m_connected;

		ReaderWriterLock m_bucketsLock;
		UnorderedMap<u32, Bucket> m_buckets;

		ReaderWriterLock m_sendOneAtTheTimeLock;

		CacheClient(const CacheClient&) = delete;
		CacheClient& operator=(const CacheClient&) = delete;
	};
}