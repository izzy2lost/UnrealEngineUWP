// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "Templates/SharedPointer.h"

class FChaosVDEngine;
class FChaosVDTraceModule;

namespace TraceServices { class IAnalysisSession; }

/**
 * Structure containing info about a trace session used by Chaos Visual Debugger
 */
struct FChaosVDTraceSessionDescriptor
{
	FString SessionName;
	bool bIsLiveSession = false;
};

/** Manager class used by Chaos VD to interact/control UE Trace systems */
class FChaosVDTraceManager
{
public:
	FChaosVDTraceManager();
	~FChaosVDTraceManager();

	/** Load a trace file and starts analyzing it
	 * @param InTraceFilename File Name including (Path Included) of the Trace file to load
	 */
	FString LoadTraceFile(const FString& InTraceFilename);
	
	/**
	 * Connects to a live Trace Session and starts analyzing it.
	 * @param InSessionHost Trace Store Address for this session
	 * @param SessionID Trace ID in the Trace Store provided as host
	 * @return Session Name
	 */
	FString ConnectToLiveSession(FStringView InSessionHost, uint32 SessionID);

	/** Returns the path to the local trace store */
	FString GetLocalTraceStoreDirPath();

	/** Returns a ptr to the session registered with the provided session name. Null if no session is found */
	TSharedPtr<const TraceServices::IAnalysisSession> GetSession(const FString& InSessionName);

	/** Stops and de-registers a trace session registered with the provided session name */
	void CloseSession(const FString& InSessionName);

private:

	/** The trace analysis session. */
	TMap<FString, TSharedPtr<const TraceServices::IAnalysisSession>> AnalysisSessionByName;

	TSharedPtr<FChaosVDTraceModule> ChaosVDTraceModule;
};
