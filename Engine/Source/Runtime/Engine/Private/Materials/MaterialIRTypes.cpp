// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRTypes.h"

#if WITH_EDITOR

namespace UE::MIR
{

const TCHAR* TypeKindToString(ETypeKind Kind)
{
	switch (Kind)
	{
		case TK_Void: return TEXT("void");
		case TK_Arithmetic: return TEXT("arithmetic");
		default: UE_MIR_UNREACHABLE();
	}
}

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

FStringView FType::GetSpelling() const
{
	if (auto Arithmetic = AsArithmetic())
	{
		return Arithmetic->Spelling;
	}
	
	UE_MIR_UNREACHABLE();
}

UE::Shader::EValueType FType::ToValueType() const
{
	using namespace UE::Shader;

	if (FArithmeticTypePtr ArithmeticType = AsArithmetic())
	{
		if (ArithmeticType->IsMatrix())
		{
			if (ArithmeticType->NumRows == 4 && ArithmeticType->NumColumns == 4)
			{
				if (ArithmeticType->ScalarKind == SK_Float)
				{
					return EValueType::Float4x4;
				}
				else
				{
					return EValueType::Numeric4x4;
				}
			}

			return EValueType::Any;
		}

		check(ArithmeticType->NumColumns == 1 && ArithmeticType->NumRows <= 4);

		switch (ArithmeticType->ScalarKind)
		{
			case SK_Bool: 	return (EValueType)((int)EValueType::Bool1 + ArithmeticType->NumRows - 1); break;
			case SK_Int: 	return (EValueType)((int)EValueType::Int1 + ArithmeticType->NumRows - 1); break;
			case SK_Float: 	return (EValueType)((int)EValueType::Float1 + ArithmeticType->NumRows - 1); break;
			default: UE_MIR_UNREACHABLE();
		}
	}
	
	UE_MIR_UNREACHABLE();
}

bool FType::IsBool1() const
{
	return this == FArithmeticType::GetBool1();
}

FArithmeticTypePtr FType::AsArithmetic() const
{
	return Kind == TK_Arithmetic ? static_cast<FArithmeticTypePtr>(this) : nullptr; 
}

FArithmeticTypePtr FType::AsScalar() const
{
	FArithmeticTypePtr Type = AsArithmetic();
	return Type->IsScalar() ? Type : nullptr;
}

FArithmeticTypePtr FType::AsVector() const
{
	FArithmeticTypePtr Type = AsArithmetic();
	return Type->IsVector() ? Type : nullptr;
}

FArithmeticTypePtr FType::AsMatrix() const
{
	FArithmeticTypePtr Type = AsArithmetic();
	return Type->IsMatrix() ? Type : nullptr;
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

FArithmeticTypePtr FArithmeticType::GetBool1()
{
	return GetScalar(SK_Bool);
}

FArithmeticTypePtr FArithmeticType::GetInt1()
{
	return GetScalar(SK_Int);
}

FArithmeticTypePtr FArithmeticType::GetFloat1()
{
	return GetScalar(SK_Float);
}

const FArithmeticType* FArithmeticType::GetScalar(EScalarKind InScalarKind)
{
	return Get(InScalarKind, 1, 1);
}

const FArithmeticType* FArithmeticType::GetVector(EScalarKind InScalarKind, int NumComponents)
{
	check(NumComponents >= 1 && NumComponents <= 4);
	return Get(InScalarKind, NumComponents, 1);
}

const FArithmeticType* FArithmeticType::GetMatrix(EScalarKind InScalarKind, int NumRows, int NumColumns)
{
	check(NumColumns > 1 && NumColumns <= 4);
	check(NumRows > 1 && NumRows <= 4);
	return Get(InScalarKind, NumRows, NumColumns);
}

FArithmeticTypePtr FArithmeticType::Get(EScalarKind InScalarKind, int NumRows, int NumColumns)
{
	check(InScalarKind >= 0 && InScalarKind <= SK_Float);
	
	static const FStringView Invalid = TEXT("invalid");

	static const FArithmeticType Types[] {
		{ { TK_Arithmetic }, { TEXT("bool") }, 		SK_Bool, 1, 1 },
		{ { TK_Arithmetic }, Invalid, 				SK_Bool, 1, 2 }, 
		{ { TK_Arithmetic }, Invalid, 				SK_Bool, 1, 3 },
		{ { TK_Arithmetic }, Invalid, 				SK_Bool, 1, 4 },
		{ { TK_Arithmetic }, { TEXT("bool2") },   	SK_Bool, 2, 1 },
		{ { TK_Arithmetic }, { TEXT("bool2x2") }, 	SK_Bool, 2, 2 },
		{ { TK_Arithmetic }, { TEXT("bool2x3") }, 	SK_Bool, 2, 3 },
		{ { TK_Arithmetic }, { TEXT("bool2x4") }, 	SK_Bool, 2, 4 },
		{ { TK_Arithmetic }, { TEXT("bool3") },   	SK_Bool, 3, 1 },
		{ { TK_Arithmetic }, { TEXT("bool3x2") }, 	SK_Bool, 3, 2 },
		{ { TK_Arithmetic }, { TEXT("bool3x3") }, 	SK_Bool, 3, 3 },
		{ { TK_Arithmetic }, { TEXT("bool3x4") }, 	SK_Bool, 3, 4 },
		{ { TK_Arithmetic }, { TEXT("bool4") },   	SK_Bool, 4, 1 },
		{ { TK_Arithmetic }, { TEXT("bool4x2") }, 	SK_Bool, 4, 2 },
		{ { TK_Arithmetic }, { TEXT("bool4x3") }, 	SK_Bool, 4, 3 },
		{ { TK_Arithmetic }, { TEXT("bool4x4") }, 	SK_Bool, 4, 4 },
		{ { TK_Arithmetic }, { TEXT("int") }, 		SK_Int, 1, 1 },
		{ { TK_Arithmetic }, Invalid, 				SK_Int, 1, 2 },
		{ { TK_Arithmetic }, Invalid, 				SK_Int, 1, 3 },
		{ { TK_Arithmetic }, Invalid, 				SK_Int, 1, 4 },
		{ { TK_Arithmetic }, { TEXT("int2") },   	SK_Int, 2, 1 },
		{ { TK_Arithmetic }, { TEXT("int2x2") }, 	SK_Int, 2, 2 },
		{ { TK_Arithmetic }, { TEXT("int2x3") }, 	SK_Int, 2, 3 },
		{ { TK_Arithmetic }, { TEXT("int2x4") }, 	SK_Int, 2, 4 },
		{ { TK_Arithmetic }, { TEXT("int3") },   	SK_Int, 3, 1 },
		{ { TK_Arithmetic }, { TEXT("int3x2") }, 	SK_Int, 3, 2 },
		{ { TK_Arithmetic }, { TEXT("int3x3") }, 	SK_Int, 3, 3 },
		{ { TK_Arithmetic }, { TEXT("int3x4") }, 	SK_Int, 3, 4 },
		{ { TK_Arithmetic }, { TEXT("int4") },   	SK_Int, 4, 1 },
		{ { TK_Arithmetic }, { TEXT("int4x2") }, 	SK_Int, 4, 2 },
		{ { TK_Arithmetic }, { TEXT("int4x3") }, 	SK_Int, 4, 3 },
		{ { TK_Arithmetic }, { TEXT("int4x4") }, 	SK_Int, 4, 4 },
		{ { TK_Arithmetic }, { TEXT("float") }, 	SK_Float, 1, 1 },
		{ { TK_Arithmetic }, Invalid, 				SK_Float, 1, 2 },
		{ { TK_Arithmetic }, Invalid, 				SK_Float, 1, 3 },
		{ { TK_Arithmetic }, Invalid, 				SK_Float, 1, 4 },
		{ { TK_Arithmetic }, { TEXT("float2") },   	SK_Float, 2, 1 },
		{ { TK_Arithmetic }, { TEXT("float2x2") }, 	SK_Float, 2, 2 },
		{ { TK_Arithmetic }, { TEXT("float2x3") }, 	SK_Float, 2, 3 },
		{ { TK_Arithmetic }, { TEXT("float2x4") }, 	SK_Float, 2, 4 },
		{ { TK_Arithmetic }, { TEXT("float3") },   	SK_Float, 3, 1 },
		{ { TK_Arithmetic }, { TEXT("float3x2") }, 	SK_Float, 3, 2 },
		{ { TK_Arithmetic }, { TEXT("float3x3") }, 	SK_Float, 3, 3 },
		{ { TK_Arithmetic }, { TEXT("float3x4") }, 	SK_Float, 3, 4 },
		{ { TK_Arithmetic }, { TEXT("float4") },   	SK_Float, 4, 1 },
		{ { TK_Arithmetic }, { TEXT("float4x2") }, 	SK_Float, 4, 2 },
		{ { TK_Arithmetic }, { TEXT("float4x3") }, 	SK_Float, 4, 3 },
		{ { TK_Arithmetic }, { TEXT("float4x4") }, 	SK_Float, 4, 4 },
	};

	int Index = InScalarKind * 4 * 4 + (NumRows - 1) * 4 + (NumColumns - 1);
	check(Index < UE_ARRAY_COUNT(Types));
	return &Types[Index];
}

FArithmeticTypePtr FArithmeticType::ToScalar() const
{
	return FArithmeticType::GetScalar(ScalarKind);
}

} // namespace UE::MIR

#endif // #if WITH_EDITOR
