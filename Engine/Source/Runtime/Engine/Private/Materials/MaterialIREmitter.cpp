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
		FDimensional* Value = (FDimensional*)FMemory::Malloc(sizeof(FDimensional) + sizeof(FValue*) * Dimensions);
		Value->Kind = VK_Dimensional;
		Value->Type = Type;
		return Value;
	}

	// Searches for an existing value in module that matches specified `Prototype`.
	// If none found, it creates a new value as a copy of the prototype, adds it to
	// the module then returns it.
	template <typename TValueType>
	static FValue* EmitPrototype(FEmitter* Emitter, const TValueType& Prototype)
	{
		if (FValue* Existing = FindValue(Emitter, &Prototype))
		{
			return Existing;
		}

		TValueType* Value = new TValueType{ Prototype };
		PushNewValue(Emitter, Value);
		return Value;
	}

	// Emits specified newly created `Value`. If the exact value already exists,
	// specified one is *destroyed* and existing one is returned instead.
	static FValue* EmitNew(FEmitter* Emitter, FValue* Value)
	{
		if (FValue* Existing = FindValue(Emitter, Value))
		{
			delete Value;
			return Existing;
		}

		PushNewValue(Emitter, Value);
		return Value;
	}

	// Looks for an existing value in the module that matches `Prototype` and returns it if found.
	static FValue* FindValue(FEmitter* Emitter, const FValue* Prototype)
	{
		/* todo: improve this with hashmap */

		for (FValue* CurrValue : Emitter->Module->Values)
		{
			if (CurrValue->Equals(Prototype))
			{
				return CurrValue;
			}
		}

		return nullptr;
	}

	static void PushNewValue(FEmitter* Emitter, FValue* Value)
	{
		Emitter->Module->Values.Add(Value);
	}
};

FEmitter::FEmitter(FMaterialIRModuleBuilder* InBuilder, UMaterial* InMaterial, FMaterialIRModule* InModule)
{
	Builder = InBuilder;
	Material = InMaterial;
	Module = InModule;
}

FValue* FEmitter::Get(const FExpressionInput* Input)
{
	FValue** Value = Builder->InputValues.Find(Input);
	return Value ? *Value : nullptr;
}
	
void FEmitter::Put(const FExpressionOutput* Output, FValue* Value)
{
	check(Output);
	Builder->OutputValues.Add(Output, Value);
}

FEmitter& FEmitter::DefaultToFloatZero(const FExpressionInput* Input)
{
	return DefaultTo(Input, 0.0f);
}

FEmitter& FEmitter::DefaultTo(const FExpressionInput* Input, float Float)
{
	if (!Input->IsConnected())
	{
		Builder->InputValues.Add(Input, EmitConstantFloat1(Float));
	}

	return *this;
}

FValue* FEmitter::TryGetFloat(const FExpressionInput* Input)
{
	FValue* Value = Get(Input);
	CheckInputIsScalar(Input, Value, SK_Float);
	return Value;
}

FValue* FEmitter::TryGetScalar(const FExpressionInput* Input)
{
	FValue* Value = Get(Input);
	CheckInputIsScalar(Input, Value);
	return Value;
}

FValue* FEmitter::TryGetArithmetic(const FExpressionInput* Input)
{
	FValue* Value = Get(Input);
	CheckInputTypeIs(Input, Value, TK_Arithmetic);
	return Value;
}

FValue* FEmitter::TryGetOfType(const FExpressionInput* Input, ETypeKind Kind)
{
	FValue* Value = Get(Input);
	if (!Value)
	{
		return nullptr;
	}

	CheckInputTypeIs(Input, Value, Kind);
	return Value;
}

void FEmitter::CheckInputIsScalar(const FExpressionInput* Input, FValue* InputValue)
{
	FArithmeticTypePtr ArithmeticType = InputValue->Type->AsArithmetic();
	if (!ArithmeticType || !ArithmeticType->IsScalar())
	{
		Errorf(TEXT("Input '%s' expected to be a scalar. It is %s instead."), *Input->InputName.ToString(), InputValue->Type->GetSpelling().GetData());
	}
}

void FEmitter::CheckInputIsScalar(const FExpressionInput* Input, FValue* InputValue, EScalarKind Kind)
{
	FArithmeticTypePtr ArithmeticType = InputValue->Type->AsArithmetic();
	if (!ArithmeticType || !ArithmeticType->IsScalar() || ArithmeticType->ScalarKind != Kind)
	{
		Errorf(TEXT("Input '%s' expected to be a %s scalar. It is %s instead."), *Input->InputName.ToString(), ScalarKindToString(Kind), InputValue->Type->GetSpelling().GetData());
	}
}

