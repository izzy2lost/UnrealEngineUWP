// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"

enum EMaterialProperty : int;

#if WITH_EDITOR

namespace MaterialIR
{

enum EValueKind
{
	/* Values */
	VK_Value = 0,
	VK_Scalar = 1 << 1,
	VK_Vector = 2 << 1,
	
	/* Instructions */
	VK_Instruction = 1,
 	VK_SetMaterialOutputInstr = VK_Instruction | 1 << 1,
};

/* Values */

struct FValue : FIRNode<EValueKind>
{
	EValueKind Kind = VK_Value;
	FTypePtr  Type{};

	void Destroy();
};

	template <EValueKind TTypeKind>
	struct TValue : FValue
	{
		static constexpr EValueKind TypeKind = TTypeKind;
	};

struct FScalarValue : TValue<VK_Scalar>
{
	union
	{
		bool  Boolean;
		int   Integer;
		float Float;
	};
};

struct FVectorValue : TValue<VK_Vector>
{
	TArrayView<const FValuePtr> GetComponents() const;
	TArrayView<FValuePtr> GetMutableComponents();
};

template <int TDimension>
struct TVectorValue : FVectorValue
{
	FValuePtr Components[TDimension];
};

/* Instructions */

struct FInstruction : TValue<VK_Instruction>
{
	FInstruction* Next{};
};

	template <EValueKind TTypeKind>
	struct TInstruction : FInstruction
	{
		static constexpr EValueKind TypeKind = TTypeKind;
	};

struct FSetMaterialOutputInstr : TInstruction<VK_SetMaterialOutputInstr>
{
	EMaterialProperty Property;
	FValuePtr ArgValue;
};

} // namespace MaterialIR

#endif
