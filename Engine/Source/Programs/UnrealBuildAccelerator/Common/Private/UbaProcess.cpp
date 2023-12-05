// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaProcess.h"
#include "UbaFileAccessor.h"
#include "UbaProtocol.h"
#include "UbaProcessStats.h"
#include "UbaApplicationRules.h"

#if PLATFORM_WINDOWS
#include "UbaDetoursPayload.h"
#include <winternl.h>
//#include <Psapi.h>
#include <detours/detours.h>
#else
#include <wchar.h>
#include <stdio.h>
#include <spawn.h>
#include <wordexp.h>

// These headers are used for tracking child and beyond
// processes and making sure they clean up properly
// Linux uses PR_SET_CHILD_SUBREAPER
// Mac has to roll it's own solution
#if PLATFORM_LINUX
#include <sys/prctl.h>
#elif PLATFORM_MAC
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

extern char **environ;
#endif

//////////////////////////////////////////////////////////////////////////////

#define UBA_EXIT_CODE(x) (9000 + x)

namespace uba
{
	#if PLATFORM_WINDOWS
	ReaderWriterLock g_envLock;
	void* g_env;
	#endif

	TString GetDirectoryName(const TString& file)
	{
		size_t nameIndex = file.find_last_of('\\');
		size_t lastSlash = file.find_last_of('/', nameIndex + 1);
		if (lastSlash != -1)
			nameIndex = lastSlash;
		if (nameIndex == -1)
			return TC("");
		return file.substr(0, nameIndex);
	}

	void Process::AddRef()
	{
		++m_refCount;
	}

	void Process::Release()
	{
		if (!--m_refCount)
			delete this;
	}

	ProcessImpl::ProcessImpl(Session& session, u32 id, ProcessImpl* parent)
	:	m_session(session)
	,	m_parentProcess(parent)
	,	m_id(id)
	,	m_comMemory(m_session.m_processCommunicationAllocator.Alloc(TC("")))
		#if !PLATFORM_WINDOWS
	,	m_cancelEvent(*new (m_comMemory.memory) Event)
	,	m_writeEvent(*new (m_comMemory.memory + sizeof(Event)) Event)
	,	m_readEvent(*new (m_comMemory.memory + sizeof(Event)*2) Event)
		#endif
	,	m_writtenFilesLock(parent ? parent->m_writtenFilesLock : *new ReaderWriterLock())
	,	m_writtenFiles(parent ? parent->m_writtenFiles : *new UnorderedMap<TString, WrittenFile>())
	,	m_tempFilesLock(parent ? parent->m_tempFilesLock : *new ReaderWriterLock())
	,	m_tempFiles(parent ? parent->m_tempFiles : *new UnorderedMap<StringKey, WrittenFile>)
	{

		CreateGuid(m_processGuid);
		m_cancelEvent.Create(true, true);
		m_writeEvent.Create(false, true);
		m_readEvent.Create(false, true);
	}

	ProcessImpl::~ProcessImpl()
	{
		if (m_comMemory.memory)
			m_cancelEvent.Set();

		m_messageThread.Wait();

		if (m_comMemory.memory)
		{
			#if !PLATFORM_WINDOWS
			m_cancelEvent.~Event();
			m_writeEvent.~Event();
			m_readEvent.~Event();
			#endif
			m_session.m_processCommunicationAllocator.Free(m_comMemory);
		}

		if (!m_parentProcess)
		{
			for (auto& pair : m_writtenFiles)
				if (pair.second.mappingHandle.IsValid())
					CloseFileMapping(pair.second.mappingHandle);
			delete &m_writtenFiles;
			delete &m_writtenFilesLock;

			ClearTempFiles();
			delete& m_tempFiles;
			delete& m_tempFilesLock;
		}
	}

	void ProcessImpl::Start(const ProcessStartInfo& startInfo, TString&& realApplication, const tchar* realWorkingDir, bool runningRemote, void* environment, bool async)
	{
		m_startTime = GetTime();

		m_startInfo = startInfo;

		m_description = startInfo.description;
		m_startInfo.description = m_description.c_str();

		m_virtualApplication = startInfo.application;
		Replace(m_virtualApplication.data(), '/', PathSeparator);
		m_startInfo.application = m_virtualApplication.c_str();

		m_arguments = startInfo.arguments;
		m_startInfo.arguments = m_arguments.c_str();

		m_virtualWorkingDir = startInfo.workingDir;
		Replace(m_virtualWorkingDir.data(), '/', PathSeparator);
		m_startInfo.workingDir = m_virtualWorkingDir.c_str();

		m_logFile = startInfo.logFile;
		Replace(m_logFile.data(), '/', PathSeparator);
		m_startInfo.logFile = m_logFile.c_str();

		m_virtualApplicationDir = GetDirectoryName(startInfo.application);

		m_realApplication = std::move(realApplication);
		m_realWorkingDir = realWorkingDir;
		if (realWorkingDir == startInfo.workingDir)
			m_realWorkingDir = m_startInfo.workingDir;

		if (m_parentProcess)
			m_waitForParent.Create(true);

		SetRulesIndex(startInfo);

		m_session.ProcessAdded(*this, 0);

		if (async)
			m_messageThread.Start([this, runningRemote, environment]() { ThreadRun(runningRemote, environment); return 0; });
		else
			ThreadRun(runningRemote, environment);
	}

