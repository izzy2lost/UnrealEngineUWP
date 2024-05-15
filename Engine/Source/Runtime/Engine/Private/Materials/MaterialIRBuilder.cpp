// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRBuilder.h"
#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"
#include "Shader/ShaderTypes.h"

#if WITH_EDITOR

namespace MaterialIR
{

struct FBuilder::FPrivate : FBuilder
{
	template <typename T>
	T* New(FTypePtr InType)
	{
		T* Val = new T;
		Val->Kind = T::TypeKind;
		Val->Type = InType;
		Module->Values.Add(Val);
		return Val;
	}
};

FBuilder::FBuilder()
{
}

FBuilder::FBuilder(FMaterialIRModule* InModule)
{
	SetTargetModule(InModule);
}

void FBuilder::SetTargetModule(FMaterialIRModule* InModule)
{
	Module = InModule;
}

FValuePtr FBuilder::NewConstantFromShaderValue(const UE::Shader::FValue& InValue)
{
	using namespace UE::Shader;

	switch (InValue.Type.ValueType)
	{
	case UE::Shader::EValueType::Float1: return NewConstantFloat1(InValue.AsFloatScalar());
	case UE::Shader::EValueType::Float2: return NewConstantFloat2(FVector2f{ InValue.Component[0].Float, InValue.Component[1].Float });
	case UE::Shader::EValueType::Float3: return NewConstantFloat3(FVector3f{ InValue.Component[0].Float, InValue.Component[1].Float, InValue.Component[2].Float });
	case UE::Shader::EValueType::Float4: return NewConstantFloat4(FVector4f{ InValue.Component[0].Float, InValue.Component[1].Float, InValue.Component[2].Float, InValue.Component[3].Float });

	case UE::Shader::EValueType::Int1: return NewConstantInt1(InValue.AsFloatScalar());
	case UE::Shader::EValueType::Int2: return NewConstantInt2(FIntVector2{ InValue.Component[0].Int, InValue.Component[1].Int });
	case UE::Shader::EValueType::Int3: return NewConstantInt3(FIntVector3{ InValue.Component[0].Int, InValue.Component[1].Int, InValue.Component[2].Int });
	case UE::Shader::EValueType::Int4: return NewConstantInt4(FIntVector4{ InValue.Component[0].Int, InValue.Component[1].Int, InValue.Component[2].Int, InValue.Component[3].Int });
	}

	UE_MIR_UNREACHABLE();
}

FValuePtr FBuilder::NewConstantFloat1(float InX)
{
	FScalarValue* S = AsPrivate()->New<FScalarValue>(FArithmeticType::GetScalar(SK_Float));
	S->Float = InX;
	return S;
}

FValuePtr FBuilder::NewConstantFloat2(const FVector2f& InValue)
{
	FValuePtr X = NewConstantFloat1(InValue.X);
	FValuePtr Y = NewConstantFloat1(InValue.Y);
	return NewVector2(X, Y);
}

FValuePtr FBuilder::NewConstantFloat3(const FVector3f& InValue)
{
	FValuePtr X = NewConstantFloat1(InValue.X);
	FValuePtr Y = NewConstantFloat1(InValue.Y);
	FValuePtr Z = NewConstantFloat1(InValue.Z);
	return NewVector3(X, Y, Z);
}

FValuePtr FBuilder::NewConstantFloat4(const FVector4f& InValue)
{
	FValuePtr X = NewConstantFloat1(InValue.X);
	FValuePtr Y = NewConstantFloat1(InValue.Y);
	FValuePtr Z = NewConstantFloat1(InValue.Z);
	FValuePtr W = NewConstantFloat1(InValue.W);
	return NewVector4(X, Y, Z, W);
}

FValuePtr FBuilder::NewConstantInt1(int InX)
{
	FScalarValue* S = AsPrivate()->New<FScalarValue>(FArithmeticType::GetScalar(SK_Int));
	S->Integer = InX;
	return S;
}

FValuePtr FBuilder::NewConstantInt2(const FIntVector2& InValue)
{
	FValuePtr X = NewConstantInt1(InValue.X);
	FValuePtr Y = NewConstantInt1(InValue.Y);
	return NewVector2(X, Y);
}

FValuePtr FBuilder::NewConstantInt3(const FIntVector3& InValue)
{
	FValuePtr X = NewConstantInt1(InValue.X);
	FValuePtr Y = NewConstantInt1(InValue.Y);
	FValuePtr Z = NewConstantInt1(InValue.Z);
	return NewVector3(X, Y, Z);
}

FValuePtr FBuilder::NewConstantInt4(const FIntVector4& InValue)
{
	FValuePtr X = NewConstantInt1(InValue.X);
	FValuePtr Y = NewConstantInt1(InValue.Y);
	FValuePtr Z = NewConstantInt1(InValue.Z);
	FValuePtr W = NewConstantInt1(InValue.W);
	return NewVector4(X, Y, Z, W);
}

FValuePtr FBuilder::NewVector2(FValuePtr InX, FValuePtr InY)
{
	check(InX->Type->IsScalar());
	check(InX->Type == InY->Type);

	FVectorValue* V = AsPrivate()->New<TVectorValue<2>>(FArithmeticType::GetVector(InX->Type->ToArithmetic()->ScalarKind, 2));
	TArrayView<FValuePtr> Components = V->GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;

	return V;
}

FValuePtr FBuilder::NewVector3(FValuePtr InX, FValuePtr InY, FValuePtr InZ)
{
	check(InX->Type->IsScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);

	FVectorValue* V = AsPrivate()->New<TVectorValue<3>>(FArithmeticType::GetVector(InX->Type->ToArithmetic()->ScalarKind, 3));
	TArrayView<FValuePtr> Components = V->GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;

	return V;
}

FValuePtr FBuilder::NewVector4(FValuePtr InX, FValuePtr InY, FValuePtr InZ, FValuePtr InW)
{
	check(InX->Type->IsScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);
	check(InZ->Type == InW->Type);

	FVectorValue* V = AsPrivate()->New<TVectorValue<4>>(FArithmeticType::GetVector(InX->Type->ToArithmetic()->ScalarKind, 4));
	TArrayView<FValuePtr> Components = V->GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;
	Components[3] = InW;

	return V;
}

FSetMaterialOutputInstr* FBuilder::EmitSetMaterialOutput(EMaterialProperty InProperty, FValue* InArgValue)
{
	FSetMaterialOutputInstr* Instr = AsPrivate()->New<FSetMaterialOutputInstr>(nullptr);
	Instr->Property = InProperty;
	Instr->ArgValue = InArgValue;
	Module->Outputs.Push(Instr);
	return Instr;
}

} // namespace MaterialIR

#endif // #if WITH_EDITOR