void FEmitter::CheckInputTypeIs(const FExpressionInput* Input, FValue* InputValue, ETypeKind Kind)
{
	if (InputValue->Type->Kind != Kind)
	{
		Errorf(TEXT("Input '%s' expected to be have type %s. It is %s instead."), *Input->InputName.ToString(), TypeKindToString(Kind), InputValue->Type->GetSpelling().GetData());
	}
}

FValue* FEmitter::EmitConstantFromShaderValue(const UE::Shader::FValue& InValue)
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

FValue* FEmitter::EmitConstantScalarZero(EScalarKind Kind)
{
	switch (Kind)
	{
		case SK_Bool: UE_MIR_TODO();
		case SK_Int: return EmitConstantInt1(0);
		case SK_Float: return EmitConstantFloat1(0.0f);
		default: UE_MIR_UNREACHABLE();
	}
}

FValue* FEmitter::EmitConstantFloat1(float InX)
{
	FScalarConstant Scalar = InitValue<FScalarConstant>(FArithmeticType::GetScalar(SK_Float));
	Scalar.Float = InX;
	return FPrivate::EmitPrototype(this, Scalar);
}

FValue* FEmitter::EmitConstantFloat2(const FVector2f& InValue)
{
	FValue* X = EmitConstantFloat1(InValue.X);
	FValue* Y = EmitConstantFloat1(InValue.Y);
	return EmitVector2(X, Y);
}

FValue* FEmitter::EmitConstantFloat3(const FVector3f& InValue)
{
	FValue* X = EmitConstantFloat1(InValue.X);
	FValue* Y = EmitConstantFloat1(InValue.Y);
	FValue* Z = EmitConstantFloat1(InValue.Z);
	return EmitVector3(X, Y, Z);
}

FValue* FEmitter::EmitConstantFloat4(const FVector4f& InValue)
{
	FValue* X = EmitConstantFloat1(InValue.X);
	FValue* Y = EmitConstantFloat1(InValue.Y);
	FValue* Z = EmitConstantFloat1(InValue.Z);
	FValue* W = EmitConstantFloat1(InValue.W);
	return EmitVector4(X, Y, Z, W);
}

FValue* FEmitter::EmitConstantInt1(int InX)
{
	FScalarConstant Scalar = InitValue<FScalarConstant>(FArithmeticType::GetScalar(SK_Int));
	Scalar.Integer = InX;
	return FPrivate::EmitPrototype(this, Scalar);
}

FValue* FEmitter::EmitConstantInt2(const FIntVector2& InValue)
{
	FValue* X = EmitConstantInt1(InValue.X);
	FValue* Y = EmitConstantInt1(InValue.Y);
	return EmitVector2(X, Y);
}

FValue* FEmitter::EmitConstantInt3(const FIntVector3& InValue)
{
	FValue* X = EmitConstantInt1(InValue.X);
	FValue* Y = EmitConstantInt1(InValue.Y);
	FValue* Z = EmitConstantInt1(InValue.Z);
	return EmitVector3(X, Y, Z);
}

FValue* FEmitter::EmitConstantInt4(const FIntVector4& InValue)
{
	FValue* X = EmitConstantInt1(InValue.X);
	FValue* Y = EmitConstantInt1(InValue.Y);
	FValue* Z = EmitConstantInt1(InValue.Z);
	FValue* W = EmitConstantInt1(InValue.W);
	return EmitVector4(X, Y, Z, W);
}

FValue* FEmitter::EmitVector2(FValue* InX, FValue* InY)
{
	check(InX->Type->AsScalar());
	check(InX->Type == InY->Type);

	TDimensional<2> Vector = InitValue<TDimensional<2>>(FArithmeticType::GetVector(InX->Type->AsArithmetic()->ScalarKind, 2));
	TArrayView<FValue*> Components = Vector.GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;

	return FPrivate::EmitPrototype(this, Vector);
}

FValue* FEmitter::EmitVector3(FValue* InX, FValue* InY, FValue* InZ)
{
	check(InX->Type->AsScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);

	TDimensional<3> Vector = InitValue<TDimensional<3>>(FArithmeticType::GetVector(InX->Type->AsArithmetic()->ScalarKind, 3));
	TArrayView<FValue*> Components = Vector.GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;

	return FPrivate::EmitPrototype(this, Vector);
}

