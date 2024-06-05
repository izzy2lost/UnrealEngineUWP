// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"

#if WITH_EDITOR

namespace UE::MIR
{

FInstructionPtr FValue::AsInstruction() const
{
	return (Kind >= VK_InstructionBegin && Kind < VK_InstructionEnd) ? static_cast<FInstructionPtr>(this) : nullptr;
}

bool FValue::Equals(FValuePtr Other) const
{
	// If kinds are different the two values are surely different.
	if (Kind != Other->Kind)
	{
		return false;
	}

	// Get the size of this value in bytes. It should match that of Other, since the value kinds are the same.
	uint32 SizeInBytes = GetSizeInBytes();
	check(SizeInBytes == Other->GetSizeInBytes());

	// Values are PODs by design, therefore simply comparing bytes is sufficient.
	return FMemory::Memcmp(this, Other, SizeInBytes) == 0;
}

uint32 FValue::GetSizeInBytes() const
{
	switch (Kind)
	{
		case VK_ScalarConstant: return sizeof(FScalarConstant);
		case VK_SetMaterialOutput: return sizeof(FScalarConstant);
		case VK_BinaryOperator: return sizeof(FBinaryOperator);
		case VK_Dimensional: return sizeof(FDimensional) + sizeof(FValuePtr) * static_cast<const FDimensional*>(this)->GetComponents().Num();
		default: UE_MIR_UNREACHABLE();
	}
}

TArrayView<const FValuePtr> FDimensional::GetComponents() const
{
	TArrayView<FValuePtr> Components = const_cast<FDimensional*>(this)->GetMutableComponents();
	return { Components.GetData(), Components.Num() };
}

TArrayView<FValuePtr> FDimensional::GetMutableComponents()
{
	FArithmeticTypePtr ArithmeticType = Type->ToArithmetic();
	check(ArithmeticType);

	FValuePtr* Ptr = (FValuePtr*)static_cast<TDimensional<1>*>(this)->Components;
	return { Ptr, ArithmeticType->NumRows };
}

} // namespace UE::MIR

#endif // #if WITH_EDITOR
