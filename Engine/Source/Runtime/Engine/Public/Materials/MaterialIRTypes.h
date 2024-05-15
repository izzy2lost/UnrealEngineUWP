// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"
#include "Shader/ShaderTypes.h"

#if WITH_EDITOR

namespace MaterialIR
{

enum ETypeKind
{
	TK_Void = 1,
	TK_Arithmetic
};

struct FType
{
	ETypeKind Kind;

	static FTypePtr FromShaderType(const UE::Shader::FType& InShaderType);
	static FTypePtr GetVoid();

	const FArithmeticType* ToArithmetic() const;
	bool IsScalar() const;

};

enum EScalarKind
{
	SK_Bool, SK_Int, SK_Float,
};

const TCHAR* ScalarKindToString(EScalarKind Kind);

struct FArithmeticType : FType
{
	EScalarKind ScalarKind;
	int NumRows;
	int NumColumns;

	static const FArithmeticType* GetScalar(EScalarKind InScalarKind);
	static const FArithmeticType* GetVector(EScalarKind InScalarKind, int NumRows);
	static const FArithmeticType* GetMatrix(EScalarKind InScalarKind, int NumColumns, int NumRows);

	bool IsScalar() const { return NumRows == 1 && NumColumns == 1; }
	bool IsVector() const { return NumRows > 1 && NumColumns == 1; }
	bool IsMatrix() const { return NumRows > 1 && NumColumns > 1; }
};

} // namespace MaterialIR

#endif
