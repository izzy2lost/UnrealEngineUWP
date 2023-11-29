// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaNetwork.h"
#include "UbaSession.h"
#include "UbaSessionServerCreateInfo.h"

namespace uba
{
	class NetworkServer;
	struct ConnectionInfo;

	class SessionServer final : public Session
	{
	public:
		SessionServer(const SessionServerCreateInfo& info);
		~SessionServer();
		
		ProcessHandle RunProcessRemote(const ProcessStartInfo& startInfo, float weight = 1.0f);
		void DisableRemoteExecution();

		void SetCustomCasKeyFromTrackedInputs(const tchar* fileName, const tchar* workingDir, const u8* trackedInputs, u32 trackedInputsBytes);
		bool GetCasKeyFromTrackedInputs(CasKey& out, const tchar* fileName, const tchar* workingDir, const u8* data, u32 dataLen);

		void SetRemoteProcessSlotAvailableEvent(const Function<void()>& remoteProcessSlotAvailableEvent);
		void SetRemoteProcessReturnedEvent(const Function<void(Process&)>& remoteProcessReturnedEvent);

		void WaitOnAllTasks();

		void SetMaxRemoteProcessCount(u32 count);

		u32 BeginExternalProcess(const tchar* description);
		void EndExternalProcess(u32 id, u32 exitCode);

		NetworkServer& GetServer();

	protected:
		struct ClientSession;

		void OnDisconnected(u32 clientId);
		bool HandleMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer);

		bool StoreCasFile(CasKey& out, const StringKey& fileNameKey, const tchar* fileName);
		bool WriteDirectoryTable(ClientSession& session, BinaryReader& reader, BinaryWriter& writer);
		bool WriteNameToHashTable(BinaryReader& reader, BinaryWriter& writer, u32 requestedSize);

		void ThreadMemoryCheckLoop();

		class RemoteProcess;

		RemoteProcess* DequeueProcess(u32 sessionId, u32 clientId);
		void OnCancelled(RemoteProcess* process);
		ProcessHandle ProcessRemoved(u32 processId);

		virtual bool PrepareProcess(const ProcessStartInfo& startInfo, bool isChild, StringBufferBase& outRealApplication, const tchar*& outRealWorkingDir) override final;
		virtual bool CreateFile(CreateFileResponse& out, const CreateFileMessage& msg, const tchar* virtualApplicationDir) override final;
		virtual void FileEntryAdded(StringKey fileNameKey, u64 lastWritten, u64 size) override final;
		virtual void PrintSessionStats(Logger& logger) override final;
		virtual void TraceSessionUpdate() override final;

		void WriteRemoteEnvironmentVariables(BinaryWriter& writer);
		bool InitializeNameToHashTable();

		NetworkServer& m_server;
		u32 m_uiLanguage;
		Atomic<u32> m_maxRemoteProcessCount;
		bool m_resetCas;
		bool m_remoteExecutionEnabled;
		bool m_nameToHashTableEnabled;

		Vector<tchar> m_remoteEnvironmentVariables;

		static constexpr u8 ServiceId = SessionServiceId;

		Function<void()> m_remoteProcessSlotAvailableEvent;
		Function<void(Process&)> m_remoteProcessReturnedEvent;

		CriticalSection m_remoteProcessAndSessionLock; // Can be re-entrant.
		List<ProcessHandle> m_queuedRemoteProcesses;
		UnorderedSet<ProcessHandle> m_activeRemoteProcesses;
		u32 m_finishedRemoteProcessCount = 0;
		u32 m_returnedRemoteProcessCount = 0;
		u32 m_availableRemoteSlotCount = 0;
		u32 m_connectionCount = 0;

		ReaderWriterLock m_binKeysLock;
		CasKey m_detoursBinaryKey;
		CasKey m_agentBinaryKey;

		struct ClientSession
		{
			TString name;
			u32 id = ~0u;
			u32 processSlotCount = 1;
			u32 usedSlotCount = 0;
			u64 lastPing = 0;
			u64 memAvail = 0;
			u64 memTotal = 0;
			float cpuLoad = 0;
			bool enabled = true;
			bool dedicated = false;

			ReaderWriterLock dirTablePosLock;
			u32 dirTablePos = 0;

		};
		Vector<ClientSession*> m_clientSessions;

		struct CustomCasKey
		{
			CasKey casKey;
			TString workingDir;
			Vector<u8> trackedInputs;
		};
		ReaderWriterLock m_customCasKeysLock;
		UnorderedMap<StringKey, CustomCasKey> m_customCasKeys;

		UnorderedMap<StringKey, CasKey> m_nameToHashLookup;
		ReaderWriterLock m_nameToHashLookupLock;
		Atomic<bool> m_nameToHashInitialized;

		ReaderWriterLock m_receivedFilesLock;
		UnorderedMap<StringKey, CasKey> m_receivedFiles;

		ReaderWriterLock m_fillUpOneAtTheTimeLock;

		ReaderWriterLock m_applicationDataLock;
		struct ApplicationData { ReaderWriterLock lock; Vector<u8> bytes; };
		UnorderedMap<StringKey, ApplicationData> m_applicationData;

		Event m_memoryThreadEvent;
		Thread m_memoryThread;
		Atomic<u64> m_memAvail;
		u64 m_memTotal = 0;
		u64 m_memRequiredToSpawn = 0;
		u8 m_memKillLoadPercent = 0;
		struct WaitingProcess { Event event; WaitingProcess* next = nullptr; };
		WaitingProcess* m_oldestWaitingProcess = nullptr;
		WaitingProcess* m_newestWaitingProcess = nullptr;
		ReaderWriterLock m_waitingProcessesLock;
		bool m_allowWaitOnMem = false;
		bool m_allowKillOnMem = false;

		SessionServer(const SessionServer&) = delete;
		void operator=(const SessionServer&) = delete;
	};
}
