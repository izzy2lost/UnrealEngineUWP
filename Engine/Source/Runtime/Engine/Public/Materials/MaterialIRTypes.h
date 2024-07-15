// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"
#include "Shader/ShaderTypes.h"

#if WITH_EDITOR

namespace UE::MIR
{

enum ETypeKind
{
	TK_Void,
	TK_Arithmetic
};

const TCHAR* TypeKindToString(ETypeKind Kind);

struct FType
{
	// Identifies what derived type this is.
	ETypeKind Kind;

	// Returns the type matching specified UE::Shader::FType.
	static FTypePtr FromShaderType(const UE::Shader::FType& InShaderType);
	
	// Returns the `void` type.
	static FTypePtr GetVoid();

	// Returns whether this type is a `bool` scalar.
	bool IsBool1() const;

	// Returns this type upcast this type to ArithmeticType if it is one. Otherwise it returns nullptr.
	FArithmeticTypePtr AsArithmetic() const;

	// Returns this type upcast this type to ArithmeticType if it's a scalar. Otherwise it returns nullptr.
	FArithmeticTypePtr AsScalar() const;

	// Returns this type upcast this type to ArithmeticType if it's a vector. Otherwise it returns nullptr.
	FArithmeticTypePtr AsVector() const;

	// Returns this type upcast this type to ArithmeticType if it's a matrix. Otherwise it returns nullptr.
	FArithmeticTypePtr AsMatrix() const;

	// Returns the this type name spelling (e.g. float4x4).
	FStringView GetSpelling() const;

	// Converts this type to a UE::Shader::EValueType.
	UE::Shader::EValueType ToValueType() const;
};

enum EScalarKind
{
	SK_Bool, SK_Int, SK_Float,
};

const TCHAR* ScalarKindToString(EScalarKind Kind);

struct FArithmeticType : FType
{
	FStringView Spelling;
	EScalarKind ScalarKind;
	int NumRows;
	int NumColumns;

	static FArithmeticTypePtr GetBool1();
	static FArithmeticTypePtr GetInt1();
	static FArithmeticTypePtr GetFloat1();

	static FArithmeticTypePtr GetScalar(EScalarKind InScalarKind);
	static FArithmeticTypePtr GetVector(EScalarKind InScalarKind, int NumRows);
	static FArithmeticTypePtr GetMatrix(EScalarKind InScalarKind, int NumColumns, int NumRows);
	static FArithmeticTypePtr Get(EScalarKind InScalarKind, int NumRows, int NumColumns);

	int  GetNumComponents() const { return NumRows * NumColumns; }
	bool IsScalar() const { return GetNumComponents() == 1; }
	bool IsVector() const { return NumRows > 1 && NumColumns == 1; }
	bool IsMatrix() const { return NumRows > 1 && NumColumns > 1; }
	FArithmeticTypePtr ToScalar() const;
};

} // namespace UE::MIR

#endif // #if WITH_EDITOR