	bool ProcessImpl::IsActive()
	{
		if (m_nativeProcessHandle == InvalidProcHandle)
		{
			//m_session.m_logger.Info(TC("IsActive false 1"), LastErrorToText().data);
			return false;
		}

		#if PLATFORM_WINDOWS
		DWORD waitRes = WaitForSingleObject((HANDLE)m_nativeProcessHandle, 0);
		if (waitRes == WAIT_TIMEOUT)
			return true;
		if (waitRes != WAIT_OBJECT_0)
		{
			m_session.m_logger.Error(TC("WaitForSingleObject failed on handle %llu id %u returning %u (%s)"), u64(m_nativeProcessHandle), m_nativeProcessId, waitRes, LastErrorToText().data);
			return false;
		}

		DWORD exitCode = STILL_ACTIVE;
		if (!GetExitCodeProcess((HANDLE)m_nativeProcessHandle, &exitCode))
		{
			m_nativeProcessExitCode = ~0u;
			m_session.m_logger.Error(TC("GetExitCodeProcess failed (%s)"), LastErrorToText().data);
			return false;
		}
		if (exitCode == STILL_ACTIVE)
			return true;
		if (!m_gotExitMessage && exitCode != 0xC0000005 && exitCode != 0xC0000409)
		{
			StringBuffer<> err;

			if (m_messageCount == 0) // This is bad.. bad binaries?
			{
				bool is64Bit = true;
				u64 fileSize = 0;
				CasKey key;
				
				FileAccessor fa(m_session.m_logger, m_realApplication.c_str());
				if (fa.OpenMemoryRead())
				{
					fileSize = fa.GetSize();
					u8* data = fa.GetData();

					CasKeyHasher hasher;
					hasher.Update(data, fileSize);
					key = ToCasKey(hasher, false);

					if (data[0] != 'M' || data[1] != 'Z')
						is64Bit = false;
					else
						is64Bit = *(u32*)(data + 0x3c) == 0x50450000;
				}

				if (!is64Bit)
					err.Appendf(TC("ERROR: Process did not start properly. Doesn't seem to be a 64-bit executable (%s Size: %llu, CasKey: %s)"), m_realApplication.c_str(), fileSize, CasKeyString(key).str);
				else
					err.Appendf(TC("ERROR: Process did not start properly. GetExitCodeProcess returned %u (%s Size: %llu, CasKey: %s)"), exitCode, m_realApplication.c_str(), fileSize, CasKeyString(key).str);
			}

			if (err.IsEmpty())
				err.Appendf(TC("ERROR: Process %llu (%s) not active but did not get exit message. Received %u messages (GetExitCodeProcess returned %u)"), u64(m_nativeProcessHandle), m_realApplication.c_str(), m_messageCount, exitCode);
			LogLine(false, err.data, LogEntryType_Error);
			m_nativeProcessExitCode = UBA_EXIT_CODE(666);
		}
		return false;

		#else

		if (m_parentProcess && m_parentProcess->m_nativeProcessId != 0) // Can't do wait on grandchildren on Linux.. but since we use PR_SET_CHILD_SUBREAPER we should once parent is gone and child is orphaned
			return true;

		while (true)
		{
			siginfo_t signalInfo;
			signalInfo.si_pid = 0;	// if remains 0, treat as child was not waitable (i.e. was running)
			int res = waitid(P_PID, (unsigned int)m_nativeProcessId, &signalInfo, WEXITED | WNOHANG | WNOWAIT);
			if (res)
			{
				UBA_ASSERT(res == -1);
				if (errno == EINTR)
					continue;
				if (errno == ECHILD) // This should not happen, but let's return true on this since we can't use waitid on processes that are not our children
					return true;
				UBA_ASSERTF(false, "waitid failed with error: %u (%s)", errno, strerror(errno));
				break;
			}
			else
			{
				if (signalInfo.si_pid != (pid_t)m_nativeProcessId)
					return true;
				break;
			}
		}

		if (!m_gotExitMessage)
		{
			StringBuffer<> err;
			err.Appendf(TC("ERROR: Process %u not active but did not get exit message. Received %u messages"), m_nativeProcessId, m_messageCount);
			LogLine(false, err.data, LogEntryType_Error);
			m_nativeProcessExitCode = UBA_EXIT_CODE(666);
		}

		//m_session.m_logger.Info(TC("IsActive false (no parent)"), LastErrorToText().data);
		return false;
		#endif
	}

	bool ProcessImpl::IsCancelled()
	{
		#if PLATFORM_WINDOWS
		return m_cancelEvent.IsSet(0);
		#else
		return m_cancelled; // can't use cancel event since memory might have been returned
		#endif
	}

	bool ProcessImpl::WaitForExit(u32 millisecondsTimeout)
	{
		return m_messageThread.Wait(millisecondsTimeout);
	}

	u64 ProcessImpl::GetTotalWallTime() const
	{
		return m_processStats.wallTime;
	}

	u64 ProcessImpl::GetTotalProcessorTime() const
	{
		return m_processStats.cpuTime;
	}

	void ProcessImpl::Cancel(bool terminate)
	{
		#if PLATFORM_WINDOWS
		m_cancelEvent.Set();
		#else
		m_cancelled = true;
		if (m_comMemory.memory)
			m_cancelEvent.Set();
		#endif
	}

	bool ProcessImpl::WaitForRead()
	{
		while (true)
		{
			if (m_readEvent.IsSet(1000))
				break;
			if (!IsActive())
				return false;
			if (IsCancelled())
				return false;
		}
		return true;
	}

	void ProcessImpl::SetWritten()
	{
		m_writeEvent.Set();
	}

	void ProcessImpl::ThreadRun(bool runningRemote, void* environment)
	{
		{
		SystemStatsScope systemStatsScope(m_systemStats);
		StorageStatsScope storageStatsScope(m_storageStats);
		SessionStatsScope sessionStatsScope(m_sessionStats);

		u8* comMemory = m_comMemory.memory;
		#if !PLATFORM_WINDOWS
		comMemory += sizeof(Event) * 3;
		#endif

		u8* writeMemory = comMemory;
		u8* readMemory = comMemory + CommunicationMemSize / 2;

		u32 retryCount = 0; // Do not allow retry

		u32 exitCode = ~0u;

		while (!IsCancelled())
		{
			exitCode = InternalCreateProcess(runningRemote, environment, m_comMemory.handle, m_comMemory.offset);

			bool loop = exitCode == 0;

			while (loop && WaitForRead())
			{
				u64 startTime = GetTime();
				BinaryReader reader(readMemory);
				BinaryWriter writer(writeMemory);
				loop = HandleMessage(reader, writer, readMemory);
				SetWritten();
				m_processStats.hostTotalTime += GetTime() - startTime;
				++m_messageCount;
			}

			u64 exitStartTime = GetTime();

			bool cancelled = IsCancelled();
			if (exitCode == 0)
				exitCode = InternalExitProcess(cancelled);

			m_processStats.exitTime = GetTime() - exitStartTime;

			if (exitCode == 0 && !m_messageSuccess)
				exitCode = UBA_EXIT_CODE(1);

			bool isChild = m_parentProcess != nullptr;
			if (cancelled || isChild)
				break;

			if (retryCount == 0)
				break;
			--retryCount;

			if (exitCode == 0xC0000005)
				m_session.m_logger.Warning(TC("Process exited with access violation. Will do one retry."));
			else if (exitCode == 0xC0000409)
				m_session.m_logger.Warning(TC("Process exited with stack buffer overflow. Will do one retry."));
			else
				break;

			m_logLines.clear();
			m_trackedInputs.clear();

			UBA_ASSERT(!m_parentProcess);
			m_writtenFiles.clear();
			ClearTempFiles();
		}

		m_processStats.wallTime = GetTime() - m_startTime;

		#if PLATFORM_WINDOWS
		if (m_accountingJobObject)
		{
			JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accountingInformation = {};
			if (QueryInformationJobObject(m_accountingJobObject, JOBOBJECTINFOCLASS::JobObjectBasicAccountingInformation, &accountingInformation, sizeof(accountingInformation), NULL))
				m_processStats.cpuTime = accountingInformation.TotalUserTime.QuadPart + accountingInformation.TotalKernelTime.QuadPart;
			CloseHandle(m_accountingJobObject);
		}
		#endif

		if (IsCancelled())
			m_exitCode = ProcessCancelExitCode;
		else
			m_exitCode = exitCode;
		}

		SystemStats::GetGlobal().Add(m_systemStats);

		m_session.ProcessExited(*this, m_processStats.wallTime);

		#if PLATFORM_LINUX
		// This should not really ever happen.. but just in case.. since children use memory from parent
		for (auto& child : m_childProcesses)
		{
			while (!((ProcessImpl*)child.m_process)->m_hasExited)
			{
				Sleep(100);
			}
		}
		#elif PLATFORM_MAC
		int res = WaitForProcessGroup(getpgrp());
		UBA_ASSERT(res == 0);
		#endif

		UBA_ASSERT(!m_parentProcess || !m_parentProcess->m_hasExited);

		m_hasExited = true;

		#if !PLATFORM_WINDOWS
		m_cancelEvent.~Event();
		m_writeEvent.~Event();
		m_readEvent.~Event();
		#endif

		m_session.m_processCommunicationAllocator.Free(m_comMemory);
		m_comMemory = {};

		if (!m_parentProcess)
			ClearTempFiles();

		if (m_startInfo.exitedFunc)
		{
			ProcessHandle h;
			h.m_process = this;
			m_startInfo.exitedFunc(m_startInfo.exitedUserData, h);
			h.m_process = nullptr;
		}
	}

