// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMRational.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VRational, VHeapValue, TEXT("Rational"));
TGlobalTrivialEmergentTypePtr<&VRational::StaticCppClassInfo> VRational::GlobalTrivialEmergentType;

VRational& VRational::Add(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VRational::New(Context,
			VInt::Add(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
			Lhs.Denominator.Get().AsInt());
	}

	return VRational::New(
		Context,
		VInt::Add(Context,
			VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
			VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt())),
		VInt::Mul(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()));
}

VRational& VRational::Sub(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VRational::New(Context,
			VInt::Sub(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
			Lhs.Denominator.Get().AsInt());
	}

	return VRational::New(
		Context,
		VInt::Sub(Context,
			VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
			VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt())),
		VInt::Mul(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()));
}

VRational& VRational::Mul(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	return VRational::New(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt()),
		VInt::Mul(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()));
}

VRational& VRational::Div(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	return VRational::New(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Lhs.Denominator.Get().AsInt(), Rhs.Numerator.Get().AsInt()));
}

VRational& VRational::Neg(FRunningContext Context, VRational& N)
{
	return VRational::New(Context, VInt::Neg(Context, N.Numerator.Get().AsInt()), N.Denominator.Get().AsInt());
}

bool VRational::Eq(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	Lhs.Reduce(Context);
	Lhs.NormalizeSigns(Context);
	Rhs.Reduce(Context);
	Rhs.NormalizeSigns(Context);

	return VInt::Eq(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt())
		&& VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt());
}

bool VRational::Gt(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VInt::Gt(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt());
	}

	return VInt::Gt(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt()));
}

bool VRational::Lt(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VInt::Lt(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt());
	}

	return VInt::Lt(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt()));
}

bool VRational::Gte(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VInt::Gte(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt());
	}

	return VInt::Gte(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt()));
}

bool VRational::Lte(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VInt::Lte(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt());
	}

	return VInt::Lte(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt()));
}

void VRational::Reduce(FRunningContext Context)
{
	if (bIsReduced)
	{
		return;
	}

	VInt A = Numerator.Get().AsInt();
	VInt B = Denominator.Get().AsInt();
	while (!VInt::Eq(Context, B, VInt(0)))
	{
		VInt Remainder = VInt::Mod(Context, A, B);
		A = B;
		B = Remainder;
	}

	Numerator.Set(Context, VInt::Div(Context, Numerator.Get().AsInt(), A));
	Denominator.Set(Context, VInt::Div(Context, Denominator.Get().AsInt(), A));
	bIsReduced = true;
}

void VRational::NormalizeSigns(FRunningContext Context)
{
	VInt Denom = Denominator.Get().AsInt();
	if (VInt::Lt(Context, Denom, VInt(0)))
	{
		// The denominator is < 0, so we need to normalize the signs
		Numerator.Set(Context, VInt::Neg(Context, Numerator.Get().AsInt()));
		Denominator.Set(Context, VInt::Neg(Context, Denom));
	}
}

void VRational::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VRational& This = ThisCell->StaticCast<VRational>();
	VHeapValue::MarkReferencedCellsImpl(&This, MarkStack);
	This.Numerator.Mark(MarkStack);
	This.Denominator.Mark(MarkStack);
}

bool VRational::EqualImpl(FRunningContext Context, VCell* ThisCell, VCell* Other, TFunction<void(VValue, VValue)> HandlePlaceholder)
{
	if (!Other->IsA<VRational>())
	{
		return false;
	}
	return Eq(Context, ThisCell->StaticCast<VRational>(), Other->StaticCast<VRational>());
}

uint32 VRational::GetTypeHashImpl(VCell* This)
{
	VRational& ThisRational = This->StaticCast<VRational>();
	if (!ThisRational.bIsReduced)
	{
		// TLS lookup to reduce rationals before hashing
		// FRunningContextPromise PromiseContext;
		FRunningContext Context((FRunningContextPromise()));
		ThisRational.Reduce(Context);
		ThisRational.NormalizeSigns(Context);
	}
	return ::HashCombineFast(GetTypeHash(ThisRational.Numerator.Get()), GetTypeHash(ThisRational.Denominator.Get()));
}

} // namespace Verse
#endif // WITH_VERSE_VM