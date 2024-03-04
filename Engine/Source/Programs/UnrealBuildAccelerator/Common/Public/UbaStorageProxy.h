// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaFileMapping.h"
#include "UbaLogger.h"
#include "UbaNetwork.h"

namespace uba
{
    class NetworkClient;
	class NetworkServer;
	class StorageImpl;
	struct BinaryReader;
	struct BinaryWriter;
	struct ConnectionInfo;
	struct MessageInfo;

	class StorageProxy
	{
	public:
		StorageProxy(NetworkServer& server, NetworkClient& client, const Guid& storageServerUid, const tchar* name, StorageImpl* localStorage = nullptr);
		~StorageProxy();

		bool Disconnect(u32 timeoutMs); // Use a timeout to try to make more graceful disconnect
		void PrintSummary();

	protected:
		u16 PopId();
		void PushId(u16 id);
		bool HandleMessage(const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer);
		bool SendEnd(const CasKey& key);

		static constexpr u8 ServiceId = StorageServiceId;

		NetworkServer& m_server;
		NetworkClient& m_client;
		StorageImpl* m_localStorage;

		MutableLogger m_logger;

		Guid m_storageServerUid;

		TString m_name;

		Atomic<u32> m_inProcessClientId;

		struct SegmentInFlight { u32 refCount; u32 segmentIndex; Event done; SegmentInFlight* prev; SegmentInFlight* next; };

		struct FileEntry
		{
			ReaderWriterLock lock;
			u8* memory = nullptr;
			u64 size = 0;
			Atomic<u64> received;
			CasKey casKey;
			u16 fetchId = 0;
			bool storeCompressed = false;
			bool sendEnd = false;
			bool error = false;
			Vector<u8> segmentsAvailable;
			SegmentInFlight* firstInFlight = nullptr;
			SegmentInFlight* lastInFlight = nullptr;
		};

		ReaderWriterLock m_filesLock;
		UnorderedMap<CasKey, FileEntry> m_files;

		struct ActiveFetch
		{
			FileEntry* file = nullptr;
			Guid clientUid;
			Atomic<u64> fetchedSize;
		};

		Event m_hasActiveFetchesEvent;
		ReaderWriterLock m_activeFetchesLock;
		UnorderedMap<u16, ActiveFetch> m_activeFetches;

		ReaderWriterLock m_availableIdsLock;
		Vector<u16> m_availableIds;
		u16 m_availableIdsHigh = 1;
	};
}
