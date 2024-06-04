// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIREmitter.h"
#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIRModuleBuilder.h"
#include "Shader/ShaderTypes.h"
#include "MaterialShared.h"
#include "MaterialExpressionIO.h"

#if WITH_EDITOR

namespace UE::MIR
{

template <typename T>
static T InitValue(FTypePtr InType)
{
	T Value{};
	Value.Kind = T::TypeKind;
	Value.Type = InType;
	return Value;
}

struct FEmitter::FPrivate : FEmitter {
	
	// Creates a new `FDimensional` value of specified `Type` and returns it.
	static FDimensional* NewDimensionalValue(FEmitter* Emitter, FArithmeticTypePtr Type)
	{
		check(!Type->IsScalar());
		int Dimensions = Type->NumRows * Type->NumColumns;
		FDimensional* Value = (FDimensional*)FMemory::Malloc(sizeof(FDimensional) + sizeof(FValuePtr) * Dimensions);
		Value->Kind = VK_Dimensional;
		Value->Type = Type;
		return Value;
	}

	// Searches for an existing value in module that matches specified `Prototype`.
	// If none found, it creates a new value as a copy of the prototype, adds it to
	// the module then returns it.
	template <typename TValueType>
	static FValuePtr EmitPrototype(FEmitter* Emitter, const TValueType& Prototype)
	{
		if (FValuePtr Existing = FindValue(Emitter, &Prototype))
		{
			return Existing;
		}

		TValueType* Value = new TValueType{ Prototype };
		Emitter->Module->Values.Add(Value);
		return Value;
	}

	// Emits specified newly created `Value`. If the exact value already exists,
	// specified one is *destroyed* and existing one is returned instead.
	static FValuePtr EmitNew(FEmitter* Emitter, FValuePtr Value)
	{
		if (FValuePtr Existing = FindValue(Emitter, Value))
		{
			delete Value;
			return Existing;
		}

		Emitter->Module->Values.Add(Value);
		return Value;
	}