	bool ProcessImpl::HandleMessage(BinaryReader& reader, BinaryWriter& writer, void* readStream)
	{
		MessageType messageType = (MessageType)reader.ReadByte();
		switch (messageType)
		{
			case MessageType_Init:
				{
					InitMessage msg;
					InitResponse response;
					m_messageSuccess = m_session.GetInitResponse(response, msg) && m_messageSuccess;
					writer.WriteBool(m_echoOn);
					writer.WriteString(m_startInfo.application);
					writer.WriteString(m_startInfo.workingDir);
					writer.WriteU64(response.directoryTableHandle);
					writer.WriteU32(response.directoryTableSize);
					writer.WriteU32(response.directoryTableCount);
					writer.WriteU64(response.mappedFileTableHandle);
					writer.WriteU32(response.mappedFileTableSize);
					writer.WriteU32(response.mappedFileTableCount);
					return true;
				}

			case MessageType_CreateFileW:
				{
					CreateFileMessage msg { *this };
					reader.ReadString(msg.fileName);
					msg.fileNameKey = reader.ReadStringKey();
					msg.access = (FileAccess)reader.ReadByte();

					CreateFileResponse response;
					m_messageSuccess = m_session.CreateFile(response, msg, m_virtualApplicationDir.c_str()) && m_messageSuccess;
					writer.WriteString(response.fileName);
					writer.WriteU64(response.size);
					writer.WriteU32(response.closeId);
					writer.WriteU32(response.mappedFileTableSize);
					writer.WriteU32(response.directoryTableSize);
					return true;
				}

			case MessageType_GetFullFileName:
				{
					GetFullFileNameMessage msg { *this };
					reader.ReadString(msg.fileName);
					msg.fileNameKey = reader.ReadStringKey();
					GetFullFileNameResponse response;
					m_messageSuccess = m_session.GetFullFileName(response, msg, m_virtualApplicationDir.c_str()) && m_messageSuccess;
					writer.WriteString(response.fileName);
					writer.WriteString(response.virtualFileName);
					writer.WriteU32(response.mappedFileTableSize);
					return true;
				}

			case MessageType_CloseFile:
				{
					CloseFileMessage msg { *this };
					reader.ReadString(msg.fileName);
					msg.closeId = reader.ReadU32();
					msg.attributes = DefaultAttributes(); // reader.ReadU32(); TODO
					msg.deleteOnClose = reader.ReadBool();
					msg.success = reader.ReadBool();
					msg.mappingHandle = reader.ReadU64();
					msg.mappingWritten = reader.ReadU64();
					msg.newNameKey = reader.ReadStringKey();
					if (msg.newNameKey != StringKeyZero)
						reader.ReadString(msg.newName);
					CloseFileResponse response;
					m_messageSuccess = m_session.CloseFile(response, msg) && m_messageSuccess;
					writer.WriteU32(response.directoryTableSize);
					return true;
				}

			case MessageType_DeleteFileW:
				{
					DeleteFileMessage msg { *this };
					reader.ReadString(msg.fileName);
					msg.fileNameKey = reader.ReadStringKey();
					msg.closeId = reader.ReadU32();
					DeleteFileResponse response;
					m_messageSuccess = m_session.DeleteFile(response, msg) && m_messageSuccess;
					writer.WriteBool(response.result);
					writer.WriteU32(response.errorCode);
					writer.WriteU32(response.directoryTableSize);
					return true;
				}

			case MessageType_CopyFile:
				{
					CopyFileMessage msg{ *this };
					msg.fromKey = reader.ReadStringKey();
					reader.ReadString(msg.fromName);
					msg.toKey = reader.ReadStringKey();
					reader.ReadString(msg.toName);
					CopyFileResponse response;
					m_messageSuccess = m_session.CopyFile(response, msg) && m_messageSuccess;
					writer.WriteString(response.fromName);
					writer.WriteString(response.toName);
					writer.WriteU32(response.closeId);
					writer.WriteU32(response.errorCode);
					writer.WriteU32(response.directoryTableSize);
					return true;
				}

			case MessageType_MoveFileW:
				{
					MoveFileMessage msg { *this };
					msg.fromKey = reader.ReadStringKey();
					reader.ReadString(msg.fromName);
					msg.toKey = reader.ReadStringKey();
					reader.ReadString(msg.toName);
					msg.flags = reader.ReadU32();
					MoveFileResponse response;
					m_messageSuccess = m_session.MoveFile(response, msg) && m_messageSuccess;
					writer.WriteBool(response.result);
					writer.WriteU32(response.errorCode);
					writer.WriteU32(response.directoryTableSize);
					return true;
				}

			case MessageType_Chmod:
				{
					ChmodMessage msg { *this };
					msg.fileNameKey = reader.ReadStringKey();
					reader.ReadString(msg.fileName);
					msg.fileMode = reader.ReadU32();
					ChmodResponse response;
					m_messageSuccess = m_session.Chmod(response, msg) && m_messageSuccess;
					writer.WriteU32(response.errorCode);
					return true;
				}

			case MessageType_CreateDirectory:
				{
					CreateDirectoryMessage msg;
					msg.nameKey = reader.ReadStringKey();
					reader.ReadString(msg.name);
					CreateDirectoryResponse response;
					m_messageSuccess = m_session.CreateDirectory(response, msg) && m_messageSuccess;
					writer.WriteBool(response.result);
					writer.WriteU32(response.errorCode);
					return true;
				}

			case MessageType_ListDirectory:
				{
					ListDirectoryMessage msg;
					reader.ReadString(msg.directoryName);
					msg.directoryNameKey = reader.ReadStringKey();
					ListDirectoryResponse response;
					m_messageSuccess = m_session.GetListDirectoryInfo(response, msg.directoryName.data, msg.directoryNameKey) && m_messageSuccess;
					writer.WriteU32(response.tableSize);
					writer.WriteU32(response.tableOffset);
					return true;
				}

			case MessageType_CreateProcess:
				{
					#if PLATFORM_LINUX
					// This process will become the parent of a process if it becomes orphaned
					static bool subreaper = []() { prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0); return true; }();
					#endif

					StringBuffer<> application;
					reader.ReadString(application);
					StringBuffer<32*1024> fullCommandLine;
					reader.ReadString(fullCommandLine);
					StringBuffer<> currentDir;
					reader.ReadString(currentDir);
					if (currentDir.IsEmpty())
						currentDir.Append(m_startInfo.workingDir);

					const tchar* commandLine = nullptr;

					// Remove application from command line
					if (fullCommandLine[0] == '"')
					{
						const tchar* second = fullCommandLine.First('"', 1);
						if (!second)
							second = fullCommandLine.data + fullCommandLine.count;
						//UBA_ASSERTF(second, TC("Missing second '\"' in command line: %s"), fullCommandLine.data); // "Unsupported cmd line format"
						commandLine = second + 1;
						if (application.IsEmpty())
							application.Append(fullCommandLine.data + 1, u64(second - fullCommandLine.data - 1));
					}
					else
					{
						const tchar* secondParamStart = fullCommandLine.First(' ', 1);
						if (!secondParamStart)
							commandLine = TC("");
						else
							commandLine = secondParamStart + 1;
						if (application.IsEmpty())
							application.Append(fullCommandLine.data, u64(secondParamStart - fullCommandLine.data));
					}

					while (*commandLine == ' ')
						++commandLine;

					StringBuffer<> temp;
					ProcessStartInfo info;
					info.application = application.data;
					info.arguments = commandLine;
					info.workingDir = currentDir.data;
					info.logFile = InternalGetChildLogFile(temp);
					info.priorityClass = m_startInfo.priorityClass;
					info.outputStatsThresholdMs = m_startInfo.outputStatsThresholdMs;
					info.logLineUserData = this;
					info.logLineFunc = [](void* userData, const tchar* line, u32 length, LogEntryType type) { ((ProcessImpl*)userData)->LogLine(false, TString(line, length), type); };

					ProcessHandle h = m_session.InternalRunProcess(info, true, this);
					m_childProcesses.push_back(h);
					u32 childProcessId = u32(m_childProcesses.size());

					auto& process = *(ProcessImpl*)h.m_process;
					process.m_echoOn = m_echoOn;

					const char* detoursLib = m_session.m_detoursLibrary.c_str();
					u32 detoursLibLen = u32(m_session.m_detoursLibrary.size());

					#if !PLATFORM_WINDOWS
					if (!*detoursLib)
					{
						detoursLib = "/mnt/e/dev/fn/Engine/Binaries/Linux/UnrealBuildAccelerator/" UBA_DETOURS_LIBRARY;
						detoursLibLen = u32(strlen(detoursLib));
					}
					#endif


					writer.WriteU32(childProcessId);
					writer.WriteU32(process.m_rulesIndex);
					writer.WriteU32(detoursLibLen);
					writer.WriteBytes(detoursLib, detoursLibLen);

					TString realCommandLine = TC("\"") + process.m_realApplication + TC("\" ") + commandLine;
					writer.WriteString(realCommandLine);
					writer.WriteString(m_realWorkingDir);

					#if !PLATFORM_WINDOWS
					writer.WriteU64(process.m_comMemory.handle.uid);
					writer.WriteU32(process.m_comMemory.offset);
					writer.WriteString(info.logFile);
					#endif

					return true;
				}

			case MessageType_StartProcess:
				{
					u32 processId = reader.ReadU32();
					UBA_ASSERT(processId > 0);
					auto& process = *(ProcessImpl*)m_childProcesses[processId - 1].m_process;
					bool result = reader.ReadBool();
					u32 lastError = reader.ReadU32();
					if (!result)
					{
						m_session.m_logger.Logf(LogEntryType_Info, TC("DetourCreateProcessWithDllEx for child process failed - %s. %s (Working dir: %s)"), LastErrorToText(lastError).data, process.m_realApplication.c_str(), process.m_realWorkingDir);
						process.m_waitForParent.Set();
						return true;
					}

#if PLATFORM_WINDOWS
					HANDLE nativeProcessHandle = (HANDLE)reader.ReadU64();
					u32 nativeProcessId = reader.ReadU32();
					HANDLE nativeThreadHandle = (HANDLE)reader.ReadU64();

					if (nativeProcessHandle)
					{
						DuplicateHandle((HANDLE)m_nativeProcessHandle, nativeProcessHandle, GetCurrentProcess(), (HANDLE*)&process.m_nativeProcessHandle, 0, false, DUPLICATE_SAME_ACCESS);
						UBA_ASSERT(process.m_nativeProcessHandle && process.m_nativeProcessHandle != InvalidProcHandle);
						DuplicateHandle((HANDLE)m_nativeProcessHandle, nativeThreadHandle, GetCurrentProcess(), &process.m_nativeThreadHandle, 0, false, DUPLICATE_SAME_ACCESS);
						UBA_ASSERT(process.m_nativeThreadHandle && process.m_nativeThreadHandle != INVALID_HANDLE_VALUE);
						process.m_nativeProcessId = nativeProcessId;
					}
#else
					u64 nativeProcessHandle = reader.ReadU64();
					u32 nativeProcessId = reader.ReadU32();
					u64 nativeThreadHandle = reader.ReadU64();
					process.m_nativeProcessHandle = (ProcHandle)nativeProcessHandle;
					process.m_nativeProcessId = nativeProcessId;
					//m_session.m_logger.Info("Got StartProcess with pid %u", nativeProcessId);
#endif
					process.m_waitForParent.Set();
					return true;
				}

			case MessageType_ExitChildProcess:
				{
					u32 nativeProcessId = reader.ReadU32();
					for (auto& child : m_childProcesses)
					{
						auto& process = *(ProcessImpl*)child.m_process;
						if (process.m_nativeProcessId != nativeProcessId)
							continue;
						process.m_parentReportedExit = true;
						return true;
					}
					UBA_ASSERT(false);
					return true;
				}

			case MessageType_UpdateTables:
				{
					u32 dirSize = m_session.GetDirectoryTableSize();
					writer.WriteU32(dirSize);
					return true;
				}

			case MessageType_CreateTempFile:
				{
					CreateTempFile(reader, m_nativeProcessHandle, m_virtualApplication.c_str());
					return true;
				}

			case MessageType_OpenTempFile:
				{
					OpenTempFile(reader, writer, m_virtualApplication.c_str());
					return true;
				}

			case MessageType_VirtualAllocFailed:
				{
					StringBuffer<> allocType;
					reader.ReadString(allocType);
					u32 error = reader.ReadU32();
					m_session.AllocFailed(*this, allocType.data, error);
					return true;
				}

			case MessageType_Log:
				{
					bool printInSession = reader.ReadBool();
					TString line = reader.ReadString();
					LogLine(printInSession, std::move(line), LogEntryType_Info);
					return true;
				}
				
			case MessageType_EchoOn:
				{
					m_echoOn = reader.ReadBool();
					return true;
				}

			case MessageType_InputDependencies:
				{
					UBA_ASSERT(m_startInfo.trackInputs);
					if (m_trackedInputs.empty())
					{
						u32 trackedInputsSize = reader.ReadU32();
						m_trackedInputs.reserve(trackedInputsSize);
					}
					u32 toRead = reader.ReadU32();
					u8* pos = m_trackedInputs.data() + m_trackedInputs.size();
					m_trackedInputs.resize(m_trackedInputs.size() + toRead);
					reader.ReadBytes(pos, toRead);
					return true;
				}

			case MessageType_Exit:
				{
					m_gotExitMessage = true;
					m_nativeProcessExitCode = reader.ReadU32();

					StringBuffer<> logName;
					reader.ReadString(logName);

					ProcessStats stats;
					stats.Read(reader, ~0u);

					m_processStats.Add(stats);

					if (m_parentProcess)
						m_parentProcess->m_processStats.Add(m_processStats);

					if (m_startInfo.outputStatsThresholdMs && TimeToMs(m_processStats.GetTotalTime()) > m_startInfo.outputStatsThresholdMs)
					{
						m_session.PrintProcessStats(m_processStats, logName.data);
						m_processStats.Print(m_session.m_logger);
					}

					{
						ScopedWriteLock lock(m_writtenFilesLock);
						for (auto& kv : m_writtenFiles)
						{
							if (kv.second.owner != this)
								continue;
							kv.second.owner = nullptr;
							if (kv.second.mappingHandle.IsValid())
								if (!m_session.WriteFileToDisk(*this, kv.second))
									m_messageSuccess = false;
						}
					}
					//PROCESS_MEMORY_COUNTERS mem;
					//mem.cb = sizeof(mem);
					//GetProcessMemoryInfo(m_nativeProcessHandle, &mem, sizeof(mem));
					//mem.PeakPagefileUsage;
					//m_session.m_logger.Debug(TC("PROCESS MEM: WorkingSet: %s Page: %s"), BytesToText(mem.PeakWorkingSetSize), BytesToText(mem.PeakPagefileUsage));
					
					return false;
				}
			case MessageType_Custom:
				{
					m_session.CustomMessage(reader, writer);
					return true;
				}
			case MessageType_FlushWrittenFiles:
				{
					return m_session.FlushWrittenFiles(*this);
				}

			case MessageType_UpdateEnvironment:
				{
					StringBuffer<> reason;
					reader.ReadString(reason);
					return m_session.UpdateEnvironment(*this, reason.data);
				}
		}
		return m_session.m_logger.Error(TC("Unknown message type %u"), messageType);
	}

	void ProcessImpl::LogLine(bool printInSession, TString&& line, LogEntryType logType)
	{
		if (IsCancelled())// || m_startInfo.stdoutHandle != INVALID_HANDLE_VALUE)
			return;
		if (printInSession)
			m_session.m_logger.Log(LogEntryType_Warning, line.c_str(), u32(line.size()));
		if (m_startInfo.logLineFunc)
			m_startInfo.logLineFunc(m_startInfo.logLineUserData, line.c_str(), u32(line.size()), logType);
		ScopedWriteLock l(m_logLinesLock);
		m_logLines.push_back({ std::move(line), logType });
	}

	bool ProcessImpl::CreateTempFile(BinaryReader& reader, ProcHandle nativeProcessHandle, const tchar* application)
	{
		StringKey key = reader.ReadStringKey();
		StringBuffer<> fileName;
		reader.ReadString(fileName);
		u64 mappingHandle = reader.ReadU64();
		u64 mappingHandleSize = reader.ReadU64();

		FileMappingHandle source;
		source.FromU64(mappingHandle);
		FileMappingHandle newHandle;
		if (!DuplicateFileMapping(nativeProcessHandle, source, GetCurrentProcessHandle(), &newHandle, FILE_MAP_READ, false, 0))
		{
			m_session.m_logger.Error(TC("Failed to duplicate handle for temp file (%s)"), fileName.data);
			return true;
		}

		ScopedWriteLock tempLock(m_tempFilesLock);
		auto insres = m_tempFiles.try_emplace(key, WrittenFile{ nullptr, StringKeyZero, fileName.data, newHandle, mappingHandleSize, mappingHandle });
		if (insres.second)
			return true;

		WrittenFile& tempFile = insres.first->second;
		FileMappingHandle oldMapping = tempFile.mappingHandle;
		tempFile.mappingHandle = newHandle;
		tempLock.Leave();
		CloseFileMapping(oldMapping);
		return true;
	}

	bool ProcessImpl::OpenTempFile(BinaryReader& reader, BinaryWriter& writer, const tchar* application)
	{
		StringKey fileKey = reader.ReadStringKey();
		StringBuffer<> fileName;
		reader.ReadString(fileName);
					
		u64 mappingHandle = 0;
		u64 mappingWritten = 0;

		ScopedReadLock lock(m_tempFilesLock);
		auto findIt = m_tempFiles.find(fileKey);
		if (findIt != m_tempFiles.end())
		{
			mappingHandle = findIt->second.mappingHandle.ToU64();
			mappingWritten = findIt->second.mappingWritten;
		}

		writer.WriteU64(mappingHandle);
		writer.WriteU64(mappingWritten);
		return true;
	}

	void ProcessImpl::SetRulesIndex(const ProcessStartInfo& si)
	{
		u32 exeNameStart = 0;
		u32 exeNameEnd = u32(m_virtualApplication.size());
		size_t lastSeparator = m_virtualApplication.find_last_of(PathSeparator);
		if (lastSeparator != -1)
			exeNameStart = u32(lastSeparator + 1);
		else if (m_virtualApplication[exeNameStart] == '"')
			++exeNameStart;
		if (m_virtualApplication[exeNameEnd - 1] == '"')
			--exeNameEnd;
		StringBuffer<128> exeName;
		exeName.Append(m_virtualApplication.c_str() + exeNameStart, exeNameEnd - exeNameStart);
		
		while (true)
		{
			for (u32 i = 1;; ++i)
			{
				const tchar* app = g_applicationRules[i].app;
				if (!app)
					break;
				if (!exeName.Equals(app))
					continue;
				m_rulesIndex = i;
				return;
			}

			if (!exeName.Equals(TC("dotnet.exe")))
				return;
			
			u32 firstArgumentStart = 0;
			u32 firstArgumentEnd = 0;
			bool quoted = false;
			for (u32 i = 0, e = u32(m_arguments.size()); i != e; ++i)
			{
				tchar c = m_arguments[i];
				if (firstArgumentEnd)
				{
					if (c == '\\')
						firstArgumentStart = i + 1;
					if ((quoted && c != '"') || (!quoted && c != ' ' && c != '\t'))
						continue;
					firstArgumentEnd = i;
					break;
				}
				else
				{
					if (c == ' ' || c == '\t')
					{
						++firstArgumentStart;
						continue;
					}
					if (c == '"')
					{
						++firstArgumentStart;
						quoted = true;
					}
					firstArgumentEnd = firstArgumentStart + 1;
				}
			}
			exeName.Clear().Append(m_arguments.data() + firstArgumentStart, firstArgumentEnd - firstArgumentStart);
		}
	}

	const tchar* ProcessImpl::InternalGetChildLogFile(StringBufferBase& temp)
	{
		if (!*m_startInfo.logFile)
			return TC("");
		temp.Append(m_startInfo.logFile);
		if (TStrcmp(temp.data + temp.count - 4, TC(".log")) == 0)
			temp.Resize(temp.count - 4);
		temp.Appendf(TC("_CHILD%u.log"), u32(m_childProcesses.size()));
		return temp.data;
	}

