// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemLive.h"
#include "OnlineSubsystemLivePackage.h"

/**
 * Base class that holds a delegate to fire when a given async task is complete
 */
class FOnlineAsyncTaskLive : public FOnlineAsyncTaskBasic<FOnlineSubsystemLive>
{
PACKAGE_SCOPE:

	/** Live user index associated with this task **/
	int32 UserIndex;

	/** Reference to the main Live subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;
	
	/** Has this request been started */
	bool bInit;

private:
	/** Hidden on purpose */
	FOnlineAsyncTaskLive() 
		: UserIndex(0)
		, LiveSubsystem(NULL)
		, bInit(false)
	{
	}

public:

	FOnlineAsyncTaskLive(class FOnlineSubsystemLive* InLiveSubsystem, int32 InUserIndex) 
		: FOnlineAsyncTaskBasic(InLiveSubsystem )
		, UserIndex(InUserIndex)
		, LiveSubsystem(InLiveSubsystem)
		, bInit(false)
	{
	}

	virtual ~FOnlineAsyncTaskLive()
	{
	}

	//. Start function must be defined to kick off any LIVE async work
	virtual void Start() = 0;

	/**
	 * By default we just start the task on first tick - override to change this behaviour
	 */
	virtual void Tick() override
	{
		if (!bInit)
		{
			bInit = true;
			Start();
		}
	}
};

/**
 *	Live version of the async task manager to register the various Live callbacks with the engine
 */
class FOnlineAsyncTaskManagerLive : public FOnlineAsyncTaskManager
{
protected:

	/** Cached reference to the main online subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;

public:

	FOnlineAsyncTaskManagerLive(class FOnlineSubsystemLive* InOnlineSubsystem)
		: LiveSubsystem(InOnlineSubsystem)
	{
	}

	~FOnlineAsyncTaskManagerLive() 
	{
	}

	// FOnlineAsyncTaskManager
	virtual void OnlineTick() override;
};


