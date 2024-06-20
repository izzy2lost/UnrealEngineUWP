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
	VK_Branch,

	VK_InstructionEnd,
};

/* Values */

struct FValue
{
	EValueKind Kind{};
	FTypePtr  Type{};

	bool IsA(EValueKind InKind) const { return Kind == InKind; }
	FInstruction* AsInstruction();
	const FInstruction* AsInstruction() const;
	bool Equals(const FValue* Other) const;
	uint32 GetSizeInBytes() const;
	TArrayView<FValue*> GetUses();

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
	TArrayView<FValue* const> GetComponents() const;
	TArrayView<FValue*> GetMutableComponents();
};

template <int TDimension>
struct TDimensional : FDimensional
{
	FValue* Components[TDimension];
};

/* Instructions */

enum EInstructionFlags
{
	IF_None = 0,
	IF_Counted = 1,
};

struct FInstruction : FValue
{
	EInstructionFlags Flags = IF_None;
	FInstruction* Next{};
	FBlock* Block{};
	uint32 NumUsers{};
	uint32 NumProcessedUsers{};

	void SetFlags(EInstructionFlags InFlags) { Flags = (EInstructionFlags)(Flags | InFlags); }
	bool GetInnerBlock(int32 Index, FValue*& OutArg, FBlock*& OutBlock); 
};

struct FBlock
{
	FBlock* Parent{};
	FInstruction* Instructions{};
	int32 Level{};
};

template <EValueKind TTypeKind>
struct TInstruction : FInstruction
{
	static constexpr EValueKind TypeKind = TTypeKind;
};

struct FSetMaterialOutput : TInstruction<VK_SetMaterialOutput>
{
	EMaterialProperty Property;
	FValue* Arg;
};

enum EBinaryOperator
{
	BO_Invalid,
	BO_Add,
	BO_Subtract,
	BO_Multiply,
	BO_Divide,
	BO_Greater,
	BO_Lower,
	BO_Equals,
};

struct FBinaryOperator : TInstruction<VK_BinaryOperator>
{
	EBinaryOperator Operator = BO_Invalid;
	FValue* LhsArg{};
	FValue* RhsArg{};
};

struct FBranch : TInstruction<VK_Branch>
{
	FValue* ConditionArg{};
	FValue* TrueArg{};
	FValue* FalseArg{};
	FBlock TrueBlock{};
	FBlock FalseBlock{};
};

} // namespace UE::MIR
#endif