FValue* FEmitter::EmitVector4(FValue* InX, FValue* InY, FValue* InZ, FValue* InW)
{
	check(InX->Type->AsScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);
	check(InZ->Type == InW->Type);

	TDimensional<4> Vector = InitValue<TDimensional<4>>(FArithmeticType::GetVector(InX->Type->AsArithmetic()->ScalarKind, 4));
	TArrayView<FValue*> Components = Vector.GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;
	Components[3] = InW;

	return FPrivate::EmitPrototype(this, Vector);
}

FValue* FEmitter::EmitArithmetic(FArithmeticTypePtr Type, FValue* Scalar)
{
	check(Scalar->Type->AsScalar());
	if (Type->IsScalar())
	{
		check(Type == Scalar->Type);
		return Scalar;
	}

	FDimensional* Value = FPrivate::NewDimensionalValue(this, Type);
	for (FValue*& Component : Value->GetMutableComponents())
	{
		Component = Scalar;
	}

	return FPrivate::EmitNew(this, Value);
}

FSetMaterialOutput* FEmitter::EmitSetMaterialOutput(EMaterialProperty InProperty, FValue* InArgValue)
{
	FSetMaterialOutput* Instr = new FSetMaterialOutput;
	Instr->Kind 	= VK_SetMaterialOutput;
	Instr->Block 	= Module->RootBlock;
	Instr->Property = InProperty;
	Instr->Arg 		= InArgValue;

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
		
		case BO_Greater:
		case BO_Lower:
		case BO_Equals:
			return FArithmeticType::GetBool();

		default:
			UE_MIR_UNREACHABLE();
	}
}

FValue* FEmitter::EmitBinaryOperator(EBinaryOperator Operator, FValue* Lhs, FValue* Rhs)
{
	check(Lhs->Type == Rhs->Type);

	/* todo: operation folding */

	FTypePtr ResultType = GetBinaryOperatorResultType(Operator, Lhs->Type, Rhs->Type);
	FBinaryOperator Proto = InitValue<FBinaryOperator>(ResultType);
	Proto.Operator = Operator;
	Proto.LhsArg= Lhs;
	Proto.RhsArg = Rhs;

	return FPrivate::EmitPrototype(this, Proto);
}

FValue* FEmitter::EmitBranch(FValue* Condition, FValue* True, FValue* False)
{
	/* todo: operation folding */

	check(True->Type == False->Type);

	FBranch Proto = InitValue<FBranch>(True->Type);
	Proto.ConditionArg = Condition;
	Proto.TrueArg = True;
	Proto.FalseArg = False;

	return FPrivate::EmitPrototype(this, Proto);
}

static FValue* ConvertScalarValue(FEmitter* Emitter, FValue* ScalarValue, EScalarKind ScalarKind, EScalarKind TargetKind)
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

static FValue* ConvertArithmeticValue(FEmitter* Emitter, FValue* Value, FArithmeticTypePtr TargetArithmeticType)
{
	FArithmeticTypePtr ValueArithmeticType = static_cast<FArithmeticTypePtr>(Value->Type);
	if (ValueArithmeticType->IsScalar())
	{
		FValue* ConvertedScalar = ConvertScalarValue(Emitter, Value, ValueArithmeticType->ScalarKind, TargetArithmeticType->ScalarKind);
		return Emitter->EmitArithmetic(TargetArithmeticType, ConvertedScalar);
	}
	else
	{
		UE_MIR_TODO();
	}
}

FValue* FEmitter::TryEmitConvert(FValue* Value, FTypePtr TargetType)
{
	// If types already match, nothing else to do, simply return the same value.
	FTypePtr ValueType = Value->Type;
	if (ValueType == TargetType)
	{
		return Value;
	}

	FValue* ResultValue{};

	// Converting from an arithmetic value to another arithmetic type.
	FArithmeticTypePtr ValueArithmeticType = Value->Type->AsArithmetic();
	FArithmeticTypePtr TargetArithmeticType = TargetType->AsArithmetic();
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

FArithmeticTypePtr FEmitter::TryGetCommonArithmeticType(FArithmeticTypePtr A, FArithmeticTypePtr B)
{
	// Trivial case: types are equal
	if (A == B)
	{
		return A;
	}
	
	//	
	if (A->IsMatrix() != B->IsMatrix())
	{
		Errorf(TEXT("No common arithmetic type between `%s` and `%s`."), A->Spelling.GetData(), B->Spelling.GetData());
		return nullptr;
	}

	EScalarKind ScalarKind = FMath::Max(A->ScalarKind, B->ScalarKind);
	int NumRows = FMath::Max(A->NumRows, B->NumRows);
	int NumColumns = FMath::Max(A->NumColumns, B->NumColumns);

	return FArithmeticType::Get(ScalarKind, NumRows, NumColumns);
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
