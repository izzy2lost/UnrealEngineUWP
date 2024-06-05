// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"

enum EMaterialProperty : int;

#if WITH_EDITOR

namespace UE::MIR {

enum EValueKind
{
	/* Values */
	VK_ScalarConstant,
	VK_Dimensional,

	/* Instructions */
	VK_InstructionBegin,

	VK_SetMaterialOutput = VK_InstructionBegin,
	VK_BinaryOperator,

	VK_InstructionEnd,
};

/* Values */

struct FValue
{
	EValueKind Kind{};
	FTypePtr  Type{};

	bool IsA(EValueKind InKind) const { return Kind == InKind; }
	FInstructionPtr AsInstruction() const;
	bool Equals(FValuePtr Other) const;
	uint32 GetSizeInBytes() const;

	template <typename T>
	const T* As() const { return this && IsA(T::TypeKind) ? static_cast<const T*>(this) : nullptr; }

};

template <EValueKind TTypeKind>
struct TValue : FValue
{
	static constexpr EValueKind TypeKind = TTypeKind;
};

struct FScalarConstant : TValue<VK_ScalarConstant>
{
	union
	{
		bool  Boolean;
		int   Integer;
		float Float;
	};
};

struct FDimensional : TValue<VK_Dimensional>
{
	TArrayView<const FValuePtr> GetComponents() const;
	TArrayView<FValuePtr> GetMutableComponents();
	uint32 GetSizeInBytes() const;
};

template <int TDimension>
struct TDimensional : FDimensional
{
	FValuePtr Components[TDimension];
};

/* Instructions */

struct FInstruction : FValue
{
	FInstruction* Next{};
};

template <EValueKind TTypeKind>
struct TInstruction : FInstruction
{
	static constexpr EValueKind TypeKind = TTypeKind;
};

struct FSetMaterialOutput : TInstruction<VK_SetMaterialOutput>
{
	EMaterialProperty Property;
	FValuePtr ArgValue;
};

enum EBinaryOperator
{
	BO_Invalid,
	BO_Add,
	BO_Subtract,
	BO_Multiply,
	BO_Divide,
};

struct FBinaryOperator : TInstruction<VK_BinaryOperator>
{
	EBinaryOperator Operator = BO_Invalid;
	FValuePtr Lhs{};
	FValuePtr Rhs{};
};

} // namespace UE::MIR
#endif
