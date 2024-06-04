// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"

#if WITH_EDITOR

namespace MaterialIR
{

struct FValue;
using FValuePtr = const FValue*;

TArrayView<const FValuePtr> FVectorValue::GetComponents() const
{
	TArrayView<FValuePtr> Components = const_cast<FVectorValue*>(this)->GetMutableComponents();
	return { Components.GetData(), Components.Num() };
}

TArrayView<FValuePtr> FVectorValue::GetMutableComponents()
{
	FArithmeticTypePtr ArithmeticType = Type->ToArithmetic();
	check(ArithmeticType);

	FValuePtr* Ptr = (FValuePtr*)static_cast<TVectorValue<1>*>(this)->Components;
	return { Ptr, ArithmeticType->NumRows };
}

} // namespace MaterialIR

#endif // #if WITH_EDITOR