	// Looks for an existing value in the module that matches `Prototype` and returns it if found.
	static FValuePtr FindValue(const FEmitter* Emitter, FValuePtr Prototype)
	{
		/* todo: improve this with hashmap */
		for (FValuePtr CurrValue : Emitter->Module->Values)
		{
			if (CurrValue->Equals(Prototype))
			{
				return CurrValue;
			}
		}

		return nullptr;
	}
};

FEmitter::FEmitter(FMaterialIRModuleBuilder* InBuilder, UMaterial* InMaterial, FMaterialIRModule* InModule)
{
	Builder = InBuilder;
	Material = InMaterial;
	Module = InModule;

	ZeroIntValue = EmitConstantScalarZero(SK_Int);
	ZeroFloatValue = EmitConstantScalarZero(SK_Float);
}

FValuePtr FEmitter::GetAndCheckInputTypeKindIs(const FExpressionInput* Input, ETypeKind Kind)
{
	FValuePtr InputValue = Get(Input);
	CheckInputTypeIs(Input, InputValue, Kind);
	return InputValue;
}

bool FEmitter::CheckInputTypeIs(const FExpressionInput* Input, FValuePtr InputValue, ETypeKind Kind)
{
	if (InputValue->Type->Kind != Kind)
	{
		Errorf(TEXT("Unexpected input '%s' value type (%s). Expected %s type."), *Input->InputName.ToString(), TEXT(""), TEXT(""));
		return false;
	}
	return true;
}

FValuePtr FEmitter::EmitConstantFromShaderValue(const UE::Shader::FValue& InValue)
{
	using namespace UE::Shader;

	switch (InValue.Type.ValueType)
	{
		case UE::Shader::EValueType::Float1: return EmitConstantFloat1(InValue.AsFloatScalar());
		case UE::Shader::EValueType::Float2: return EmitConstantFloat2(FVector2f{ InValue.Component[0].Float, InValue.Component[1].Float });
		case UE::Shader::EValueType::Float3: return EmitConstantFloat3(FVector3f{ InValue.Component[0].Float, InValue.Component[1].Float, InValue.Component[2].Float });
		case UE::Shader::EValueType::Float4: return EmitConstantFloat4(FVector4f{ InValue.Component[0].Float, InValue.Component[1].Float, InValue.Component[2].Float, InValue.Component[3].Float });

		case UE::Shader::EValueType::Int1: return EmitConstantInt1(InValue.AsFloatScalar());
		case UE::Shader::EValueType::Int2: return EmitConstantInt2(FIntVector2{ InValue.Component[0].Int, InValue.Component[1].Int });
		case UE::Shader::EValueType::Int3: return EmitConstantInt3(FIntVector3{ InValue.Component[0].Int, InValue.Component[1].Int, InValue.Component[2].Int });
		case UE::Shader::EValueType::Int4: return EmitConstantInt4(FIntVector4{ InValue.Component[0].Int, InValue.Component[1].Int, InValue.Component[2].Int, InValue.Component[3].Int });
	}

	UE_MIR_UNREACHABLE();
}

FValuePtr FEmitter::EmitConstantScalarZero(EScalarKind Kind)
{
	switch (Kind)
	{
		case SK_Bool: UE_MIR_TODO();
		case SK_Int: return ZeroIntValue;
		case SK_Float: return ZeroFloatValue;
		default: UE_MIR_UNREACHABLE();
	}
}

FValuePtr FEmitter::EmitConstantFloat1(float InX)
{
	FScalarConstant Scalar = InitValue<FScalarConstant>(FArithmeticType::GetScalar(SK_Float));
	Scalar.Float = InX;

	return FPrivate::EmitPrototype(this, Scalar);
}

FValuePtr FEmitter::EmitConstantFloat2(const FVector2f& InValue)
{
	FValuePtr X = EmitConstantFloat1(InValue.X);
	FValuePtr Y = EmitConstantFloat1(InValue.Y);
	return EmitVector2(X, Y);
}

FValuePtr FEmitter::EmitConstantFloat3(const FVector3f& InValue)
{
	FValuePtr X = EmitConstantFloat1(InValue.X);
	FValuePtr Y = EmitConstantFloat1(InValue.Y);
	FValuePtr Z = EmitConstantFloat1(InValue.Z);
	return EmitVector3(X, Y, Z);
}

FValuePtr FEmitter::EmitConstantFloat4(const FVector4f& InValue)
{
	FValuePtr X = EmitConstantFloat1(InValue.X);
	FValuePtr Y = EmitConstantFloat1(InValue.Y);
	FValuePtr Z = EmitConstantFloat1(InValue.Z);
	FValuePtr W = EmitConstantFloat1(InValue.W);
	return EmitVector4(X, Y, Z, W);
}

FValuePtr FEmitter::EmitConstantInt1(int InX)
{
	FScalarConstant Scalar = InitValue<FScalarConstant>(FArithmeticType::GetScalar(SK_Int));
	Scalar.Integer = InX;

	return FPrivate::EmitPrototype(this, Scalar);
}

FValuePtr FEmitter::EmitConstantInt2(const FIntVector2& InValue)
{
	FValuePtr X = EmitConstantInt1(InValue.X);
	FValuePtr Y = EmitConstantInt1(InValue.Y);
	return EmitVector2(X, Y);
}

FValuePtr FEmitter::EmitConstantInt3(const FIntVector3& InValue)
{
	FValuePtr X = EmitConstantInt1(InValue.X);
	FValuePtr Y = EmitConstantInt1(InValue.Y);
	FValuePtr Z = EmitConstantInt1(InValue.Z);
	return EmitVector3(X, Y, Z);
}

FValuePtr FEmitter::EmitConstantInt4(const FIntVector4& InValue)
{
	FValuePtr X = EmitConstantInt1(InValue.X);
	FValuePtr Y = EmitConstantInt1(InValue.Y);
	FValuePtr Z = EmitConstantInt1(InValue.Z);
	FValuePtr W = EmitConstantInt1(InValue.W);
	return EmitVector4(X, Y, Z, W);
}

FValuePtr FEmitter::EmitVector2(FValuePtr InX, FValuePtr InY)
{
	check(InX->Type->ToScalar());
	check(InX->Type == InY->Type);

	TDimensional<2> Vector = InitValue<TDimensional<2>>(FArithmeticType::GetVector(InX->Type->ToArithmetic()->ScalarKind, 2));
	TArrayView<FValuePtr> Components = Vector.GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;

	return FPrivate::EmitPrototype(this, Vector);
}

FValuePtr FEmitter::EmitVector3(FValuePtr InX, FValuePtr InY, FValuePtr InZ)
{
	check(InX->Type->ToScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);

	TDimensional<3> Vector = InitValue<TDimensional<3>>(FArithmeticType::GetVector(InX->Type->ToArithmetic()->ScalarKind, 3));
	TArrayView<FValuePtr> Components = Vector.GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;

	return FPrivate::EmitPrototype(this, Vector);
}

FValuePtr FEmitter::EmitVector4(FValuePtr InX, FValuePtr InY, FValuePtr InZ, FValuePtr InW)
{
	check(InX->Type->ToScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);
	check(InZ->Type == InW->Type);

	TDimensional<4> Vector = InitValue<TDimensional<4>>(FArithmeticType::GetVector(InX->Type->ToArithmetic()->ScalarKind, 4));
	TArrayView<FValuePtr> Components = Vector.GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;
	Components[3] = InW;

	return FPrivate::EmitPrototype(this, Vector);
}

FValuePtr FEmitter::EmitArithmetic(FArithmeticTypePtr Type, FValuePtr Scalar)
{
	check(Scalar->Type->ToScalar());
	if (Type->IsScalar())
	{
		check(Type == Scalar->Type);
		return Scalar;
	}

	FDimensional* Value = FPrivate::NewDimensionalValue(this, Type);
	for (FValuePtr& Component : Value->GetMutableComponents())
	{
		Component = Scalar;
	}

	return FPrivate::EmitNew(this, Value);
}

FSetMaterialOutput* FEmitter::EmitSetMaterialOutput(EMaterialProperty InProperty, FValue* InArgValue)
{
	FSetMaterialOutput* Instr = new FSetMaterialOutput;
	Instr->Kind 	= VK_SetMaterialOutput;
	Instr->Property = InProperty;
	Instr->ArgValue = InArgValue;

	Module->Values.Add(Instr);
	Module->Outputs.Add(Instr);
	return Instr;
}

static FTypePtr GetBinaryOperatorResultType(EBinaryOperator Operator, FTypePtr LhsType, FTypePtr RhsType)
{
	switch (Operator)
	{
		case BO_Add:
		case BO_Subtract:
		case BO_Multiply:
		case BO_Divide:
			check(LhsType == RhsType);
			return LhsType;
		
		default:
			UE_MIR_UNREACHABLE();
	}
}

FValuePtr FEmitter::EmitBinaryOperator(EBinaryOperator Operator, FValuePtr Lhs, FValuePtr Rhs)
{
	check(Lhs->Type == Rhs->Type);

	/* todo: operation folding */

	FTypePtr ResultType = GetBinaryOperatorResultType(Operator, Lhs->Type, Rhs->Type);
	FBinaryOperator Prototype = InitValue<FBinaryOperator>(ResultType);
	Prototype.Operator = Operator;
	Prototype.Lhs= Lhs;
	Prototype.Rhs = Rhs;

	return FPrivate::EmitPrototype(this, Prototype);
}

static FValuePtr ConvertScalarValue(FEmitter* Emitter, FValuePtr ScalarValue, EScalarKind ScalarKind, EScalarKind TargetKind)
{
	if (ScalarKind == TargetKind)
	{
		return ScalarValue;
	}

	auto ScalarConstant = ScalarValue->As<FScalarConstant>();
	if (!ScalarConstant)
	{
		UE_MIR_UNREACHABLE(); // todo
	}

	switch (ScalarKind)
	{
		case SK_Bool:
		case SK_Int:
		{
			switch (TargetKind)
			{
				case SK_Bool: UE_MIR_TODO();
				case SK_Int: return Emitter->EmitConstantInt1(ScalarConstant->Integer);
				case SK_Float: return Emitter->EmitConstantFloat1((float)ScalarConstant->Integer);
				default: UE_MIR_UNREACHABLE();
			}
		}

		case SK_Float:
		{
			switch (TargetKind)
			{
				case SK_Bool: UE_MIR_TODO();
				case SK_Int: return Emitter->EmitConstantInt1((int)ScalarConstant->Float);
				default: UE_MIR_UNREACHABLE();
			}
		}

		default: break;
	}

	UE_MIR_UNREACHABLE();
}

static FValuePtr ConvertArithmeticValue(FEmitter* Emitter, FValuePtr Value, FArithmeticTypePtr TargetArithmeticType)
{
	FArithmeticTypePtr ValueArithmeticType = static_cast<FArithmeticTypePtr>(Value->Type);
	if (ValueArithmeticType->IsScalar())
	{
		FValuePtr ConvertedScalar = ConvertScalarValue(Emitter, Value, ValueArithmeticType->ScalarKind, TargetArithmeticType->ScalarKind);
		return Emitter->EmitArithmetic(TargetArithmeticType, ConvertedScalar);
	}
	else
	{
		UE_MIR_TODO();
	}
}

FValuePtr FEmitter::TryEmitConvert(FValuePtr Value, FTypePtr TargetType)
{
	// If types already match, nothing else to do, simply return the same value.
	FTypePtr ValueType = Value->Type;
	if (ValueType == TargetType)
	{
		return Value;
	}

	FValuePtr ResultValue{};

	// Converting from an arithmetic value to another arithmetic type.
	FArithmeticTypePtr ValueArithmeticType = Value->Type->ToArithmetic();
	FArithmeticTypePtr TargetArithmeticType = TargetType->ToArithmetic();
	if (ValueArithmeticType && TargetArithmeticType)
	{
		ResultValue = ConvertArithmeticValue(this, Value, TargetArithmeticType);
	}

	/* ... */

	// No other legal conversions applicable. Report error if we haven't converted the value.
	if (!ResultValue)
	{
		Errorf(TEXT("Cannot convert value of type '%s' to '%s'"), TEXT(""), TEXT(""));
	}

	return ResultValue;
}

FValuePtr FEmitter::Get(const FExpressionInput* Input)
{
	check(Input);
	FValuePtr* Value = Builder->InputValues.Find(Input);
	return *Value;
}
	
void FEmitter::Put(const FExpressionOutput* Output, FValuePtr Value)
{
	check(Output);
	Builder->OutputValues.Add(Output, Value);
}

void FEmitter::Error(FString Message)
{
	FMaterialIRModule::FError Error;
	Error.Expression = Expression;
	Error.Message = MoveTemp(Message);
	Module->Errors.Push(Error);
	bHasExprBuildError = true;
}

} // namespace UE::MIR

#endif // #if WITH_EDITOR