#if PLATFORM_MAC
	int ProcessImpl::WaitForProcessGroup(pid_t pgid)
	{
		int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_PGRP, pgid};

		for (;;) {
			// Query the list of processes in the group by using sysctl(3).
			// This is "hard" because we don't know how big that list is, so we
			// have to first query the size of the output data and then account for
			// the fact that the size might change by the time we actually issue
			// the query.
			struct kinfo_proc *procs = NULL;
			size_t nprocs = 0;
			do {
				size_t len;
				if (sysctl(name, 4, 0, &len, NULL, 0) == -1) {
					printf("Something went wrong\n");
					return -1;
				}
				procs = (struct kinfo_proc *)malloc(len);
				if (sysctl(name, 4, procs, &len, NULL, 0) == -1) {
					UBA_ASSERT(errno == ENOMEM);
					free(procs);
					procs = NULL;
				} else {
					nprocs = len / sizeof(struct kinfo_proc);
				}
			} while (procs == NULL);
			UBA_ASSERT(nprocs >= 1);  // Must have found the group leader at least.

			if (nprocs == 1) {
				// Found only one process, which must be the leader because we have
				// purposely expect it as a zombie.
				UBA_ASSERT(procs->kp_proc.p_pid == pgid);
				free(procs);
				return 0;
			}

			// More than one process left in the process group.  Pause a little bit
			// before retrying to avoid burning CPU.
			struct timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 1000000;
			if (nanosleep(&ts, NULL) == -1) {
				UBA_ASSERT(FALSE);
				return -1;
			}
		}
	}
