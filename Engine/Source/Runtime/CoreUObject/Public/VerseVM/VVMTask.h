// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMGlobalProgram.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/VVMObject.h"
#include "VerseVM/VVMReturnSlot.h"
#include "VerseVM/VVMTree.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
struct FOp;
struct VFailureContext;

struct VTask : VObject
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VObject);
	COREUOBJECT_API static TGlobalHeapPtr<VEmergentType> EmergentType;

	// Tasks to resume on completion.
	TWriteBarrier<VValue> Result;
	TWriteBarrier<VTask> Awaiters;  // Head of a linked list of tasks that have called Await, most recent first.
	TWriteBarrier<VTask> PrevAwait; // Link for when this task is on some other task's Awaiters list.

	// Where execution should continue when suspending.
	FOp* YieldPC;
	TWriteBarrier<VFrame> YieldFrame;
	TWriteBarrier<VTask> YieldTask;
	TWriteBarrier<VFailureContext> FailureContext; // Should not change on suspend - stored only for assertions.

	// Where the task should resume after suspending.
	FOp* ResumePC{nullptr};
	TWriteBarrier<VFrame> ResumeFrame;
	VReturnSlot ResumeSlot; // May point into ResumeFrame or one of its ancestors.

	bool bSuspended{false};
	void* NativeResumeSlot{nullptr};
	TFunction<void()> NativeDefer;

	COREUOBJECT_API void ResumeInTransaction(FRunningContext Context, VValue ResumeArgument);

	static VTask& New(FAllocationContext Context, FOp* YieldPC, VFrame* YieldFrame, VTask* YieldTask, VFailureContext* FailureContext)
	{
		return *new (AllocateFastCell(Context, *EmergentType)) VTask(Context, YieldPC, YieldFrame, YieldTask, FailureContext);
	}

	static FOpResult AwaitImpl(FRunningContext Context, VTask* Task, VValue Scope, VNativeFunction::Args Arguments)
	{
		if (!Scope.IsCellOfType<VTask>())
		{
			V_DIE("Tried to await non-VTask");
		}
		VTask& This = Scope.StaticCast<VTask>();

		if (This.Result.Get().IsUninitialized())
		{
			Task->PrevAwait.Set(Context, This.Awaiters.Get());
			This.Awaiters.Set(Context, Task);
			V_YIELD();
		}
		else
		{
			V_RETURN(This.Result.Get());
		}
	}

	void Suspend(FAccessContext Context)
	{
		FailureContext.Reset();
		bSuspended = true;
	}

	void Resume(FAccessContext Context, VFailureContext& InheritedFailureContext)
	{
		FailureContext.Set(Context, InheritedFailureContext);
		bSuspended = false;
	}

	VTask* FinishedExecuting(FAccessContext Context)
	{
		VTask* ToResume = Awaiters.Get();
		Awaiters.Reset();
		return ToResume;
	}

private:
	VTask(FAllocationContext Context, FOp* YieldPC, VFrame* YieldFrame, VTask* YieldTask, VFailureContext* FailureContext)
		: VObject(Context, *EmergentType)
		, YieldPC(YieldPC)
		, YieldFrame(Context, YieldFrame)
		, YieldTask(Context, YieldTask)
		, FailureContext(Context, FailureContext)
		, ResumeSlot(Context, nullptr)
	{
	}
};
} // namespace Verse

#endif