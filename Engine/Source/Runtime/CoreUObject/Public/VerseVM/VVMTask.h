// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMReturnSlot.h"
#include "VerseVM/VVMTree.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
struct FOp;
struct VFailureContext;

struct VTask : VCell
	, TIntrusiveTree<VTask>
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	// Where execution should continue when suspending.
	FOp* YieldPC;
	TWriteBarrier<VFrame> YieldFrame;
	TWriteBarrier<VTask> YieldTask;
	TWriteBarrier<VFailureContext> FailureContext; // Should not change on suspend - stored only for assertions.

	// Where the task should resume after suspending.
	FOp* ResumePC{nullptr};
	TWriteBarrier<VFrame> ResumeFrame;
	VReturnSlot ResumeSlot;          // May point into ResumeFrame or one of its ancestors.
	TWriteBarrier<VTask> ResumeTask; // May be this or a child task due to leniency.

	bool bSuspended{false};
	void* NativeResumeSlot{nullptr};
	TFunction<void()> NativeDefer;

	COREUOBJECT_API void ResumeInTransaction(FRunningContext Context, VValue ResumeArgument);

	static VTask& New(FAllocationContext Context, FOp* YieldPC, VFrame* YieldFrame, VFailureContext* FailureContext, VTask* Parent)
	{
		return *new (Context.AllocateFastCell(sizeof(VTask))) VTask(Context, YieldPC, YieldFrame, FailureContext, Parent);
	}

	void Suspend(FAccessContext Context)
	{
		ForEach([](VTask& Task) {
			Task.FailureContext.Reset();
			Task.bSuspended = true;
		});
	}

	void Resume(FAccessContext Context, VFailureContext& InheritedFailureContext)
	{
		ForEach([&](VTask& Task) {
			Task.FailureContext.Set(Context, InheritedFailureContext);
			Task.bSuspended = false;
		});
	}

	void FinishedExecuting(FAccessContext Context)
	{
		Detach(Context);
	}

private:
	VTask(FAllocationContext Context, FOp* YieldPC, VFrame* YieldFrame, VFailureContext* FailureContext, VTask* Parent)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, TIntrusiveTree(Context, Parent)
		, YieldPC(YieldPC)
		, YieldFrame(Context, YieldFrame)
		, YieldTask(Context, Parent)
		, FailureContext(Context, FailureContext)
		, ResumeSlot(Context, nullptr)
	{
	}
};
} // namespace Verse
