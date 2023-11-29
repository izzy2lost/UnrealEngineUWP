// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaNetworkClient.h"
#include "UbaStorage.h"

namespace uba
{
	class NetworkClient;

	using StartProxyCallback = bool(void* userData, u16 port, const Guid& storageServerUid);

	struct StorageClientCreateInfo : StorageCreateInfo
	{
		StorageClientCreateInfo(NetworkClient& c, const tchar* rootDir_) : StorageCreateInfo(rootDir_, c.GetLogWriter()), client(c) {}
		NetworkClient& client;
		const tchar* zone = TC("");
		u16 proxyPort = DefaultStorageProxyPort;
		bool sendCompressed = true;
		
		StartProxyCallback* startProxyCallback = nullptr;
		void* startProxyUserData = nullptr;
	};


	class StorageClient final : public StorageImpl
	{
	public:
		StorageClient(const StorageClientCreateInfo& info);
		~StorageClient();

		bool IsUsingProxy();
		void StopProxy();

#if !UBA_USE_SPARSEFILE
		virtual bool GetCasFileName(StringBufferBase& out, const CasKey& casKey) override;
#endif
		virtual MappedView MapView(const CasKey& casKey, const tchar* hint) override;

		virtual bool GetZone(StringBufferBase& out) override;
		virtual bool RetrieveCasFile(RetrieveResult& out, const CasKey& casKey, const tchar* hint, FileMappingBuffer* mappingBuffer = nullptr, u64 memoryMapAlignment = 1, bool allowProxy = true) override;
		virtual bool StoreCasFile(CasKey& out, const tchar* fileName, const CasKey& casKeyOverride = CasKeyZero, bool deferCreation = false) override;
		virtual bool StoreCasFile(CasKey& out, StringKey fileNameKey, const tchar* fileName, FileMappingHandle mappingFile, u64 mappingOffset, u64 fileSize, const tchar* hint, bool deferCreation = false, bool keepMappingInMemory = false) override;
		virtual void Ping() override;

		static bool SendBatchMessages(NetworkClient& client, u16 fetchId, u8* slot, u64 capacity, u64 left, u32 messageMaxSize, u32& readIndex, u32& responseSize);
		static bool SendAllSegments(NetworkClient& client, u16 fetchId, u8* readBuffer, u64 left, u32 messageMaxSize);
		bool SendFile(const CasKey& casKey, const tchar* fileName, u8* sourceMem, u64 sourceSize, const tchar* hint);

		NetworkClient& m_client;
		bool m_sendCompressed;

		Guid m_storageServerUid;

		TString m_zone;

		ReaderWriterLock m_temporaryStorageFilesLock;
		struct MappedFile { FileMappingHandle handle; u64 size; TString fileName; };
		UnorderedMap<CasKey, MappedFile> m_temporaryStorageFiles;

		ReaderWriterLock m_sendOneAtTheTimeLock;
		ReaderWriterLock m_retrieveOneBatchAtTheTimeLock;

		static constexpr u8 ServiceId = StorageServiceId;

		struct ProxyClient;
		ReaderWriterLock m_proxyClientLock;
		ProxyClient* m_proxyClient = nullptr;
		u64 m_proxyClientKeepAliveTime = 0;

		StartProxyCallback* m_startProxyCallback = nullptr;
		void* m_startProxyUserData = nullptr;
	};

}