#endif

	u32 ProcessImpl::InternalCreateProcess(bool runningRemote, void* environment, FileMappingHandle communicationHandle, u64 communicationOffset)
	{
		ScopedWriteLock initLock(m_initLock);
		Logger& logger = m_session.m_logger;

#if PLATFORM_WINDOWS

		if (!m_parentProcess)
		{
			const char* detoursLib = m_session.m_detoursLibrary.c_str();
			if (!*detoursLib)
				detoursLib = UBA_DETOURS_LIBRARY_ANSI;

			TString commandLine = TC("\"") + m_realApplication + TC("\" ") + m_startInfo.arguments;
			LPCSTR dlls[] = { detoursLib };

			STARTUPINFOEX siex;
			STARTUPINFO& si = siex.StartupInfo;
			ZeroMemory(&siex, sizeof(STARTUPINFOEX));
			si.cb = sizeof(STARTUPINFOEX);

			PROCESS_INFORMATION processInfo;
			ZeroMemory(&processInfo, sizeof(processInfo));

			DWORD creationFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP | m_startInfo.priorityClass;
			BOOL inheritHandles = false;


			SIZE_T attributesBufferSize = 0;
			::InitializeProcThreadAttributeList(nullptr, 1, 0, &attributesBufferSize);

			u8 attributesBuffer[128];
			if (sizeof(attributesBuffer) < attributesBufferSize)
			{
				logger.Error(TC("Attributes buffer is too small, needs to be at least %llu"), u64(attributesBufferSize));
				return UBA_EXIT_CODE(2);
			}

			PPROC_THREAD_ATTRIBUTE_LIST attributes = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributesBuffer);
			if (!::InitializeProcThreadAttributeList(attributes, 1, 0, &attributesBufferSize))
			{
				logger.Error(TC("InitializeProcThreadAttributeList failed (%s)"), LastErrorToText().data);
				return UBA_EXIT_CODE(3);
			}

			auto destroyAttr = MakeGuard([&]() { ::DeleteProcThreadAttributeList(attributes); });


			siex.lpAttributeList = attributes;
			creationFlags |= EXTENDED_STARTUPINFO_PRESENT;


			ScopedReadLock jobObjectLock(m_session.m_processJobObjectLock);
			if (!m_session.m_processJobObject)
			{
				m_cancelEvent.Set();
				return ProcessCancelExitCode;
			}

			bool isDetachedProcess = g_applicationRules[m_rulesIndex].rules->AllowDetach();

			HANDLE hJob = CreateJobObject(nullptr, nullptr);
			JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = { };
			info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_BREAKAWAY_OK;
			SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &info, sizeof(info));
			m_accountingJobObject = hJob;

			HANDLE jobs[] = { m_session.m_processJobObject, m_accountingJobObject };

			if (!::UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, jobs, sizeof(jobs), nullptr, nullptr))
			{
				logger.Error(TC("UpdateProcThreadAttribute failed when setting job list (%s)"), LastErrorToText().data);
				return UBA_EXIT_CODE(4);
			}
			
			if (isDetachedProcess)
				creationFlags |= DETACHED_PROCESS;
			else
				creationFlags |= CREATE_NO_WINDOW;

			u32 retryCount = 0;
			while (!IsCancelled())
			{
				LPCWSTR workingDir = *m_realWorkingDir ? m_realWorkingDir : NULL;
				if (DetourCreateProcessWithDlls(NULL, (tchar*)commandLine.c_str(), NULL, NULL, inheritHandles, creationFlags, environment, workingDir, &si, &processInfo, sizeof_array(dlls), dlls, NULL))
					break;

				DWORD error = GetLastError();

				if (error == ERROR_ACCESS_DENIED || error == ERROR_INTERNAL_ERROR)
				{
					// We have no idea why this is happening.. but it seems to recover when retrying.
					// Could it be related to two process spawning at the exact same time or something?
					// It happens extremely rarely and can happen on both host and remotes
					bool retry = retryCount++ < 5;
					const tchar* errorText = error == ERROR_ACCESS_DENIED ? TC("access denied") : TC("internal error");
					logger.Logf(retry ? LogEntryType_Info : LogEntryType_Error, TC("DetourCreateProcessWithDllEx failed with %s, retrying %s (Working dir: %s)"), errorText, commandLine.c_str(), workingDir);
					if (!retry)
						return UBA_EXIT_CODE(5);
					Sleep(100 + (rand() % 200)); // We have no idea
					ZeroMemory(&processInfo, sizeof(processInfo));
					continue;
				}
				else if (error == ERROR_WRITE_PROTECT) // AWS shutting down
				{
					m_cancelEvent.Set();
					return ProcessCancelExitCode;
				}

				LastErrorToText lett(error);
				const tchar* errorText = lett.data;
				if (error == ERROR_INVALID_HANDLE)
					errorText = TC("Can't detour a 32-bit target process from a 64-bit parent process.");

				if (!IsCancelled())
				{
					if (error == ERROR_DIRECTORY)
						logger.Error(TC("HOW CAN THIS HAPPEN? '%s'"), workingDir);

					logger.Error(TC("DetourCreateProcessWithDllEx failed: %s (Working dir: %s). Exit code: %u - %s"), commandLine.c_str(), workingDir, error, errorText);
				}
				return UBA_EXIT_CODE(6);
			}

			//auto closeThreadHandle = MakeGuard([&]() { CloseHandle(pi.hThread); });
			//auto closeProcessHandle = MakeGuard([&]() { CloseHandle(pi.hProcess); });

			destroyAttr.Execute();

			m_nativeProcessHandle = (ProcHandle)(u64)processInfo.hProcess;
			m_nativeProcessId = processInfo.dwProcessId;
			m_nativeThreadHandle = processInfo.hThread;
		}
		else
		{
			u64 startTime = GetTime();
			while (!m_waitForParent.IsSet(500))
			{
				if (IsCancelled())
					break;
				if (TimeToMs(GetTime() - startTime) > 120 * 1000) // 
				{
					startTime = GetTime();
					logger.Error(TC("Waiting for parent process in createprocess has now taken more than 120 seconds."));
				}
			}

			if (m_nativeProcessHandle == InvalidProcHandle) // Failed to create the child process
				return UBA_EXIT_CODE(7);
		}

		HANDLE hostProcess;
		HANDLE currentProcess = GetCurrentProcess();
		if (!DuplicateHandle(currentProcess, currentProcess, (HANDLE)m_nativeProcessHandle, &hostProcess, 0, FALSE, DUPLICATE_SAME_ACCESS))
		{
			if (!IsCancelled())
				logger.Error(TC("Failed to duplicate host process handle for process"));//% ls."), commandLine.c_str());
			return UBA_EXIT_CODE(8);
		}

		DetoursPayload payload;
		payload.processGuid = m_processGuid;
		payload.hostProcess = hostProcess;
		payload.cancelEvent = m_cancelEvent.GetHandle();
		payload.writeEvent = m_writeEvent.GetHandle();
		payload.readEvent = m_readEvent.GetHandle();
		payload.communicationHandle = communicationHandle.handle;
		payload.communicationOffset = communicationOffset;
		payload.rulesIndex = m_rulesIndex;
		payload.runningRemote = runningRemote;
		payload.isChild = m_parentProcess != nullptr;
		payload.trackInputs = m_startInfo.trackInputs;
		payload.useCustomAllocator = m_startInfo.useCustomAllocator && g_applicationRules[m_rulesIndex].rules->AllowMiMalloc();
		payload.isRunningWine = IsRunningWine();
		payload.uiLanguage = m_startInfo.uiLanguage;
		if (*m_startInfo.logFile)
		{
			#if !UBA_DEBUG_LOG_ENABLED
			static bool runOnce = [&]() { logger.Warning(TC("Build has log files disabled so no logs will be produced")); return false; }();
			#endif
			payload.logFile.Append(m_startInfo.logFile);
		}

		if (!DetourCopyPayloadToProcessEx((HANDLE)m_nativeProcessHandle, DetoursPayloadGuid, &payload, sizeof(payload)))
		{
			logger.Error(TC("Failed to copy payload to process"));//% ls."), commandLine.c_str());
			return UBA_EXIT_CODE(9);
		}

		if (!AlternateGroupAffinity(m_nativeThreadHandle))
		{
			logger.Error(TC("Failed to set thread group affinity to process"));//% ls. (% ls)"), commandLine.c_str(), LastErrorToText().data);
			return UBA_EXIT_CODE(10);
		}

		m_processStats.startupTime = GetTime() - m_startTime;

		if (ResumeThread(m_nativeThreadHandle) == -1)
		{
			logger.Error(TC("Failed to resume thread for"));//% ls. (% ls)", commandLine.c_str(), LastErrorToText().data);
			return UBA_EXIT_CODE(11);
		}

		//closeThreadHandle.Execute();
		//closeProcessHandle.Cancel();
