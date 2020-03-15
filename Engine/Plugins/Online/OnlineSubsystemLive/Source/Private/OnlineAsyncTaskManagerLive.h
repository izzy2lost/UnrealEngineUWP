// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemLivePackage.h"
#include "OnlineSubsystemLive.h"

// @ATG_CHANGE : UWP LIVE support - moved platform specific includes to "OnlineSubsystemLive.h"

class FOnlineSubsystemLive;

/**
 * Base class that holds a delegate to fire when a given async task is complete
 */
class FOnlineAsyncTaskLive : public FOnlineAsyncTaskBasic<FOnlineSubsystemLive>
{
PACKAGE_SCOPE:
	/** Live user index associated with this task **/
	int32 UserIndex;

public:
	/** Hidden on purpose */
	FOnlineAsyncTaskLive() = delete;

	explicit FOnlineAsyncTaskLive(FOnlineSubsystemLive* const InLiveSubsystem, const int32 InUserIndex = -1)
		: FOnlineAsyncTaskBasic(InLiveSubsystem)
		, UserIndex(InUserIndex)
	{
	}

	virtual ~FOnlineAsyncTaskLive() = default;

	/**
	 * Handle start-up/initialize code here.  This is ran on the Game Thread the tick before the first call to Tick().
	 */
	virtual void Initialize() override
	{
	}

	/**
	 * Handle processing/updating the task here.  This is ran on the Online Thread.
	 */
	virtual void Tick() override
	{
	}
};

/**
 * Task to wrap processing of IAsyncOperation/Concurrency operations into the FOnlineAsyncTask system
 */
template<typename Result_t, typename AsyncOperation_t = Windows::Foundation::IAsyncOperation<Result_t>>
class FOnlineAsyncTaskConcurrencyLive
	: public FOnlineAsyncTaskBasic<FOnlineSubsystemLive>
{
	static_assert(std::is_base_of<Windows::Foundation::IAsyncInfo, AsyncOperation_t>::value, "AsyncOperation_t must derive from IAsyncInfo");

public:
	/**
	 * Fires off a Concurrency::task to be later handled in ProcessResult
	 */
	FOnlineAsyncTaskConcurrencyLive(FOnlineSubsystemLive* const InLiveSubsystem, Windows::Xbox::System::User^ InLocalUser, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext = nullptr)
		: FOnlineAsyncTaskBasic(InLiveSubsystem)
		, LocalUser(InLocalUser)
		, LiveContext(InLiveContext)
	{
		check(LocalUser);
	}

	/**
	 * Fires off a Concurrency::task to be later handled in ProcessResult
	 */
	FOnlineAsyncTaskConcurrencyLive(FOnlineSubsystemLive* const InLiveSubsystem, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext)
		: FOnlineAsyncTaskBasic(InLiveSubsystem)
		, LocalUser(nullptr)
		, LiveContext(InLiveContext)
	{
		check(LiveContext);
	}

protected:
	/**
	 * Default Destructor
	 */
	virtual ~FOnlineAsyncTaskConcurrencyLive() = default;

private:
	/**
	 * Handles creating an AsyncOperation.  The task should be created and returned in this method
	 *
	 * @Return The AsyncOperation to run, or nullptr if there was a failure
	 */
	virtual AsyncOperation_t^ CreateOperation() = 0;

	/**
	 * Handles processing of an async result.  Any processed data should be stored on the task until Finalize is called on the Game thread.
	 *
	 * @Param CompletedTask The finished task to call get() on
	 * @Return Whether or not our task was successful and is stored in bWasSuccessful
	 */
	virtual bool ProcessResult(const Concurrency::task<Result_t>& CompletedTask) = 0;

	/**
	 * Create and start processing our AsyncOperation in a task
	 */
	virtual void Initialize() override final
	{
		AsyncOperation_t^ AsyncOperation = nullptr;
		try
		{
			UE_LOG_ONLINE(Verbose, TEXT("Starting task for %s."), *ToString());
			AsyncOperation = CreateOperation();
		}
		catch (const Concurrency::task_canceled& Ex)
		{
			UE_LOG_ONLINE(Warning, TEXT("Unhandled task_canceled exception caught in %s; operation failed with reason '%s'."), *ToString(), ANSI_TO_TCHAR(Ex.what()));
		}
		catch (const std::exception& Ex)
		{
			UE_LOG_ONLINE(Warning, TEXT("Unhandled std::exception exception caught in %s; operation failed for reason '%s'."), *ToString(), ANSI_TO_TCHAR(Ex.what()));
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG_ONLINE(Warning, TEXT("Unhandled %s exception caught in %s; operation failed with code 0x%08X and reason '%s'."), Ex->GetType()->ToString()->Data(), *ToString(), Ex->HResult, Ex->Message->Data());
		}
		catch (...)
		{
			// Anything we don't handle above, just crash on
			checkf(false, TEXT("Unhandled exception caught in %s."), *ToString());
		}

		// If our operation is nullptr, we failed to init it
		if (AsyncOperation == nullptr)
		{
			bWasSuccessful = false;
			bIsComplete = true;
		}
		else
		{
			Task = Concurrency::create_task(AsyncOperation);
		}
	}

	/**
	 * Tick until the task is complete.  Once complete, the task will be forwarded to ProcessResult
	 */
	virtual void Tick() override final
	{
		// If our AsyncOperation failed to be created, bIsComplete will be set to true already
		if (bIsComplete)
		{
			return;
		}

		// Tick until our task is done
		if (Task.is_done())
		{
			// Concurrency tasks like to throw a lot of exceptions, and we don't want to risk
			// any uncaught exceptions taking us down
			bWasSuccessful = false;
			try
			{
				UE_LOG_ONLINE(Verbose, TEXT("Processing Results for task %s()."), *ToString());
				bWasSuccessful = ProcessResult(Task);
			}
			catch (const Concurrency::task_canceled& Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("Unhandled task_canceled exception caught in %s(); task was cancelled for reason '%s'."), *ToString(), ANSI_TO_TCHAR(Ex.what()));
			}
			catch (const std::exception& Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("Unhandled std::exception exception caught in %s(); task failed for reason '%s'."), *ToString(), ANSI_TO_TCHAR(Ex.what()));
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("Unhandled %s exception caught in %s(); task failed with code 0x%08X and reason '%s'."), Ex->GetType()->ToString()->Data(), *ToString(), Ex->HResult, Ex->Message->Data());
			}
			catch (...)
			{
				// Anything we don't handle above, just crash on
				checkf(false, TEXT("Unhandled exception caught in %s()."), *ToString());
			}

			// We're done!
			bIsComplete = true;
		}
	}

protected:
	Windows::Xbox::System::User^ LocalUser;
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext;

private:
	Concurrency::task<Result_t> Task;
};

/**
 * Helper for IAsyncActions, which do not have a return type
 */
using FOnlineAsyncTaskConcurrencyLiveAsyncAction = FOnlineAsyncTaskConcurrencyLive<void, Windows::Foundation::IAsyncAction>;

/**
 *	Live version of the async task manager to register the various Live callbacks with the engine
 */
class FOnlineAsyncTaskManagerLive : public FOnlineAsyncTaskManager
{
protected:

	/** Cached reference to the main online subsystem */
	FOnlineSubsystemLive* LiveSubsystem;

public:

	FOnlineAsyncTaskManagerLive(FOnlineSubsystemLive* InOnlineSubsystem)
		: LiveSubsystem(InOnlineSubsystem)
	{
	}

	virtual ~FOnlineAsyncTaskManagerLive() = default;

	// FOnlineAsyncTaskManager
	virtual void OnlineTick() override;
};
