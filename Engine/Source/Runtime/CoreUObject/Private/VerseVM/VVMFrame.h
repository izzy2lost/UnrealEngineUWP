// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMType.h"

namespace Verse
{
struct FOp;
struct VProcedure;

struct VFrame : VCell
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	const uint32 NumRegisters;
	TWriteBarrier<VFrame> CallerFrame;
	FOp* CallerPC{nullptr};
	VRestValue ReturnEffectToken{0};
	TWriteBarrier<VProcedure> Procedure;
	TWriteBarrier<VValue> ReturnSlot;
	VRestValue Registers[];

	static VFrame& New(FAllocationContext Context, uint32 NumRegisters, VFrame* CallerFrame, FOp* CallerPC, VProcedure& Procedure, VValue ReturnSlot)
	{
		return *new (Context.AllocateFastCell(offsetof(VFrame, Registers) + sizeof(VRestValue) * NumRegisters)) VFrame(Context, NumRegisters, CallerFrame, CallerPC, Procedure, ReturnSlot);
	}

	COREUOBJECT_API static void MarkReferencedCellsImpl(VCell* This, FMarkStack&);

	VFrame& CloneWithoutCallerInfo(FAllocationContext Context)
	{
		return VFrame::New(Context, *this);
	}

private:
	static VFrame& New(FAllocationContext Context, VFrame& Other)
	{
		return *new (Context.AllocateFastCell(offsetof(VFrame, Registers) + sizeof(VRestValue) * Other.NumRegisters)) VFrame(Context, Other);
	}

	VFrame(FAllocationContext Context, uint32 InNumRegisters, VFrame* CallerFrame, FOp* CallerPC, VProcedure& Procedure, VValue ReturnSlot)
		: VCell(Context, VEmergentTypeCreator::GetOrCreate(Context, VTrivialType::Singleton.Get(), &StaticCppClassInfo))
		, NumRegisters(InNumRegisters)
		, CallerFrame(Context, CallerFrame)
		, CallerPC(CallerPC)
		, Procedure(Context, Procedure)
		, ReturnSlot(Context, ReturnSlot)
	{
		for (uint32 RegisterIndex = 0; RegisterIndex < NumRegisters; ++RegisterIndex)
		{
			// TODO SOL-4222: Pipe through proper split depth here.
			new (&Registers[RegisterIndex]) VRestValue(0);
		}
	}

	// We don't copy the CallerFrame/CallerPC because during lenient execution
	// this won't return to the caller.
	VFrame(FAllocationContext Context, VFrame& Other)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumRegisters(Other.NumRegisters)
		, Procedure(Context, Other.Procedure.Get())
		, ReturnSlot(Context, Other.ReturnSlot.Get())
	{
		ReturnEffectToken.Set(Context, Other.ReturnEffectToken.Get(Context));
		for (uint32 RegisterIndex = 0; RegisterIndex < NumRegisters; ++RegisterIndex)
		{
			// TODO SOL-4222: Pipe through proper split depth here.
			new (&Registers[RegisterIndex]) VRestValue(0);
			Registers[RegisterIndex].Set(Context, Other.Registers[RegisterIndex].Get(Context));
		}
	}
};

} // namespace Verse