#else // #if PLATFORM_WINDOWS

		if (!m_parentProcess)
		{

			StringBuffer<> ldPreload;
			#if PLATFORM_LINUX
				ldPreload.Append("LD_PRELOAD=");
			#else
				ldPreload.Append("DYLD_INSERT_LIBRARIES=");
			#endif
			
			//if (m_session.m_detoursLibrary[0] != '/')
			//	ldPreload.Append("./");
			const char* detoursLib = m_session.m_detoursLibrary.c_str();
			if (*detoursLib)
				ldPreload.Append(detoursLib);
			else
				ldPreload.Append("./" UBA_DETOURS_LIBRARY);

			StringBuffer<128> comIdVar;
			comIdVar.Append("UBA_COMID=").AppendValue(communicationHandle.uid).Append('+').AppendValue(communicationOffset);

			StringBuffer<512> workingDir;
			workingDir.Append("UBA_CWD=").Append(m_realWorkingDir);

			StringBuffer<32> rulesStr;
			rulesStr.Append("UBA_RULES=").AppendValue(m_rulesIndex);

			StringBuffer<512> logFile;
			if (*m_startInfo.logFile)
			{
				#if !UBA_DEBUG_LOG_ENABLED
				static bool runOnce = [&]() { logger.Warning(TC("Build has log files disabled so no logs will be produced")); return false; }();
				#endif
				logFile.Append("UBA_LOGFILE=").Append(m_startInfo.logFile);
			}


			Vector<const char*> envvars;

			const char* it = (const char*)environment;
			while (*it)
			{
				const char* s = it;
				envvars.push_back(s);
				it += TStrlen(s) + 1;
			}

			envvars.push_back(ldPreload.data);
			envvars.push_back(comIdVar.data);
			envvars.push_back(workingDir.data);
			envvars.push_back(rulesStr.data);
			if (runningRemote)
				envvars.push_back("UBA_REMOTE=1");
			if (!logFile.IsEmpty())
				envvars.push_back(logFile.data);

			envvars.push_back(nullptr);

			wordexp_t  w;

			auto expRes = wordexp(m_realApplication.c_str(), &w, 0);
			if (expRes != 0)
			{
				logger.Error("wordexp failed (%i) parsing application name: %s", expRes, m_realApplication.c_str());
				return UBA_EXIT_CODE(16);
			}
			const char* args = m_startInfo.arguments;
			expRes = wordexp(args, &w, WRDE_APPEND);
			if (expRes != 0)
			{
				logger.Error("wordexp failed (%i) parsing arguments: %s", expRes, args);
				return UBA_EXIT_CODE(16);
			}

			//logger.Info(TC("ARGS: %hs\n", args.c_str());
			//const char* argv[] = { app.c_str(), args.c_str(), nullptr };
			//const char* argv[] = { m_realApplication.c_str(), "-o", "code", "Code.cpp", nullptr };

			short flags = POSIX_SPAWN_SETPGROUP;

			posix_spawnattr_t attr;
			int res = posix_spawnattr_init(&attr);
			UBA_ASSERTF(res == 0, TC("posix_spawnattr_init"));
			res = posix_spawnattr_setflags(&attr, flags);
			UBA_ASSERTF(res == 0, TC("posix_spawnattr_setflags"));
			res = posix_spawnattr_setpgroup(&attr, getpgrp());
			UBA_ASSERTF(res == 0, TC("posix_spawnattr_setpgroup"));

			posix_spawn_file_actions_t fileActions;
			posix_spawn_file_actions_init(&fileActions);

			if (!*m_realWorkingDir)
			{
				#if PLATFORM_MAC
				posix_spawn_file_actions_addchdir_np(&fileActions, m_realWorkingDir);
				#else
				//UBA_ASSERT(false); // TODO: Revisit
				#endif
			}

			pid_t processID;
			res = posix_spawnp(&processID, m_realApplication.c_str(), &fileActions, &attr, w.we_wordv, (char**)envvars.data());

			posix_spawn_file_actions_destroy(&fileActions);
			posix_spawnattr_destroy(&attr);

			if (res != 0)
			{
				logger.Error(TC("posix_spawn failed: %s %s (Working dir: %s) -> %i (%s)"), m_realApplication.c_str(), m_startInfo.arguments, m_realWorkingDir, res, strerror(errno));
				return UBA_EXIT_CODE(12);
			}
			m_nativeProcessHandle = (ProcHandle)1;
			m_nativeProcessId = u32(processID);
		}
		else
		{
			//logger.Info("Waiting for parent");
			u64 startTime = GetTime();
			while (!m_waitForParent.IsSet(500))
			{
				if (IsCancelled())
					break;
				if (TimeToMs(GetTime() - startTime) > 120 * 1000) // 
				{
					startTime = GetTime();
					logger.Error(TC("Waiting for parent process in createprocess has now taken more than 120 seconds."));
				}
			}

			//logger.Info("DONE waiting on parent");

			if (m_nativeProcessHandle == InvalidProcHandle) // Failed to create the child process
				return UBA_EXIT_CODE(7);
		}
