// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFM/AutoRTFM.h"
#include "CoreGlobals.h"
#include "HAL/IConsoleManager.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"

namespace
{
	// Move this to a local only and use functions to access this
#if UE_AUTORTFM_ENABLED_RUNTIME_BY_DEFAULT
	int GAutoRTFMRuntimeEnabled = AutoRTFM::EAutoRTFMEnabledState::AutoRTFM_Enabled;
#else
	int GAutoRTFMRuntimeEnabled = AutoRTFM::EAutoRTFMEnabledState::AutoRTFM_Disabled;
#endif // UE_AUTORTFM_ENABLED_RUNTIME_BY_DEFAULT

	void UpdateAutoRTFMRuntimeCrashData()
	{
		FGenericCrashContext::SetGameData(TEXT("IsAutoRTFMRuntimeEnabled"), AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled() ? TEXT("true") : TEXT("false"));
	}
}

#if UE_AUTORTFM
static FAutoConsoleVariableRef CVarAutoRTFMRuntimeEnabled(
	TEXT("AutoRTFMRuntimeEnabled"),
	GAutoRTFMRuntimeEnabled,
	TEXT("Enables the AutoRTFM runtime"),
	FConsoleVariableDelegate::CreateLambda([] (IConsoleVariable*) { UpdateAutoRTFMRuntimeCrashData(); }),
	ECVF_Default
);

static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
{
	UpdateAutoRTFMRuntimeCrashData();
});
#endif

namespace AutoRTFM
{
	namespace ForTheRuntime
	{
		bool SetAutoRTFMRuntime(EAutoRTFMEnabledState State)
		{
			// #noop if AutoRTFM is not compiled in
#if UE_AUTORTFM
			switch (GAutoRTFMRuntimeEnabled)
			{
			default:
				break;
			case EAutoRTFMEnabledState::AutoRTFM_ForcedDisabled:
				UE_LOG(LogCore, Log, TEXT("Ignoring changing AutoRTFM runtime state due to GAutoRTFMRuntimeEnabled being set to forced disabled."));
				return false;
			case EAutoRTFMEnabledState::AutoRTFM_ForcedEnabled:
				UE_LOG(LogCore, Log, TEXT("Ignoring changing AutoRTFM runtime state due to GAutoRTFMRuntimeEnabled being set to forced enabled."));
				return false;
			}

			GAutoRTFMRuntimeEnabled = State;

			UpdateAutoRTFMRuntimeCrashData();

			return true;
#else
			return false;
#endif
		}

		bool IsAutoRTFMRuntimeEnabled()
		{
			// #noop if AutoRTFM is not compiled in
#if UE_AUTORTFM
			switch (GAutoRTFMRuntimeEnabled)
			{
			default:
				return false;
			case EAutoRTFMEnabledState::AutoRTFM_Enabled:
			case EAutoRTFMEnabledState::AutoRTFM_ForcedEnabled:
			case EAutoRTFMEnabledState::AutoRTFM_EnabledForAllVerse:
				return true;
			}
#else
			return false;
#endif
		}

		bool IsAutoRTFMRuntimeEnabledForAllVerse()
		{
			// #noop if AutoRTFM is not compiled in
#if UE_AUTORTFM
			switch (GAutoRTFMRuntimeEnabled)
			{
			default:
				return false;
			case EAutoRTFMEnabledState::AutoRTFM_EnabledForAllVerse:
				return true;
			}
#else
			return false;
#endif
		}
	}
}

#if (defined(__AUTORTFM) && __AUTORTFM)
#include "AutoRTFM/AutoRTFMConstants.h"
#include "CallNest.h"
#include "Context.h"
#include "ContextInlines.h"
#include "ContextStatus.h"
#include "FunctionMapInlines.h"
#include "TransactionInlines.h"
#include "Toggles.h"
#include "Utils.h"

#include "Templates/Tuple.h"

// This is the implementation of the AutoRTFM.h API. Ideally, functions here should just delegate to some internal API.
// For now, I have these functions also perform some error checking.

