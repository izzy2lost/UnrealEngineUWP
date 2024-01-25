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
#include <poll.h>
#include <stdio.h>
#include <spawn.h>

// These headers are used for tracking child and beyond
// processes and making sure they clean up properly
// Linux uses PR_SET_CHILD_SUBREAPER
// Mac has to roll it's own solution
#if PLATFORM_LINUX
#include <sys/prctl.h>
#include <sys/resource.h>
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

	void ProcessImpl::Start(const ProcessStartInfo& startInfo, TString&& realApplication, const tchar* realWorkingDir, bool runningRemote, void* environment, bool async, bool enableDetour)
	{
		m_detourEnabled = enableDetour;

		m_startTime = GetTime();

		m_startInfo = startInfo;

		m_description = startInfo.description;
		m_startInfo.description = m_description.c_str();

		m_virtualApplication = startInfo.application;
		FixPathSeparators(m_virtualApplication.data());
		m_startInfo.application = m_virtualApplication.c_str();

		m_arguments = startInfo.arguments;
		m_startInfo.arguments = m_arguments.c_str();

		m_virtualWorkingDir = startInfo.workingDir;
		FixPathSeparators(m_virtualWorkingDir.data());
		m_startInfo.workingDir = m_virtualWorkingDir.c_str();

		m_logFile = startInfo.logFile;
		FixPathSeparators(m_logFile.data());
		m_startInfo.logFile = m_logFile.c_str();

		size_t nameIndex = m_virtualApplication.find_last_of(PathSeparator);
		if (nameIndex != -1)
			m_virtualApplicationDir = m_virtualApplication.substr(0, nameIndex);

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

		#if PLATFORM_MAC
		if (m_parentProcess && m_gotExitMessage) // TODO: We need a timeout here... if child crashes we will never get exit message
			return false;
		#endif

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
		u8* readMemory = comMemory;// +CommunicationMemSize / 2;

		u32 retryCount = 0; // Do not allow retry

		u32 exitCode = ~0u;

		while (!IsCancelled())
		{
			exitCode = InternalCreateProcess(runningRemote, environment, m_comMemory.handle, m_comMemory.offset);

			bool loop = exitCode == 0 && m_detourEnabled;

			while (loop && WaitForRead())
			{
				u64 startTime = GetTime();
				BinaryReader reader(readMemory);
				BinaryWriter writer(writeMemory);
				loop = HandleMessage(reader, writer);
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

		// For some reason a parent can exit before a child. Need to figure out repro for this but I've seen it happen on ClangEditor win64
		for (auto& child : m_childProcesses)
			while (!((ProcessImpl*)child.m_process)->m_hasExited)
				Sleep(10);

		UBA_ASSERT(!m_parentProcess || !m_parentProcess->m_hasExited);

		m_session.ProcessExited(*this, m_processStats.wallTime);

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
			auto exitedFunc = m_startInfo.exitedFunc;
			auto userData = m_startInfo.userData;
			m_startInfo.exitedFunc = nullptr;
			m_startInfo.userData = nullptr;
			ProcessHandle h;
			h.m_process = this;
			exitedFunc(userData, h);
			h.m_process = nullptr;
		}
	}

	bool ProcessImpl::HandleMessage(BinaryReader& reader, BinaryWriter& writer)
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
					m_messageSuccess = m_session.CreateFile(response, msg) && m_messageSuccess;
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
					m_messageSuccess = m_session.GetFullFileName(response, msg) && m_messageSuccess;
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
					StringBuffer<64*1024> fullCommandLine;
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

					ProcessHandle h = m_session.InternalRunProcess(info, true, this, true);
					m_childProcesses.push_back(h);
					u32 childProcessId = u32(m_childProcesses.size());

					auto& process = *(ProcessImpl*)h.m_process;
					process.m_echoOn = m_echoOn;

					const char* detoursLib = m_session.m_detoursLibrary.c_str();
					u32 detoursLibLen = u32(m_session.m_detoursLibrary.size());

					writer.WriteU32(childProcessId);
					writer.WriteU32(process.m_rulesIndex);
					writer.WriteU32(detoursLibLen);
					writer.WriteBytes(detoursLib, detoursLibLen);

					writer.WriteString(m_realWorkingDir);
					#if PLATFORM_WINDOWS
					TString realCommandLine = TC("\"") + process.m_realApplication + TC("\" ") + commandLine;
					writer.WriteString(realCommandLine);
					#else
					writer.WriteString(process.m_realApplication);
					writer.WriteString(commandLine);
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
						m_session.m_logger.Logf(LogEntryType_Info, TC("Detoured process failed to start child process - %s. %s (Working dir: %s)"), LastErrorToText(lastError).data, process.m_realApplication.c_str(), process.m_realWorkingDir);
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
					bool isError = reader.ReadBool();
					TString line = reader.ReadString();
					LogLine(printInSession, std::move(line), isError ? LogEntryType_Error : LogEntryType_Info);
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

					if (!IsCancelled())
						if (m_startInfo.writeOutputFilesOnFail || GetApplicationRules()[m_rulesIndex].rules->IsExitCodeSuccess(m_nativeProcessExitCode))
							WriteFilesToDisk();

					if (m_parentProcess)
					{
						m_parentProcess->m_processStats.Add(m_processStats);
						m_parentProcess->m_sessionStats.Add(m_sessionStats);
						m_parentProcess->m_storageStats.Add(m_storageStats);
						m_parentProcess->m_systemStats.Add(m_systemStats);
					}

					if (m_startInfo.outputStatsThresholdMs && TimeToMs(m_processStats.GetTotalTime()) > m_startInfo.outputStatsThresholdMs)
					{
						m_session.PrintProcessStats(m_processStats, logName.data);
						m_processStats.Print(m_session.m_logger, GetFrequency());
					}

					return false;
				}

			case MessageType_GetNextProcess:
				{
					u32 prevExitCode = reader.ReadU32();
					NextProcessInfo nextProcess;

					StackBinaryWriter<16 * 1024> statsWriter;
					
					ProcessStats processStats;
					processStats.Read(reader, TraceVersion);
					processStats.startupTime = m_processStats.startupTime;
					processStats.wallTime = GetTime() - m_startTime;
					processStats.cpuTime = 0;
					processStats.hostTotalTime = m_processStats.hostTotalTime;
					
					processStats.Write(statsWriter);
					m_sessionStats.Write(statsWriter);
					m_storageStats.Write(statsWriter);
					m_systemStats.Write(statsWriter);
					BinaryReader statsReader(statsWriter.GetData(), 0, statsWriter.GetPosition());

					bool newProcess = false;
					m_messageSuccess = m_session.GetNextProcess(*this, newProcess, nextProcess, prevExitCode, statsReader) && m_messageSuccess;
					writer.WriteBool(newProcess);
					if (!newProcess)
						return true;

					m_startTime = GetTime();
					m_processStats = {};
					m_sessionStats = {};
					m_storageStats = {};
					m_systemStats = {};

					writer.WriteString(nextProcess.arguments);
					writer.WriteString(nextProcess.workingDir);
					writer.WriteString(nextProcess.description);
					writer.WriteString(nextProcess.logFile);
					return true;
				}

			case MessageType_Custom:
				{
					m_session.CustomMessage(*this, reader, writer);
					return true;
				}
			case MessageType_FlushWrittenFiles:
				{
					WriteFilesToDisk();
					bool result = m_session.FlushWrittenFiles(*this);
					writer.WriteBool(result);
					return true;
				}

			case MessageType_UpdateEnvironment:
				{
					StringBuffer<> reason;
					reader.ReadString(reason);
					bool resetStats = reader.ReadBool();
					bool result = m_session.UpdateEnvironment(*this, reason.data, resetStats);
					writer.WriteBool(result);
					return true;
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
		
		auto rules = GetApplicationRules();
		
		while (true)
		{
			for (u32 i = 1;; ++i)
			{
				const tchar* app = rules[i].app;
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

	bool ProcessImpl::WriteFilesToDisk()
	{
		TimerScope ts(m_processStats.writeFiles);
		ScopedWriteLock lock(m_writtenFilesLock);
		for (auto& kv : m_writtenFiles)
		{
			if (kv.second.owner != this)
				continue;
			kv.second.owner = nullptr;
			if (kv.second.mappingHandle.IsValid())
				if (!m_session.WriteFileToDisk(*this, kv.second))
					return false;
		}
		return true;
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

	struct ProcessImpl::PipeReader
	{
		PipeReader(ProcessImpl& p, LogEntryType lt) : process(p), logType(lt) {}
		~PipeReader()
		{
			if (!currentString.empty())
				process.LogLine(false, TString(currentString), logType);
		}

		void ReadData(char* buf, u32 readCount)
		{
			char* startPos = buf;
			while (true)
			{
				char* endOfLine = strchr(startPos, '\n');
				if (!endOfLine)
				{
					currentString.append(TString(startPos, startPos + strlen(startPos)));
					return;
				}
				char* newStart = endOfLine + 1;
				if (endOfLine > buf && endOfLine[-1] == '\r')
					--endOfLine;
				currentString.append(TString(startPos, endOfLine));
				process.LogLine(false, TString(currentString), logType);
				currentString.clear();
				startPos = newStart;
			}
		}

		ProcessImpl& process;
		LogEntryType logType;
		TString currentString;
	};

	u32 ProcessImpl::InternalCreateProcess(bool runningRemote, void* environment, FileMappingHandle communicationHandle, u64 communicationOffset)
	{
		ScopedWriteLock initLock(m_initLock);
		Logger& logger = m_session.m_logger;

#if PLATFORM_WINDOWS

		HANDLE readPipe = INVALID_HANDLE_VALUE;
		auto readPipeGuard = MakeGuard([&]() { CloseHandle(readPipe); });

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

			bool isDetachedProcess = GetApplicationRules()[m_rulesIndex].rules->AllowDetach() && m_detourEnabled;

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

				if (m_detourEnabled)
				{
					if (DetourCreateProcessWithDlls(NULL, (tchar*)commandLine.c_str(), NULL, NULL, inheritHandles, creationFlags, environment, workingDir, &si, &processInfo, sizeof_array(dlls), dlls, NULL))
						break;
				}
				else
				{
					SECURITY_ATTRIBUTES saAttr;
					saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
					saAttr.bInheritHandle = TRUE;
					saAttr.lpSecurityDescriptor = NULL;

					HANDLE writePipe;
					if (!CreatePipe(&readPipe, &writePipe, &saAttr, 0))
					{
						logger.Error(TC("CreatePipe failed"));
						return UBA_EXIT_CODE(18);
					}

					auto writePipeGuard = MakeGuard([&]() { CloseHandle(writePipe); });

					if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0))
					{
						logger.Error(TC("SetHandleInformation failed"));
						return UBA_EXIT_CODE(18);
					}

					si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
					si.hStdError = writePipe;
					si.hStdOutput = writePipe;
					si.dwFlags |= STARTF_USESTDHANDLES;

					if (CreateProcessW(NULL, (tchar*)commandLine.c_str(), NULL, NULL, TRUE, creationFlags, environment, workingDir, &si, &processInfo))
						break;
				}

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

		if (m_detourEnabled)
		{
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
			payload.useCustomAllocator = m_startInfo.useCustomAllocator && GetApplicationRules()[m_rulesIndex].rules->AllowMiMalloc();
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

		CloseHandle(m_nativeThreadHandle);
		m_nativeThreadHandle = 0;

		if (!m_detourEnabled)
		{
			PipeReader pipeReader(*this, LogEntryType_Info);
			TString currentString;
			while (true)
			{
				char buf[4096];
				DWORD readCount = 0;
				if (!::ReadFile(readPipe, buf, sizeof(buf) - 1, &readCount, NULL))
					break;
				buf[readCount] = 0;
				pipeReader.ReadData(buf, readCount);
			}
		}

#else // #if PLATFORM_WINDOWS

		if (!m_parentProcess)
		{
			const char* realApplication = m_realApplication.c_str();
			StringBuffer<> tempApplication;
			if (m_realApplication.find(' ') != TString::npos)
			{
				tempApplication.Append('\"').Append(m_realApplication).Append('\"');
				realApplication = tempApplication.data;
			}

			Vector<TString> arguments;
			if (!ParseArguments(arguments, m_startInfo.arguments))
			{
				logger.Error("Failed to parse arguments: %s", m_startInfo.arguments);
				return UBA_EXIT_CODE(16);
			}
			Vector<const char*> arguments2;
			arguments2.reserve(arguments.size() + 2);
			arguments2.push_back(m_virtualApplication.data());
			for (auto& s : arguments)
				arguments2.push_back(s.data());
			arguments2.push_back(nullptr);
			auto argsArray = arguments2.data();

			short flags = POSIX_SPAWN_SETPGROUP;

			posix_spawnattr_t attr;
			int res = posix_spawnattr_init(&attr);
			UBA_ASSERTF(res == 0, TC("posix_spawnattr_init (%s)"), strerror(errno));
			auto attrGuard = MakeGuard([&]() { posix_spawnattr_destroy(&attr); });

			res = posix_spawnattr_setflags(&attr, flags);
			UBA_ASSERTF(res == 0, TC("posix_spawnattr_setflags (%s)"), strerror(errno));
			res = posix_spawnattr_setpgroup(&attr, getpgrp());
			UBA_ASSERTF(res == 0, TC("posix_spawnattr_setpgroup (%s)"), strerror(errno));

			posix_spawn_file_actions_t fileActions;
			res = posix_spawn_file_actions_init(&fileActions);
			UBA_ASSERTF(res == 0, TC("posix_spawn_file_actions_init (%s)"), strerror(errno));
			auto actionsGuard = MakeGuard([&]() { posix_spawn_file_actions_destroy(&fileActions); });
			
			if (*m_realWorkingDir)
			{
				#if PLATFORM_MAC
				posix_spawn_file_actions_addchdir_np(&fileActions, m_realWorkingDir);
				#else
				//UBA_ASSERT(false); // TODO: Revisit
				#endif
			}

			StringBuffer<128> comIdVar;
			StringBuffer<512> workingDir;
			StringBuffer<32> rulesStr;
			StringBuffer<512> logFile;
			StringBuffer<512> ldLibraryPath;
			StringBuffer<512> detoursVar;
			StringBuffer<32> processVar;

			Vector<const char*> envvars;

			const char* it = (const char*)environment;
			while (*it)
			{
				const char* s = it;
				envvars.push_back(s);
				it += TStrlen(s) + 1;
			}

			int outPipe[2] = { 0 };
			int errPipe[2] = { 0 };
			auto pipeGuard0 = MakeGuard([&]() { if (outPipe[0]) close(outPipe[0]); if (errPipe[0]) close(errPipe[0]); });
			auto pipeGuard1 = MakeGuard([&]() { if (outPipe[1]) close(outPipe[1]); if (errPipe[1]) close(errPipe[1]); });

			if (m_detourEnabled)
			{
				const char* detoursLib = m_session.m_detoursLibrary.c_str();
				if (*detoursLib)
				{
#if PLATFORM_LINUX
					//if (strchr(detoursLib, ' '))
					{
						const char* lastSlash = strrchr(detoursLib, '/');
						UBA_ASSERT(lastSlash);
						StringBuffer<> ldLibPath;
						ldLibPath.Append(detoursLib, lastSlash - detoursLib);
						ldLibraryPath.Append("LD_LIBRARY_PATH=").Append(ldLibPath);
						detoursLib = lastSlash + 1;
					}
#endif
				}
				else
					detoursLib = "./" UBA_DETOURS_LIBRARY;

#if PLATFORM_LINUX
				detoursVar.Append("LD_PRELOAD=").Append(detoursLib);
#else
				detoursVar.Append("DYLD_INSERT_LIBRARIES=").Append(detoursLib);
#endif

				processVar.Append("UBA_SESSION_PROCESS=").AppendValue(getpid());


				comIdVar.Append("UBA_COMID=").AppendValue(communicationHandle.uid).Append('+').AppendValue(communicationOffset);
				workingDir.Append("UBA_CWD=").Append(m_realWorkingDir);
				rulesStr.Append("UBA_RULES=").AppendValue(m_rulesIndex);

				if (*m_startInfo.logFile)
				{
#if !UBA_DEBUG_LOG_ENABLED
					static bool runOnce = [&]() { logger.Warning(TC("Build has log files disabled so no logs will be produced")); return false; }();
#endif
					logFile.Append("UBA_LOGFILE=").Append(m_startInfo.logFile);
				}

				if (ldLibraryPath.count)
					envvars.push_back(ldLibraryPath.data);
				envvars.push_back(detoursVar.data);
				envvars.push_back(processVar.data);
				envvars.push_back(comIdVar.data);
				envvars.push_back(workingDir.data);
				envvars.push_back(rulesStr.data);
				if (runningRemote)
					envvars.push_back("UBA_REMOTE=1");
				if (!logFile.IsEmpty())
					envvars.push_back(logFile.data);
			}
			else
			{
				if (pipe(outPipe) || pipe(errPipe))
				{
					logger.Error("pipe failed (%s)", strerror(errno));
					return UBA_EXIT_CODE(18);
				}

				pipeGuard0.Cancel(); // TODO Should this be here? If process fails, will the below actions execute?

				posix_spawn_file_actions_addclose(&fileActions, outPipe[0]);
				posix_spawn_file_actions_addclose(&fileActions, errPipe[0]);
				posix_spawn_file_actions_adddup2(&fileActions, outPipe[1], 1);
				posix_spawn_file_actions_adddup2(&fileActions, errPipe[1], 2);

				posix_spawn_file_actions_addclose(&fileActions, outPipe[1]);
				posix_spawn_file_actions_addclose(&fileActions, errPipe[1]);
			}

			envvars.push_back(nullptr);

			u32 retryCount = 0;
			pid_t processID;
			while (true)
			{
				ScopedReadLock lock(m_session.m_hackToPreventETXTBUSY);

				res = posix_spawnp(&processID, m_realApplication.c_str(), &fileActions, &attr, (char**)argsArray, (char**)envvars.data());
				if (res == 0)
					break;

				if (errno == ETXTBSY && retryCount < 5)
				{
					logger.Warning(TC("posix_spawn failed with ETXTBSY, will retry %s %s (Working dir: %s)"), m_realApplication.c_str(), m_startInfo.arguments, m_realWorkingDir);
					Sleep(2000);
					++retryCount;
					continue;
				}

				logger.Error(TC("posix_spawn failed: %s %s (Working dir: %s) -> %i (%s)"), m_realApplication.c_str(), m_startInfo.arguments, m_realWorkingDir, res, strerror(errno));
				return UBA_EXIT_CODE(12);
			}
			
			errno = 0;
			int prio = getpriority(PRIO_PROCESS, processID);
			if (prio != -1)
			{
				errno = 0;
				if (setpriority(PRIO_PROCESS, processID, prio + 2) == -1)
				{
					UBA_ASSERTF(errno == ESRCH || errno == EPERM, TC("setpriority failed: %s. pid: %i prio: %i (%s)"), m_realApplication.c_str(), processID, prio + 2, strerror(errno));
				}
			}

			m_nativeProcessHandle = (ProcHandle)1;
			m_nativeProcessId = u32(processID);

			if (!m_detourEnabled)
			{
				pipeGuard1.Execute();

				PipeReader outReader(*this, LogEntryType_Info);
				PipeReader errReader(*this, LogEntryType_Error);

				pollfd plist[] = { {outPipe[0],POLLIN, 0}, {errPipe[0],POLLIN, 0} };

				for (int rval; (rval = poll(plist, sizeof_array(plist), -1)) > 0;)
				{
					if (plist[0].revents & POLLHUP && plist[1].revents & POLLHUP) // If they both have hung up we hang up
						break;

					if (plist[0].revents & POLLERR || plist[1].revents & POLLERR) // If there is an error on any of them we hang up
					{
						logger.Error(TC("pipe polling error"));
						break;
					}
					int fd = 0;
					PipeReader* pipeReader = nullptr;
					if (plist[0].revents & POLLIN)
					{
						fd = outPipe[0];
						pipeReader = &outReader;
					}
					else if (plist[1].revents & POLLIN)
					{
						fd = errPipe[0];
						pipeReader = &errReader;
					}
					else
						break; // nothing left to read

					char buffer[1024];
					int bytesRead = read(fd, buffer, sizeof_array(buffer) - 1);
					buffer[bytesRead] = 0;
					pipeReader->ReadData(buffer, bytesRead);
				}
			}
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
			if (m_gotExitMessage || !m_detourEnabled)
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

	bool ParseArguments(Vector<TString>& outArguments, const tchar* argumentString)
	{
		const tchar* argStart = argumentString;
		bool isInArg = false;
		bool isInQuotes = false;
		bool isEnd = *argumentString == 0;
		tchar lastChar = 0;
		for (const tchar* it = argumentString; !isEnd; lastChar = *it, ++it)
		{
			isEnd = *it == 0;
			if (*it == ' ' || *it == '\t' || isEnd)
			{
				if (isInQuotes || !isInArg)
					continue;

				TString result(argStart, it);
				tchar lastChar2 = 0;
				for (auto i = result.begin(); i != result.end();)
				{
					if (*i == '\"')
					{
						if (lastChar2 == '\\')
							i = result.erase(i - 1) + 1;
						else
							i = result.erase(i);
						lastChar2 = 0;
						continue;
					}
					lastChar2 = *i;
					++i;
				}

				outArguments.push_back(std::move(result));
				isInArg = false;
				continue;
			}

			if (!isInArg)
			{
				isInArg = true;
				argStart = it;
				if (*it == '\"')
					isInQuotes = true;
				continue;
			}

			if (*it == '\"')
			{
				if (isInQuotes && lastChar == '\\')
					continue;

				isInQuotes = !isInQuotes;
			}
		}
		return true;
	}
}