#endif
		return 0;
	}

	u32 ProcessImpl::InternalExitProcess(bool cancel)
	{
		ScopedWriteLock lock(m_initLock);
		Logger& logger = m_session.m_logger;

		ProcHandle handle = m_nativeProcessHandle;
		if (handle == InvalidProcHandle)
			return ~0u;

		if (m_parentProcess)
		{
			u64 startTime = GetTime();
			while (!m_waitForParent.IsSet(500))
			{
				if (IsCancelled())
					break;
				if (TimeToMs(GetTime() - startTime) > 120 * 1000) // 
				{
					startTime = GetTime();
					logger.Error(TC("Waiting for parent process in exitprocess has now taken more than 120 seconds."));
				}
			}
		}
		m_nativeProcessHandle = InvalidProcHandle;

#if PLATFORM_WINDOWS

		auto closeHandleGuard = MakeGuard([&]() { CloseHandle((HANDLE)handle); });

		bool hadTimeout = false;
		if (cancel)
			TerminateProcess((HANDLE)handle, ProcessCancelExitCode);
		else
		{	
			while (true)
			{
				DWORD res = WaitForSingleObject((HANDLE)handle, 120 * 1000);
				if (res == WAIT_OBJECT_0)
				{
					break;
				}

				if (res == WAIT_TIMEOUT)
				{
					if (!hadTimeout && m_nativeProcessExitCode != STILL_ACTIVE)
					{
						hadTimeout = true;
						const tchar* gotMessage = m_gotExitMessage ? TC("Got") : TC("Did not get");
						const tchar* isCancelledNewCheck = IsCancelled() ? TC("true") : TC("false");
						logger.Info(TC("WaitForSingleObject timed out after 120 seconds waiting for process %s to exit (Exit code %u, %s ExitMessage and wrote %u files. Cancelled: %s. Runtime: %s). Will terminate and wait again"), m_startInfo.description, m_nativeProcessExitCode, gotMessage, u32(m_writtenFiles.size()), isCancelledNewCheck, TimeToText(GetTime() - m_startTime).str);
						TerminateProcess((HANDLE)handle, m_nativeProcessExitCode);
						continue;
					}
					logger.Error(TC("WaitForSingleObject failed while waiting for process %s to exit even after terminating it (%s)"), m_startInfo.description, LastErrorToText().data);
				}
				else if (res == WAIT_FAILED)
					logger.Error(TC("WaitForSingleObject failed while waiting for process to exit (%s)"), LastErrorToText().data);
				else if (res == WAIT_ABANDONED)
					logger.Error(TC("Abandoned, this should never happen"));
				TerminateProcess((HANDLE)handle, UBA_EXIT_CODE(13));
				return UBA_EXIT_CODE(13);
			}
		}

		bool res = true;
		if (!hadTimeout)
		{
			DWORD nativeExitCode = 0;
			res = GetExitCodeProcess((HANDLE)handle, (DWORD*)&nativeExitCode);
			if (!res && GetLastError() == ERROR_INVALID_HANDLE) // Was already terminated
				return ~0u;
			if (m_gotExitMessage)
				m_nativeProcessExitCode = nativeExitCode;
		}

		if (res || cancel)
			return m_nativeProcessExitCode;
		logger.Warning(TC("GetExitCodeProcess failed (%s)"), LastErrorToText().data);
		return UBA_EXIT_CODE(14);
#else

		auto g = MakeGuard([this]() { m_nativeProcessId = 0; });

		if (cancel)
		{
			kill((pid_t)m_nativeProcessId, -1);
			return m_nativeProcessExitCode;
		}


		if (m_parentProcess != nullptr) // We can't wait for grandchildren.. if we got here the parent reported the child as exited
			return 0;

		// Process should have been waited on here because of IsActive
		int status = 0;
		while (true)
		{
			int res = waitpid((pid_t)m_nativeProcessId, &status, 0);
			if (res == -1)
			{
				logger.Error(TC("waitpid failed on %u (%hs)"), m_nativeProcessId, strerror(errno));
				return UBA_EXIT_CODE(15);
			}
			if (WIFEXITED(status))
			{
				m_nativeProcessExitCode = WEXITSTATUS(status);
				break;
			}
			if (WIFSIGNALED(status))
			{
				//logger.Info(TC("SIGNALED"));
				m_nativeProcessExitCode = WTERMSIG(status);
				break;
			}
			Sleep(1);
		}

		return m_nativeProcessExitCode;
#endif
	}

	void ProcessImpl::ClearTempFiles()
	{
		for (auto& pair : m_tempFiles)
			if (pair.second.mappingHandle.IsValid())
				CloseFileMapping(pair.second.mappingHandle);
		m_tempFiles.clear();
	}
}