namespace AutoRTFM
{

UE_AUTORTFM_FORCEINLINE autortfm_result TransactThenOpenImpl(void (*UninstrumentedWork)(void*), void* Arg)
{
	return static_cast<autortfm_result>(
		AutoRTFM::Transact([&]
		{
			autortfm_open(UninstrumentedWork, Arg);
		}));
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_is_transactional") bool autortfm_is_transactional()
{
	if (ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		return FContext::Get()->IsTransactional();
	}

	return false;
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_is_closed") bool autortfm_is_closed()
{
    return false;
}

// First Part - the API exposed outside transactions.
extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_transact") autortfm_result autortfm_transact(void (*UninstrumentedWork)(void*), void (*InstrumentedWork)(void*), void* Arg)
{
	if (ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
	    return static_cast<autortfm_result>(FContext::Get()->Transact(InstrumentedWork, Arg));
	}

	(*UninstrumentedWork)(Arg);
	return autortfm_committed;
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_transact_then_open") autortfm_result autortfm_transact_then_open(void (*UninstrumentedWork)(void*), void (*InstrumentedWork)(void*), void* Arg)
{
	return TransactThenOpenImpl(UninstrumentedWork, Arg);
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_commit") void autortfm_commit(void (*UninstrumentedWork)(void*), void (*InstrumentedWork)(void*), void* Arg)
{
    autortfm_result Result = autortfm_transact(UninstrumentedWork, InstrumentedWork, Arg);
	UE_CLOG(Result != autortfm_committed, LogAutoRTFM, Fatal, TEXT("Unexpected transaction result: %u."), Result);
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_abort") void autortfm_abort()
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_abort` was called from outside a transaction."));
	FContext::Get()->AbortByRequestAndThrow();
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_start_transaction") bool autortfm_start_transaction()
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_start_transaction` was called from outside a transact."));
	return FContext::Get()->StartTransaction();
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_commit_transaction") autortfm_result autortfm_commit_transaction()
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_commit_transaction` was called from outside a transact."));
	return static_cast<autortfm_result>(FContext::Get()->CommitTransaction());
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_abort_transaction") autortfm_result autortfm_abort_transaction()
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_abort_transaction` was called from outside a transact."));
	FContext* const Context = FContext::Get();
	return static_cast<autortfm_result>(Context->AbortTransaction(false, false));
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_cascading_abort_transaction") autortfm_result autortfm_cascading_abort_transaction()
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_cascading_abort_transaction` was called from outside a transact."));
	FContext* const Context = FContext::Get();
	return static_cast<autortfm_result>(Context->AbortTransaction(false, true));
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_clear_transaction_status") void autortfm_clear_transaction_status()
{
	ASSERT(FContext::Get()->IsAborting());
	FContext::Get()->ClearTransactionStatus();
}

extern "C" UE_AUTORTFM_NOAUTORTFM bool autortfm_is_aborting()
{
	if (ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		return FContext::Get()->IsAborting();
	}

	return false;
}

extern "C" UE_AUTORTFM_NOAUTORTFM bool autortfm_current_nest_throw()
{
	FContext::Get()->Throw();
	return true;
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_open") void autortfm_open(void (*Work)(void*), void* Arg)
{
	Work(Arg);
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_close") autortfm_status autortfm_close(void (*UninstrumentedWork)(void*), void (*InstrumentedWork)(void*), void* Arg)
{
	autortfm_status Result = autortfm_status_ontrack;

	if (ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("Close called from an outside a transaction."));

		FContext* const Context = FContext::Get();

		if (InstrumentedWork)
		{
			Result = static_cast<autortfm_status>(Context->CallClosedNest(InstrumentedWork, Arg));
		}
		else
		{
			UE_LOG(LogAutoRTFM, Fatal, TEXT("Could not find function %p '%s' where '%s'."), UninstrumentedWork, *GetFunctionDescription(UninstrumentedWork), ANSI_TO_TCHAR("autortfm_close"));
	        Context->AbortByLanguageAndThrow();
		}
	}
	else
	{
		UninstrumentedWork(Arg);
	}

	return Result;
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_record_open_write") void autortfm_record_open_write(void* Ptr, size_t Size)
{
	FContext* const Context = FContext::Get();

    Context->CheckOpenRecordWrite(Ptr);
	Context->RecordWrite(Ptr, Size);
}

extern "C" UE_AUTORTFM_NOAUTORTFM void autortfm_register_open_function(void* OriginalFunction, void* NewFunction)
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("Registering open %p->%p"), OriginalFunction, NewFunction);
    FunctionMapAdd(OriginalFunction, NewFunction);
}

UE_AUTORTFM_AUTORTFM("RTFM_OnCommitInternal") void ForTheRuntime::OnCommitInternal(TFunction<void()> && Work)
{
    Work();
}

UE_AUTORTFM_AUTORTFM("RTFM_OnAbortInternal") void ForTheRuntime::OnAbortInternal(TFunction<void()> && Work)
{
}

UE_AUTORTFM_AUTORTFM("RTFM_PushOnAbortHandlerInternal") void ForTheRuntime::PushOnAbortHandlerInternal(const void* Key, TFunction<void()> && Work)
{
}

UE_AUTORTFM_AUTORTFM("RTFM_PopOnAbortHandlerInternal") void ForTheRuntime::PopOnAbortHandlerInternal(const void* Key)
{
}

void OpenCommit(TFunction<void()>&& Work)
{
	ForTheRuntime::OnCommitInternal(MoveTemp(Work));
}

void OpenAbort(TFunction<void()>&& Work)
{
	ForTheRuntime::OnAbortInternal(MoveTemp(Work));
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_on_commit") void autortfm_on_commit(void (*Work)(void*), void* Arg)
{
    Work(Arg);
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_on_abort") void autortfm_on_abort(void (*Work)(void*), void* Arg)
{
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_push_on_abort_handler") void autortfm_push_on_abort_handler(const void* Key, void (*Work)(void*), void* Arg)
{
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_pop_on_abort_handler") void autortfm_pop_on_abort_handler(const void* Key)
{
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_did_allocate") void* autortfm_did_allocate(void* Ptr, size_t Size)
{
    return Ptr;
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_did_free") void autortfm_did_free(void* Ptr)
{
	// We only need to process did free if we need to track allocation locations.
	if constexpr (bTrackAllocationLocations)
	{
		if (UNLIKELY(GIsCriticalError))
		{
			return;
		}

		if (FContext::IsTransactional())
		{
			FContext* const Context = FContext::Get();

		    // We only care about frees that are occuring when the transaction
		    // is in an on-going state (it's not committing or aborting).
		    if (EContextStatus::OnTrack == Context->GetStatus())
		    {
		    	Context->DidFree(Ptr);
		    }
		}
	}
}

extern "C" UE_AUTORTFM_AUTORTFM("RTFM_autortfm_check_consistency_assuming_no_races") void autortfm_check_consistency_assuming_no_races()
{
    if (FContext::IsTransactional())
    {
        AutoRTFM::Unreachable();
    }
}

extern "C" UE_AUTORTFM_NOAUTORTFM void autortfm_check_abi(void* const Ptr, const size_t Size)
{
    struct FConstants final
    {
		const uint32_t Major = AutoRTFM::Constants::Major;
		const uint32_t Minor = AutoRTFM::Constants::Minor;
		const uint32_t Patch = AutoRTFM::Constants::Patch;

		// This is messy - but we want to do comparisons but without comparing any padding bytes.
		// Before C++20 we cannot use a default created operator== and operator!=, so we use this
		// ugly trick to just compare the members.
	private:
		auto Tied() const
		{
			return Tie(Major, Minor, Patch);
		}

	public:
		bool operator==(const FConstants& Other) const
		{
			return Tied() == Other.Tied();
		}

		bool operator!=(const FConstants& Other) const
		{
			return !(*this == Other);
		}
    } RuntimeConstants;

	UE_CLOG(sizeof(FConstants) != Size, LogAutoRTFM, Fatal, TEXT("ABI error between AutoRTFM compiler and runtime."));

    const FConstants* const CompilerConstants = static_cast<FConstants*>(Ptr);

	UE_CLOG(RuntimeConstants != *CompilerConstants, LogAutoRTFM, Fatal, TEXT("ABI error between AutoRTFM compiler and runtime."));
}

// Second Part - the same API exposed inside transactions. Note that we don't expose all of the API
// to transactions! That's intentional. However, things like autortfm_defer_until_commit can be called
// from an open nest in a transaction.
extern "C" UE_AUTORTFM_NOAUTORTFM bool RTFM_autortfm_is_transactional()
{
    return true;
}

extern "C" UE_AUTORTFM_NOAUTORTFM bool RTFM_autortfm_is_closed()
{
    return true;
}

extern "C" UE_AUTORTFM_NOAUTORTFM autortfm_result RTFM_autortfm_transact(void (*UninstrumentedWork)(void*), void (*InstrumentedWork)(void*), void* Arg)
{
	FContext* Context = FContext::Get();
    return static_cast<autortfm_result>(Context->Transact(InstrumentedWork, Arg));
}

extern "C" UE_AUTORTFM_NOAUTORTFM autortfm_result RTFM_autortfm_transact_then_open(void (*UninstrumentedWork)(void*), void (*InstrumentedWork)(void*), void* Arg)
{
    return TransactThenOpenImpl(UninstrumentedWork, Arg);
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_commit(void (*UninstrumentedWork)(void*), void (*InstrumentedWork)(void*), void* Arg)
{
    autortfm_result Result = autortfm_transact(UninstrumentedWork, InstrumentedWork, Arg);
	UE_CLOG(Result != autortfm_committed, LogAutoRTFM, Fatal, TEXT("Unexpected transaction result: %u."), Result);
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_abort()
{
	FContext* Context = FContext::Get();
    Context->AbortByRequestAndThrow();
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_start_transaction()
{
	UE_LOG(LogAutoRTFM, Fatal, TEXT("The function `autortfm_start_transaction` was called from closed code."));
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_commit_transaction()
{
	UE_LOG(LogAutoRTFM, Fatal, TEXT("The function `RTFM_autortfm_commit_transaction` was called from closed code."));
}

extern "C" UE_AUTORTFM_NOAUTORTFM autortfm_result RTFM_autortfm_abort_transaction()
{
	FContext* const Context = FContext::Get();
	return static_cast<autortfm_result>(Context->AbortTransaction(true, false));
}

extern "C" UE_AUTORTFM_NOAUTORTFM autortfm_result RTFM_autortfm_cascading_abort_transaction()
{
	FContext* const Context = FContext::Get();
	return static_cast<autortfm_result>(Context->AbortTransaction(true, true));
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_clear_transaction_status()
{
	UE_LOG(LogAutoRTFM, Fatal, TEXT("The function `autortfm_clear_transaction_status` was called from closed code."));
	AutoRTFM::Unreachable();
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_open(void (*Work)(void*), void* Arg)
{
	Work(Arg);

	FContext* Context = FContext::Get();
	if (Context->IsAborting())
	{
		Context->Throw();
	}
}

extern "C" UE_AUTORTFM_NOAUTORTFM autortfm_status RTFM_autortfm_close(void (*UninstrumentedWork)(void*), void (*InstrumentedWork)(void*), void* Arg)
{
	FContext* const Context = FContext::Get();

	if (InstrumentedWork)
    {
        InstrumentedWork(Arg);
    }
	else
	{
		UE_LOG(LogAutoRTFM, Fatal, TEXT("Could not find function %p '%s' where '%s'."), UninstrumentedWork, *GetFunctionDescription(UninstrumentedWork), ANSI_TO_TCHAR("autortfm_close"));
	    Context->AbortByLanguageAndThrow();
	}

	return static_cast<autortfm_status>(Context->GetStatus());
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_record_open_write(void*, size_t)
{
	UE_LOG(LogAutoRTFM, Fatal, TEXT("The function `autortfm_record_open_write` was called from closed code."));
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_OnCommitInternal(TFunction<void()> && Work)
{
	FContext* Context = FContext::Get();
    ASSERT(Context->GetStatus() == EContextStatus::OnTrack);
    Context->GetCurrentTransaction()->DeferUntilCommit(MoveTemp(Work));
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_OnAbortInternal(TFunction<void()>&& Work)
{
	FContext* Context = FContext::Get();
    ASSERT(Context->GetStatus() == EContextStatus::OnTrack);
    Context->GetCurrentTransaction()->DeferUntilAbort(MoveTemp(Work));
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_PushOnAbortHandlerInternal(const void* Key, TFunction<void()>&& Work)
{
	FContext* Context = FContext::Get();
    ASSERT(Context->GetStatus() == EContextStatus::OnTrack);
    Context->GetCurrentTransaction()->PushDeferUntilAbortHandler(Key, MoveTemp(Work));
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_PopOnAbortHandlerInternal(const void* Key)
{
	FContext* Context = FContext::Get();
    ASSERT(Context->GetStatus() == EContextStatus::OnTrack);
    Context->GetCurrentTransaction()->PopDeferUntilAbortHandler(Key);
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_on_commit(void (*Work)(void*), void* Arg)
{
    RTFM_OnCommitInternal([Work, Arg] { Work(Arg); });
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_on_abort(void (*Work)(void*), void* Arg)
{
    RTFM_OnAbortInternal([Work, Arg] { Work(Arg); });
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_push_on_abort_handler(const void* Key, void (*Work)(void*), void* Arg)
{
    RTFM_PushOnAbortHandlerInternal(Key, [Work, Arg] { Work(Arg); });
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_pop_on_abort_handler(const void* Key)
{
    RTFM_PopOnAbortHandlerInternal(Key);
}

extern "C" UE_AUTORTFM_NOAUTORTFM void* RTFM_autortfm_did_allocate(void* Ptr, size_t Size)
{
	FContext* Context = FContext::Get();
    Context->DidAllocate(Ptr, Size);
    return Ptr;
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_did_free(void* Ptr)
{
	// We should never-ever-ever actually free memory from within closed code of
	// a transaction.
	AutoRTFM::Unreachable();
}

extern "C" UE_AUTORTFM_NOAUTORTFM void RTFM_autortfm_check_consistency_assuming_no_races()
{
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_check_consistency_assuming_no_races);

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
