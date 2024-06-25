// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"

#if WITH_EDITOR

namespace UE::MIR
{

FInstruction* FValue::AsInstruction()
{
	return this && (Kind >= VK_InstructionBegin && Kind < VK_InstructionEnd) ? static_cast<FInstruction*>(this) : nullptr;
}

const FInstruction* FValue::AsInstruction() const
{
	return const_cast<FValue*>(this)->AsInstruction();
}

bool FValue::Equals(const FValue* Other) const
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
		case VK_Constant: return sizeof(FConstant);
		case VK_SetMaterialOutput: return sizeof(FSetMaterialOutput);
		case VK_BinaryOperator: return sizeof(FBinaryOperator);
		case VK_Dimensional: return sizeof(FDimensional) + sizeof(FValue*) * static_cast<const FDimensional*>(this)->GetComponents().Num();
		case VK_Branch: return sizeof(FBranch);
		default: UE_MIR_UNREACHABLE();
	}
}

TArrayView<FValue*> FValue::GetUses()
{
	switch (Kind)
	{
		case VK_Constant:
		case VK_Dimensional:
			return {};

		case VK_SetMaterialOutput:
		{
			auto This = static_cast<FSetMaterialOutput*>(this);
			return { &This->Arg, 1 };
		}

		case VK_BinaryOperator:
		{
			auto This = static_cast<FBinaryOperator*>(this);
			return { &This->LhsArg, 2 };
		}

		case VK_Branch:
		{
			auto This = static_cast<FBranch*>(this);
			return { &This->ConditionArg, 3 };
		}

		default: UE_MIR_UNREACHABLE();
	}
}

TArrayView<FValue* const> FDimensional::GetComponents() const
{
	TArrayView<FValue*> Components = const_cast<FDimensional*>(this)->GetComponents();
	return { Components.GetData(), Components.Num() };
}

TArrayView<FValue*> FDimensional::GetComponents()
{
	FArithmeticTypePtr ArithmeticType = Type->AsArithmetic();
	check(ArithmeticType);

	FValue** Ptr = (FValue**)static_cast<TDimensional<1>*>(this)->Components;
	return { Ptr, ArithmeticType->NumRows };
}

bool FDimensional::AreComponentsConstant() const
{
	for (FValue const* Component : GetComponents())
	{
		if (!Component->As<FConstant>())
		{
			return false;
		}
	}
	return true;
}

bool FInstruction::GetInnerBlock(int32 Index, FValue*& OutArg, FBlock*& OutBlock)
{
	switch (Kind)
	{
		case VK_Branch:
		{
			auto This = static_cast<FBranch*>(this);
			if (Index == 0)
			{
				OutArg = This->TrueArg;
				OutBlock = &This->TrueBlock;
				return true;
			}
			else if (Index == 1)
			{
				OutArg = This->FalseArg;
				OutBlock = &This->FalseBlock;
				return true;
			}
			break;
		}

		default:
		{
			TArrayView<FValue*> Uses = GetUses();
			if (Uses.IsValidIndex(Index))
			{
				OutArg = Uses[Index];
				OutBlock = Block;
				return true;
			}
			break;
		}
	}

	return false;
}

bool IsArithmeticOperator(EBinaryOperator Op)
{
	return Op >= BO_Add && Op <= BO_Divide;
}

bool IsComparisonOperator(EBinaryOperator Op)
{
	return Op >= BO_Greater && Op <= BO_NotEquals;
}

} // namespace UE::MIR

#endif // #if WITH_EDITOR
