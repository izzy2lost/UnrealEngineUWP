// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaStats.h"
#include "UbaTrace.h"

namespace uba
{
	class NetworkClient;

	class TraceView
	{
	public:
		struct Process
		{
			u32 id = 0;
			u32 exitCode = ~0u;
			u64 start = 0;
			u64 stop = 0;
			TString description;
			HBITMAP bitmap = 0;
			u32 bitmapOffset = 0;
			bool bitmapDirty = true;
			bool returned = false;
			ProcessStats processStats;
			SessionStats sessionStats;
			StorageStats storageStats;
			SystemStats systemStats;
		};

		struct Processor
		{
			Vector<Process> processes;
		};

		struct SessionUpdate
		{
			u64 time;
			u64 send;
			u64 recv;
			u64 ping;
			u64 memAvail;
			float cpuLoad;
			u8 connectionCount;
		};

		struct WorkRecord
		{
			const tchar* description = nullptr;
			u64 start = 0;
			u64 stop = 0;
			HBITMAP bitmap = 0;
			u32 bitmapOffset = 0;
		};

		struct WorkTrack
		{
			Vector<WorkRecord> records;
		};


		struct FileTransfer
		{
			CasKey key;
			u64 size;
			TString hint;
			u64 start;
			u64 stop;
		};

		struct Session
		{
			TString name;
			Guid clientUid;
			Vector<Processor> processors;
			Vector<SessionUpdate> updates;
			Vector<TString> summary;
			Vector<FileTransfer> fetchedFiles;
			Vector<FileTransfer> storedFiles;
			u64 fetchedFilesBytes = 0;
			u64 storedFilesBytes = 0;
			u32 maxVisibleFiles = 0;

			float highestSendPerS = 0;
			float highestRecvPerS = 0;

			bool isReset = true;
			u64 disconnectTime = ~u64(0);
			u64 prevUpdateTime = 0;
			u64 prevSend = 0;
			u64 prevRecv = 0;
			u64 memTotal = 0;
			u64 processActiveCount = 0;
			u64 processExitedCount = 0;

			TString proxyName;
			bool proxyCreated = false;
		};

		struct ProcessLocation
		{
			u32 sessionIndex = 0;
			u32 processorIndex = 0;
			u32 processIndex = 0;
			bool operator==(const ProcessLocation& o) const { return sessionIndex == o.sessionIndex && processorIndex == o.processorIndex && processIndex == o.processIndex; }
		};

		Process* GetProcess(const ProcessLocation& loc) { return &(sessions[loc.sessionIndex].processors[loc.processorIndex].processes[loc.processIndex]); }
		void Clear() { sessions.clear(); workTracks.clear(); strings.clear(); startTime = 0; finished = true; };

		Vector<Session> sessions;
		Vector<WorkTrack> workTracks;
		Vector<TString> strings;
		u64 startTime = 0;
		u64 frequency = 0;
		u32 version = 0;
		bool finished = true;
	};


	class TraceReader
	{
	public:
		TraceReader(Logger& logger);
		~TraceReader();

		#if PLATFORM_WINDOWS

		// Use for file read
		bool ReadFile(TraceView& out, const tchar* fileName, bool replay);
		bool UpdateReadFile(TraceView& out, u64 maxTime, bool& outChanged);

		// Use for network
		bool StartReadClient(TraceView& out, NetworkClient& client);
		bool UpdateReadClient(TraceView& out, NetworkClient& client, bool& outChanged);

		// Use for local
		bool StartReadNamed(TraceView& out, const tchar* namedTrace, bool silentFail = false);
		bool UpdateReadNamed(TraceView& out, bool& outChanged);

		bool ReadMemory(TraceView& out, bool trackHost);
		bool ReadTrace(TraceView& out, BinaryReader& reader, u64 maxTime);
		void StopAllActive(TraceView& out, u64 stopTime);

		bool SaveAs(const tchar* fileName);

		Guid ReadClientId(TraceView& out, BinaryReader& reader);
		TraceView::Session& GetSession(TraceView& out, u32 sessionIndex);
		TraceView::Session* GetSession(TraceView& out, const Guid& clientUid);

		UnorderedMap<u32, TraceView::ProcessLocation> m_activeProcesses;

		struct WorkRecordLocation { u32 track; u32 index; };
		UnorderedMap<u32, WorkRecordLocation> m_activeWorkRecords;

		Vector<u32> m_sessionIndexToSession;

		#endif

		Logger& m_logger;
		TraceChannel m_channel;
		ReaderWriterLock m_memoryLock;
		FileMappingHandle m_memoryHandle;
		u8* m_memoryBegin = nullptr;
		u8* m_memoryPos = nullptr;
		u8* m_memoryEnd = nullptr;
		u64 m_startTime = ~u64(0);
		HANDLE m_hostProcess = NULL;
	};
}