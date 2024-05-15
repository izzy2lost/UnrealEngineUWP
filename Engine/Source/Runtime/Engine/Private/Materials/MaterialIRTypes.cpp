// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRTypes.h"

#if WITH_EDITOR

namespace MaterialIR
{

FTypePtr FType::FromShaderType(const UE::Shader::FType& InShaderType)
{
	check(!InShaderType.IsStruct());
	check(!InShaderType.IsObject());

	switch (InShaderType.ValueType)
	{
		case UE::Shader::EValueType::Void:
			return FType::GetVoid();

		case UE::Shader::EValueType::Float1:
		case UE::Shader::EValueType::Float2:
		case UE::Shader::EValueType::Float3:
		case UE::Shader::EValueType::Float4:
			return FArithmeticType::GetVector(SK_Float, (int)InShaderType.ValueType - (int)UE::Shader::EValueType::Float1 + 1);

		case UE::Shader::EValueType::Int1:
		case UE::Shader::EValueType::Int2:
		case UE::Shader::EValueType::Int3:
		case UE::Shader::EValueType::Int4:
			return FArithmeticType::GetVector(SK_Int, (int)InShaderType.ValueType - (int)UE::Shader::EValueType::Int1 + 1);

		case UE::Shader::EValueType::Bool1:
		case UE::Shader::EValueType::Bool2:
		case UE::Shader::EValueType::Bool3:
		case UE::Shader::EValueType::Bool4:
			return FArithmeticType::GetVector(SK_Bool, (int)InShaderType.ValueType - (int)UE::Shader::EValueType::Int1 + 1);

		default:
			UE_MIR_UNREACHABLE();
	}
}

FTypePtr FType::GetVoid()
{
	static FType Type{ TK_Void };
	return &Type;
}

const FArithmeticType* FType::ToArithmetic() const
{
	return Kind == TK_Arithmetic ? static_cast<const FArithmeticType*>(this) : nullptr; 
}

bool FType::IsScalar() const
{
	if (FArithmeticTypePtr Arith = ToArithmetic())
	{
		return Arith->IsScalar();
	}
	return false;
}

const TCHAR* ScalarKindToString(EScalarKind Kind)
{
	switch (Kind)
	{
		case SK_Bool: return TEXT("bool"); break;
		case SK_Int: return TEXT("int"); break;
		case SK_Float: return TEXT("float"); break;
		default: UE_MIR_UNREACHABLE();
	}
}

static const FArithmeticType* GetNumericalType(EScalarKind InScalarKind, int NumRows, int NumColumns)
{
	check(InScalarKind >= 0 && InScalarKind <= SK_Float);
	
	static const FArithmeticType Types[] {
		{ { TK_Arithmetic }, SK_Bool, 1, 1 },
		{ { TK_Arithmetic }, SK_Bool, 1, 2 }, 
		{ { TK_Arithmetic }, SK_Bool, 1, 3 },
		{ { TK_Arithmetic }, SK_Bool, 1, 4 },
		{ { TK_Arithmetic }, SK_Bool, 2, 1 },
		{ { TK_Arithmetic }, SK_Bool, 2, 2 },
		{ { TK_Arithmetic }, SK_Bool, 2, 3 },
		{ { TK_Arithmetic }, SK_Bool, 2, 4 },
		{ { TK_Arithmetic }, SK_Bool, 3, 1 },
		{ { TK_Arithmetic }, SK_Bool, 3, 2 },
		{ { TK_Arithmetic }, SK_Bool, 3, 3 },
		{ { TK_Arithmetic }, SK_Bool, 3, 4 },
		{ { TK_Arithmetic }, SK_Bool, 4, 1 },
		{ { TK_Arithmetic }, SK_Bool, 4, 2 },
		{ { TK_Arithmetic }, SK_Bool, 4, 3 },
		{ { TK_Arithmetic }, SK_Bool, 4, 4 },
		{ { TK_Arithmetic }, SK_Int, 1, 1 },
		{ { TK_Arithmetic }, SK_Int, 1, 2 },
		{ { TK_Arithmetic }, SK_Int, 1, 3 },
		{ { TK_Arithmetic }, SK_Int, 1, 4 },
		{ { TK_Arithmetic }, SK_Int, 2, 1 },
		{ { TK_Arithmetic }, SK_Int, 2, 2 },
		{ { TK_Arithmetic }, SK_Int, 2, 3 },
		{ { TK_Arithmetic }, SK_Int, 2, 4 },
		{ { TK_Arithmetic }, SK_Int, 3, 1 },
		{ { TK_Arithmetic }, SK_Int, 3, 2 },
		{ { TK_Arithmetic }, SK_Int, 3, 3 },
		{ { TK_Arithmetic }, SK_Int, 3, 4 },
		{ { TK_Arithmetic }, SK_Int, 4, 1 },
		{ { TK_Arithmetic }, SK_Int, 4, 2 },
		{ { TK_Arithmetic }, SK_Int, 4, 3 },
		{ { TK_Arithmetic }, SK_Int, 4, 4 },
		{ { TK_Arithmetic }, SK_Float, 1, 1 },
		{ { TK_Arithmetic }, SK_Float, 1, 2 },
		{ { TK_Arithmetic }, SK_Float, 1, 3 },
		{ { TK_Arithmetic }, SK_Float, 1, 4 },
		{ { TK_Arithmetic }, SK_Float, 2, 1 },
		{ { TK_Arithmetic }, SK_Float, 2, 2 },
		{ { TK_Arithmetic }, SK_Float, 2, 3 },
		{ { TK_Arithmetic }, SK_Float, 2, 4 },
		{ { TK_Arithmetic }, SK_Float, 3, 1 },
		{ { TK_Arithmetic }, SK_Float, 3, 2 },
		{ { TK_Arithmetic }, SK_Float, 3, 3 },
		{ { TK_Arithmetic }, SK_Float, 3, 4 },
		{ { TK_Arithmetic }, SK_Float, 4, 1 },
		{ { TK_Arithmetic }, SK_Float, 4, 2 },
		{ { TK_Arithmetic }, SK_Float, 4, 3 },
		{ { TK_Arithmetic }, SK_Float, 4, 4 },
	};

	int Index = InScalarKind * 4 * 4 + (NumRows - 1) * 4 + (NumColumns - 1);
	check(Index < UE_ARRAY_COUNT(Types));
	return &Types[Index];
}

const FArithmeticType* FArithmeticType::GetScalar(EScalarKind InScalarKind)
{
	return GetNumericalType(InScalarKind, 1, 1);
}

const FArithmeticType* FArithmeticType::GetVector(EScalarKind InScalarKind, int NumComponents)
{
	check(NumComponents >= 1 && NumComponents <= 4);
	return GetNumericalType(InScalarKind, NumComponents, 1);
}

const FArithmeticType* FArithmeticType::GetMatrix(EScalarKind InScalarKind, int NumRows, int NumColumns)
{
	check(NumColumns > 1 && NumColumns <= 4);
	check(NumRows > 1 && NumRows <= 4);
	return GetNumericalType(InScalarKind, NumRows, NumColumns);
}

} // namespace MaterialIR

#endif // #if WITH_EDITOR
