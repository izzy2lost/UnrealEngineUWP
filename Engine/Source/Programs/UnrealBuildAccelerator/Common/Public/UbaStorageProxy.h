// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaFileMapping.h"
#include "UbaNetwork.h"

namespace uba
{
    class NetworkClient;
	class NetworkServer;
	struct BinaryReader;
	struct BinaryWriter;
	struct ConnectionInfo;

	class StorageProxy
	{
	public:
		StorageProxy(NetworkServer& server, NetworkClient& client, const Guid& storageServerUid, const tchar* name);

		bool Disconnect(u32 timeoutMs); // Use a timeout to try to make more graceful disconnect
		void PrintSummary();

	protected:
		bool HandleMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer);
		u16 PopId();
		void PushId(u16 id);

		static constexpr u8 ServiceId = StorageServiceId;

		NetworkServer& m_server;
		NetworkClient& m_client;

		LoggerWithWriter m_logger;

		Guid m_storageServerUid;

		TString m_name;

		struct FileEntry { ReaderWriterLock lock; MappedView view; bool hasSegments = false; bool storeCompressed = false; };
		ReaderWriterLock m_filesLock;
		UnorderedMap<CasKey, FileEntry> m_files;

		struct ActiveFetch
		{
			FileEntry* file = nullptr;
			Atomic<u64> fetchedSize;
		};

		Event m_hasActiveFetchesEvent;
		ReaderWriterLock m_activeFetchesLock;
		UnorderedMap<u16, ActiveFetch> m_activeFetches;

		FileMappingBuffer m_fileMapping;

		ReaderWriterLock m_availableIdsLock;
		Vector<u16> m_availableIds;
		u16 m_availableIdsHigh = 1;
	};
}
