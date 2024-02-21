// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/TypeCompatibleBytes.h"
#include "VVMBytecode.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMType.h"
#include "VVMUTF8String.h"

namespace Verse
{
struct FAbstractVisitor;
/*
This is layed out in Memory:
VProcedure
TWriteBarrier<VValue>          Constant  [0]
TWriteBarrier<VValue>          Constant  [1]
...
TWriteBarrier<VValue>          Constant  [NumConstants - 1];
TWriteBarrier<VUniqueString>   NamedParam[0]
TWriteBarrier<VUniqueString>   NamedParam[1]
...
TWriteBarrier<VUniqueString>   NamedParam[NumNamedParameters - 1];
FOp                            Ops
  + NumOpBytes                 EOD
*/
struct VProcedure : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	const uint32 NumParameters;
	const uint32 NumNamedParameters;
	const uint32 NumRegisters;
	const uint32 NumOpBytes;

	const uint32 NumConstants;
	TWriteBarrier<VValue> Constants[];

	FOp* GetOpsBegin() { return BitCast<FOp*>(GetNameParamsEnd()); }
	FOp* GetOpsEnd() { return BitCast<FOp*>(BitCast<uint8*>(GetOpsBegin()) + NumOpBytes); }

	// In bytes.
	uint32 BytecodeOffset(const FOp& Bytecode)
	{
		return BytecodeOffset(&Bytecode);
	}

	uint32 BytecodeOffset(const void* Data)
	{
		checkSlow(GetOpsBegin() <= Data && Data < GetOpsEnd());
		return static_cast<uint32>(BitCast<char*>(Data) - BitCast<char*>(GetOpsBegin()));
	}

	TWriteBarrier<VValue>* GetConstantsBegin()
	{
		return Constants;
	}
	TWriteBarrier<VValue>* GetConstantsEnd()
	{
		return GetConstantsBegin() + NumConstants;
	}

	void SetConstant(FAllocationContext Context, FConstantIndex ConstantIndex, VValue Value)
	{
		checkSlow(ConstantIndex.Index < NumConstants);
		Constants[ConstantIndex.Index].Set(Context, Value);
	}

	VValue GetConstant(FConstantIndex ConstantIndex)
	{
		checkSlow(ConstantIndex.Index < NumConstants);
		return Constants[ConstantIndex.Index].Get();
	}

	TWriteBarrier<VUniqueString>* GetNamedParams()
	{
		return (TWriteBarrier<VUniqueString>*)GetConstantsEnd();
	}
	TWriteBarrier<VUniqueString>* GetNameParamsEnd()
	{
		return GetNamedParams() + NumNamedParameters;
	}

	static VProcedure& New(FAllocationContext Context, uint32 NumParameters, uint32 NumNamedParameters, uint32 NumRegisters, uint32 NumConstants, size_t NumOpBytes)
	{
		const size_t NumBytes = offsetof(VProcedure, Constants)
							  + sizeof(Constants[0]) * NumConstants
							  + sizeof(TWriteBarrier<VValue>) * NumNamedParameters
							  + NumOpBytes;
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, NumBytes)) VProcedure(Context, NumParameters, NumNamedParameters, NumRegisters, NumConstants, NumOpBytes);
	}

	static void SerializeImpl(VProcedure*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

private:
	VProcedure(FAllocationContext Context, uint32 InNumArguments, uint32 InNumNamedParameters, uint32 InNumRegisters, uint32 InNumConstants, uint32 InNumOpBytes)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumParameters(InNumArguments)
		, NumNamedParameters(InNumNamedParameters)
		, NumRegisters(InNumRegisters)
		, NumOpBytes(InNumOpBytes)
		, NumConstants(InNumConstants)
	{
		for (uint32 ConstantIndex = 0; ConstantIndex < NumConstants; ++ConstantIndex)
		{
			new (&Constants[ConstantIndex]) TWriteBarrier<VValue>{};
		}
	}

	/// Overridden from `VCell` because we want to ensure that the variadic arguments allocated in the function get de-allocated
	/// once the function object lifetime ends. Otherwise they would not get their destructors called normally.
	~VProcedure();

	template <typename FuncType>
	void ForEachOpCode(FuncType&& Func);

	template <typename TVisitor>
	void VisitOpCodes(TVisitor& Visitor);
	void LoadOpCodes(FAbstractVisitor& Visitor);
	void SaveOpCodes(FAbstractVisitor& Visitor);
};
} // namespace Verse
#endif // WITH_VERSE_VM
