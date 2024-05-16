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
	VK_Value = 1 << 0,
	VK_Scalar = VK_Value | 1 << 2,
	VK_Vector = VK_Value | 2 << 2,
	
	/* Instructions */
	VK_Instruction = 1 << 1,
 	VK_SetMaterialOutputInstr = VK_Instruction | 1 << 2,
};

/* Values */

struct FValue
{
	EValueKind Kind{};
	FTypePtr  Type{};

	bool IsA(EValueKind InKind) const
	{
		return (Kind & InKind) == InKind;
	}

	template <typename T>
	const T* Cast() const
	{
		return this && IsA(T::TypeKind) ? static_cast<const T*>(this) : nullptr;
	}

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
