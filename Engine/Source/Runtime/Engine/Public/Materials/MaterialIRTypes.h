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
	ETypeKind 	Kind;

	static FTypePtr FromShaderType(const UE::Shader::FType& InShaderType);
	static FTypePtr GetVoid();

	FStringView GetSpelling() const;
	FArithmeticTypePtr AsArithmetic() const;
	FArithmeticTypePtr AsScalar() const;
	FArithmeticTypePtr AsVector() const;
	FArithmeticTypePtr AsMatrix() const;
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

	static FArithmeticTypePtr GetBool();
	static FArithmeticTypePtr GetInt();
	static FArithmeticTypePtr GetFloat();
	static FArithmeticTypePtr GetScalar(EScalarKind InScalarKind);
	static FArithmeticTypePtr GetVector(EScalarKind InScalarKind, int NumRows);
	static FArithmeticTypePtr GetMatrix(EScalarKind InScalarKind, int NumColumns, int NumRows);
	static FArithmeticTypePtr Get(EScalarKind InScalarKind, int NumRows, int NumColumns);

	bool IsScalar() const { return NumRows == 1 && NumColumns == 1; }
	bool IsVector() const { return NumRows > 1 && NumColumns == 1; }
	bool IsMatrix() const { return NumRows > 1 && NumColumns > 1; }
};

} // namespace UE::MIR

#endif // #if WITH_EDITOR
