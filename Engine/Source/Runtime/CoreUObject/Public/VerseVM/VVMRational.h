// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "CoreTypes.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "VVMContext.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMInt.h"
#include "VVMValue.h"

namespace Verse
{

struct VRational : VHeapValue
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VValue> Numerator;
	TWriteBarrier<VValue> Denominator;

	static VRational& Add(FRunningContext, VRational& Lhs, VRational& Rhs);
	static VRational& Sub(FRunningContext, VRational& Lhs, VRational& Rhs);
	static VRational& Mul(FRunningContext, VRational& Lhs, VRational& Rhs);
	static VRational& Div(FRunningContext, VRational& Lhs, VRational& Rhs);
	static VRational& Neg(FRunningContext, VRational& N);
	static bool Eq(FRunningContext, VRational& Lhs, VRational& Rhs);
	static bool Gt(FRunningContext, VRational& Lhs, VRational& Rhs);
	static bool Lt(FRunningContext, VRational& Lhs, VRational& Rhs);
	static bool Gte(FRunningContext, VRational& Lhs, VRational& Rhs);
	static bool Lte(FRunningContext, VRational& Lhs, VRational& Rhs);

	void Reduce(FRunningContext);
	void NormalizeSigns(FRunningContext Context);
	bool IsZero() const { return Numerator.Get().AsInt().IsZero(); }
	bool IsReduced() const { return bIsReduced; }

	static VRational& New(FAllocationContext Context, VInt InNumerator, VInt InDenominator)
	{
		return *new (Context.AllocateFastCell(sizeof(VRational))) VRational(Context, InNumerator, InDenominator);
	}

	static VRational& New(FAllocationContext Context, VValue InNumerator, VValue InDenominator)
	{
		return *new (Context.AllocateFastCell(sizeof(VRational))) VRational(Context, InNumerator, InDenominator);
	}

	COREUOBJECT_API static void MarkReferencedCellsImpl(VCell* This, FMarkStack&);

	COREUOBJECT_API static bool EqualImpl(FRunningContext Context, VCell* This, VCell* Other, TFunction<void(VValue, VValue)> HandlePlaceholder);

	COREUOBJECT_API static uint32 GetTypeHashImpl(VCell* This);

private:
	VRational(FAllocationContext Context, VValue InNumerator, VValue InDenominator)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, bIsReduced(false)
	{
		checkSlow(InDenominator.IsInt() && InNumerator.IsInt());
		checkSlow(!InDenominator.AsInt().IsZero());
		Numerator.Set(Context, InNumerator);
		Denominator.Set(Context, InDenominator);
	}

	VRational(FAllocationContext Context, VInt InNumerator, VInt InDenominator)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, bIsReduced(false)
	{
		checkSlow(!InDenominator.IsZero());
		Numerator.Set(Context, InNumerator);
		Denominator.Set(Context, InDenominator);
	}

	bool bIsReduced;
};

} // namespace Verse
