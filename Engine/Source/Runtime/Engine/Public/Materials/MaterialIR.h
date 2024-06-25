// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"

enum EMaterialProperty : int;

#if WITH_EDITOR

namespace UE::MIR {

enum EValueKind
{
	/* Values */

	VK_Constant,

	/* Instructions */

	VK_Dimensional,
	VK_SetMaterialOutput,
	VK_BinaryOperator,
	VK_Branch,
	VK_Subscript,
	VK_Cast,

	VK_InstructionEnd,
	VK_InstructionBegin = VK_Dimensional,
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
	T* As() { return this && IsA(T::TypeKind) ? static_cast<T*>(this) : nullptr; }

	template <typename T>
	const T* As() const { return this && IsA(T::TypeKind) ? static_cast<const T*>(this) : nullptr; }
};

template <EValueKind TTypeKind>
struct TValue : FValue
{
	static constexpr EValueKind TypeKind = TTypeKind;
};

using TInteger = int64_t;
using TFloat = double;

struct FConstant : TValue<VK_Constant>
{
	union
	{
		bool  		Boolean;
		TInteger	Integer;
		TFloat 	Float;
	};

	template <typename T>
	T Get() const
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return Boolean;
		}
		else if constexpr (std::is_integral_v<T>)
		{
			return Integer;
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			return Float;
		}
		else
		{
			check(false && "unexpected type T.");
		}
	}
};

/* Instructions */

struct FBlock
{
	FBlock* Parent{};
	FInstruction* Instructions{};
	int32 Level{};
};

enum EInstructionFlags
{
	IF_None = 0,
	IF_Counted = 1,
};

struct FInstruction : FValue
{
	EInstructionFlags Flags{};
	FInstruction* Next{};
	FBlock* Block{};
	uint32 NumUsers{};
	uint32 NumProcessedUsers{};

	void SetFlags(EInstructionFlags InFlags) { Flags = (EInstructionFlags)(Flags | InFlags); }
	bool GetInnerBlock(int32 Index, FValue*& OutArg, FBlock*& OutBlock); 
};

template <EValueKind TTypeKind>
struct TInstruction : FInstruction
{
	static constexpr EValueKind TypeKind = TTypeKind;
};

struct FDimensional : TInstruction<VK_Dimensional>
{
	// Returns the constant array of component values. 
	TArrayView<FValue* const> GetComponents() const;

	// Returns the mutable array of component values. 
	TArrayView<FValue*> GetComponents();

	// Returns whether all components are constant.
	bool AreComponentsConstant() const;

};

template <int TDimension>
struct TDimensional : FDimensional
{
	FValue* Components[TDimension];
};

struct FSetMaterialOutput : TInstruction<VK_SetMaterialOutput>
{
	EMaterialProperty Property;
	FValue* Arg;
};

enum EBinaryOperator
{
	BO_Invalid,

	/* Arithmetic */
	BO_Add,
	BO_Subtract,
	BO_Multiply,
	BO_Divide,

	/* Comparison */
	BO_Greater,
	BO_GreaterOrEquals,
	BO_Lower,
	BO_LowerOrEquals,
	BO_Equals,
	BO_NotEquals,
};

bool IsArithmeticOperator(EBinaryOperator Op);
bool IsComparisonOperator(EBinaryOperator Op);

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

struct FSubscript : TInstruction<VK_Subscript>
{
	FValue* Arg;
	int Index;
};

struct FCast : TInstruction<VK_Cast>
{
	FValue* Arg{};
};

} // namespace UE::MIR
#endif
