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

const TCHAR* VectorComponentToString(EVectorComponent Component)
{
	static const TCHAR* Strings[] = { TEXT("x"), TEXT("y"), TEXT("z"), TEXT("w") };
	return Strings[(int)Component];
}

FSwizzleMask::FSwizzleMask(EVectorComponent X)
: NumComponents{ 1 }
{
	Components[0] = X;
}

FSwizzleMask::FSwizzleMask(EVectorComponent X, EVectorComponent Y)
: NumComponents{ 4 }
{
	Components[0] = X;
	Components[1] = Y;
}

FSwizzleMask::FSwizzleMask(EVectorComponent X, EVectorComponent Y, EVectorComponent Z)
: NumComponents{ 4 }
{
	Components[0] = X;
	Components[1] = Y;
	Components[2] = Z;
}

FSwizzleMask::FSwizzleMask(EVectorComponent X, EVectorComponent Y, EVectorComponent Z, EVectorComponent W)
: NumComponents{ 4 }
{

	Components[0] = X;
	Components[1] = Y;
	Components[2] = Z;
	Components[3] = W;
}

void FSwizzleMask::Append(EVectorComponent Component)
{
	check(NumComponents < 4);
	Components[NumComponents++] = Component;
}

struct FEmitter::FPrivate : FEmitter
{
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

template <typename T>
static T InitValue(FTypePtr InType)
{
	T Value{};
	Value.Kind = T::TypeKind;
	Value.Type = InType;
	return Value;
}

template <typename TValueType>
static TValueType* NewValue(FEmitter* Emitter, FTypePtr Type)
{
	TValueType* Value = (TValueType*)new TValueType{};
	Value->Kind = TValueType::TypeKind;
	Value->Type = Type;
	FEmitter::FPrivate::PushNewValue(Value);
	return Value;
}

// Creates a new `FDimensional` value of specified `Type` and returns it.
static FDimensional* NewDimensionalValue(FEmitter* Emitter, FArithmeticTypePtr Type)
{
	check(!Type->IsScalar());
	int Dimensions = Type->NumRows * Type->NumColumns;
	void* Alloc = FMemory::Malloc(sizeof(FDimensional) + sizeof(FValue*) * Dimensions);
	FDimensional* Value = new (Alloc) FDimensional{};
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
	if (FValue* Existing = FEmitter::FPrivate::FindValue(Emitter, &Prototype))
	{
		return Existing;
	}

	TValueType* Value = new TValueType{ Prototype };
	FEmitter::FPrivate::PushNewValue(Emitter, Value);
	return Value;
}

// Emits specified newly created `Value`. If the exact value already exists,
// specified one is *destroyed* and existing one is returned instead.
static FValue* EmitNew(FEmitter* Emitter, FValue* Value)
{
	if (FValue* Existing = FEmitter::FPrivate::FindValue(Emitter, Value))
	{
		delete Value;
		return Existing;
	}

	FEmitter::FPrivate::PushNewValue(Emitter, Value);
	return Value;
}

FEmitter::FEmitter(FMaterialIRModuleBuilder* InBuilder, UMaterial* InMaterial, FMaterialIRModule* InModule)
{
	Builder = InBuilder;
	Material = InMaterial;
	Module = InModule;

	// Create and reference the true/false constants.
	FConstant Temp = InitValue<FConstant>(FArithmeticType::GetBool1());

	Temp.Boolean = true;
	ConstantTrue = EmitPrototype(this, Temp);

	Temp.Boolean = false;
	ConstantFalse = EmitPrototype(this, Temp);
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

FEmitter& FEmitter::DefaultTo(const FExpressionInput* Input, TFloat Float)
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
		case SK_Bool: return EmitConstantFalse();
		case SK_Int: return EmitConstantInt1(0);
		case SK_Float: return EmitConstantFloat1(0.0f);
		default: UE_MIR_UNREACHABLE();
	}
}

FValue* FEmitter::EmitConstantTrue()
{
	return ConstantTrue;
}

FValue* FEmitter::EmitConstantFalse()
{
	return ConstantFalse;
}

FValue* FEmitter::EmitConstantBool1(bool InX)
{
	return InX ? EmitConstantTrue() : EmitConstantFalse();
}

FValue* FEmitter::EmitConstantFloat1(TFloat InX)
{
	FConstant Scalar = InitValue<FConstant>(FArithmeticType::GetScalar(SK_Float));
	Scalar.Float = InX;
	return EmitPrototype(this, Scalar);
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

FValue* FEmitter::EmitConstantInt1(TInteger InX)
{
	FConstant Scalar = InitValue<FConstant>(FArithmeticType::GetScalar(SK_Int));
	Scalar.Integer = InX;
	return EmitPrototype(this, Scalar);
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
	TArrayView<FValue*> Components = Vector.GetComponents();
	Components[0] = InX;
	Components[1] = InY;

	return EmitPrototype(this, Vector);
}

FValue* FEmitter::EmitVector3(FValue* InX, FValue* InY, FValue* InZ)
{
	check(InX->Type->AsScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);

	TDimensional<3> Vector = InitValue<TDimensional<3>>(FArithmeticType::GetVector(InX->Type->AsArithmetic()->ScalarKind, 3));
	TArrayView<FValue*> Components = Vector.GetComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;

	return EmitPrototype(this, Vector);
}

FValue* FEmitter::EmitVector4(FValue* InX, FValue* InY, FValue* InZ, FValue* InW)
{
	check(InX->Type->AsScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);
	check(InZ->Type == InW->Type);

	TDimensional<4> Vector = InitValue<TDimensional<4>>(FArithmeticType::GetVector(InX->Type->AsArithmetic()->ScalarKind, 4));
	TArrayView<FValue*> Components = Vector.GetComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;
	Components[3] = InW;

	return EmitPrototype(this, Vector);
}

FValue* FEmitter::EmitSubscript(FValue* Value, int Index)
{
	FArithmeticTypePtr ArithmeticType = Value->Type->AsArithmetic();
	check(ArithmeticType);

	// Getting first component and Value is already a scalar, just return itself.
	if (Index == 0 && Value->Type->AsScalar())
	{
		return Value;
	}

	// Getting first component and Value is already a scalar, just return itself.
	if (FDimensional* DimensionalValue = Value->As<FDimensional>())
	{
		check(Index < DimensionalValue->GetComponents().Num());
		return DimensionalValue->GetComponents()[Index];
	}
	
	// We can't resolve it at compile time: emit subscript value.
	FSubscript Prototype = InitValue<FSubscript>(ArithmeticType->ToScalar());
	Prototype.Arg = Value;
	Prototype.Index = Index;

	return EmitPrototype(this, Prototype);
}

FValue* FEmitter::TryEmitSwizzle(FValue* Value, FSwizzleMask Mask)
{
	// At least one component must have been specified.
	check(Mask.NumComponents > 0);

	// We can only swizzle on non-matrix arithmetic types.
	FArithmeticTypePtr ArithmeticType = Value->Type->AsVector();
	if (!ArithmeticType || ArithmeticType->IsMatrix())
	{
		Errorf(TEXT("Cannot swizzle a `%s` value."), Value->Type->GetSpelling().GetData());
		return nullptr;
	}

	// Make sure each component in the mask fits the number of components in Value.
	for (EVectorComponent Component : Mask)
	{
		if ((int)Component >= ArithmeticType->NumRows)
		{
			Errorf(TEXT("Value of type `%s` has no component `%s`."), ArithmeticType->Spelling.GetData(), VectorComponentToString(Component));
			return nullptr;
		}
	}

	// If only one component is requested, we can use EmitSubscript() to return the single component.
	if (Mask.NumComponents == 1)
	{
		return EmitSubscript(Value, (int)Mask.Components[0]);
	}

	// If the requested number of components is the same as Value and the order in which the components
	// are specified in the mask is sequential (e.g. x, y, z) then this is a no op, simply return Value as is.
	if (Mask.NumComponents == ArithmeticType->GetNumComponents())
	{
		bool InOrder = true;
		for (int i = 0; i < Mask.NumComponents; ++i)
		{
			if (Mask.Components[i] != (EVectorComponent)i)
			{
				InOrder = false;
				break;
			}
		}

		if (InOrder)
		{
			return Value;
		}
	}
	
	// Make the result vector type.
	FArithmeticTypePtr ResultType = FArithmeticType::GetVector(ArithmeticType->ScalarKind, Mask.NumComponents);
	FDimensional* Result = NewDimensionalValue(this, ResultType);

	for (int i = 0; i < Mask.NumComponents; ++i)
	{
		Result->GetComponents()[i] = EmitSubscript(Value, (int)Mask.Components[i]);
	}

	return Result;
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

template <typename T>
static T FoldScalarArithmeticOperator(EBinaryOperator Operator, T Lhs, T Rhs)
{
	switch (Operator)
	{
		case BO_Add: return Lhs + Rhs;
		case BO_Subtract: return Lhs - Rhs;
		case BO_Multiply: return Lhs * Rhs;
		case BO_Divide: return Lhs / Rhs;
		default: UE_MIR_UNREACHABLE();
	}
}

template <typename T>
static bool FoldComparisonOperatorScalar(EBinaryOperator Operator, T Lhs, T Rhs)
{
	switch (Operator)
	{
		case BO_Greater: return Lhs > Rhs;
		case BO_GreaterOrEquals: return Lhs >= Rhs;
		case BO_Lower: return Lhs < Rhs;
		case BO_LowerOrEquals: return Lhs <= Rhs;
		case BO_Equals: return Lhs == Rhs;
		case BO_NotEquals: return Lhs != Rhs;
		default: UE_MIR_UNREACHABLE();
	}
}

static FValue* FoldBinaryOperatorScalar(FEmitter* Emitter, EBinaryOperator Operator, const FConstant* Lhs, const FConstant* Rhs)
{
	FArithmeticTypePtr ArithType = Lhs->Type->AsArithmetic();

	if (IsArithmeticOperator(Operator))
	{
		switch (ArithType->ScalarKind)
		{
			case SK_Int:
			{
				int Result = FoldScalarArithmeticOperator<TInteger>(Operator, Lhs->Integer, Rhs->Integer);
				return Emitter->EmitConstantInt1(Result);
			}

			case SK_Float:
			{
				TFloat Result = FoldScalarArithmeticOperator<TFloat>(Operator, Lhs->Float, Rhs->Float);
				return Emitter->EmitConstantFloat1(Result);
			}

			default:
				UE_MIR_UNREACHABLE();
		}
	}
	else if (IsComparisonOperator(Operator))
	{
		bool Result;
		switch (ArithType->ScalarKind)
		{
			case SK_Int:
				Result = FoldComparisonOperatorScalar<TInteger>(Operator, Lhs->Integer, Rhs->Integer);
				break;

			case SK_Float:
				Result = FoldComparisonOperatorScalar<TFloat>(Operator, Lhs->Float, Rhs->Float);
				break;

			default:
				UE_MIR_UNREACHABLE();
		}
		return Emitter->EmitConstantBool1(Result);
	}
	else
	{
		UE_MIR_UNREACHABLE();
	}
}

FValue* FEmitter::EmitBinaryOperator(EBinaryOperator Operator, FValue* Lhs, FValue* Rhs)
{
	// Argument value types must always match.
	check(Lhs->Type == Rhs->Type);

	// Get the operands arithmetic type.
	FArithmeticTypePtr ArithType = Lhs->Type->AsArithmetic();
	check(ArithType);

	// Try converting operands to scalar constants. If they're both so, fold the binary
	// operator right away and return the result.
	const FConstant* ScalarLhs = Lhs->As<FConstant>();
	const FConstant* ScalarRhs = Rhs->As<FConstant>();

	if (ScalarLhs && ScalarRhs)
	{
		if (FValue* Value = FoldBinaryOperatorScalar(this, Operator, ScalarLhs, ScalarRhs))
		{
			return Value;
		}
	}

	// Determine the result type. If the operator is arithmetic, the result type will be the same
	// as the operands type. Otherwise it will have the same number of components but bool.
	FArithmeticTypePtr ResultType = IsArithmeticOperator(Operator)
		? ArithType
		: FArithmeticType::Get(SK_Bool, ArithType->NumRows, ArithType->NumColumns);

	// Now check that both operands are dimensional. If so, emit a new dimensional value with each component
	// being the result of the binary operation applied to the corresponding components of the operands.
	const FDimensional* DimensionalLhs = Lhs->As<FDimensional>();
	const FDimensional* DimensionalRhs = Rhs->As<FDimensional>();

	if (DimensionalLhs && DimensionalRhs)
	{
		FDimensional* Result = NewDimensionalValue(this, ResultType);

		auto ResultComponents = Result->GetComponents();
		auto LhsComponents = DimensionalLhs->GetComponents();
		auto RhsComponents = DimensionalRhs->GetComponents();

		for (int i = 0; i < ResultType->GetNumComponents(); ++i)
		{
			ResultComponents[i] = EmitBinaryOperator(Operator, LhsComponents[i], RhsComponents[i]);
		}

		return Result;
	}

	// One (or both) of the operands is runtime only. Emit the runtime binary operation instruction.
	FBinaryOperator Proto = InitValue<FBinaryOperator>(ResultType);
	Proto.Operator = Operator;
	Proto.LhsArg= Lhs;
	Proto.RhsArg = Rhs;

	return EmitPrototype(this, Proto);
}

FValue* FEmitter::EmitBranch(FValue* Condition, FValue* True, FValue* False)
{
	// Condition must be of type bool
	check(Condition->Type->IsBool1());

	// If the condition is a scalar constant, then simply evaluate the result now.
	if (const FConstant* ConstCondition = Condition->As<FConstant>())
	{
		return ConstCondition->Boolean ? True : False;
	}

	// If the condition is not static, the types of the true and false operands must match.
	// The resulting type will be that of the branch instruction created.
	check(True->Type == False->Type);

	// Create the branch instruction.
	FBranch Proto = InitValue<FBranch>(True->Type);
	Proto.ConditionArg = Condition;
	Proto.TrueArg = True;
	Proto.FalseArg = False;

	return EmitPrototype(this, Proto);
}

static FValue* CastConstant(FEmitter* Emitter, FConstant* Constant, EScalarKind ConstantScalarKind, EScalarKind TargetKind)
{
	if (ConstantScalarKind == TargetKind)
	{
		return Constant;
	}

	switch (ConstantScalarKind)
	{
		case SK_Bool:
		case SK_Int:
		{
			switch (TargetKind)
			{
				case SK_Bool: UE_MIR_TODO();
				case SK_Int: return Emitter->EmitConstantInt1(Constant->Integer);
				case SK_Float: return Emitter->EmitConstantFloat1((TFloat)Constant->Integer);
				default: UE_MIR_UNREACHABLE();
			}
		}

		case SK_Float:
		{
			switch (TargetKind)
			{
				case SK_Bool: UE_MIR_TODO();
				case SK_Int: return Emitter->EmitConstantInt1((int)Constant->Float);
				default: UE_MIR_UNREACHABLE();
			}
		}

		default: break;
	}

	UE_MIR_UNREACHABLE();
}

static FValue* ConstructArithmeticValue(FEmitter* Emitter, FArithmeticTypePtr TargetArithmeticType, FValue* Initializer)
{
	FArithmeticTypePtr InitializerArithmeticType = Initializer->Type->AsArithmetic();
	if (!InitializerArithmeticType)
	{
		Emitter->Errorf(TEXT("Cannot construct a '%s' from non arithmetic type '%s'."), TargetArithmeticType->Spelling.GetData(), Initializer->Type->GetSpelling().GetData());
		return nullptr;
	}

	// Construct a scalar from another scalar.
	if (TargetArithmeticType->IsScalar() && InitializerArithmeticType->IsScalar())
	{
		// Construct the scalar from a constant.
		if (FConstant* ConstantInitializer = Initializer->As<FConstant>())
		{
			return CastConstant(Emitter, ConstantInitializer, InitializerArithmeticType->ScalarKind, TargetArithmeticType->ScalarKind);
		}
		else
		{
			/* todo: emit conversion unary operator */
			UE_MIR_TODO();
		}
	}

	// Construct a vector or matrix from a scalar. E.g. float4(3.14f)
	if (!TargetArithmeticType->IsScalar() && InitializerArithmeticType->IsScalar())
	{
		// Create the result dimensional value.
		FDimensional* Result = NewDimensionalValue(Emitter, TargetArithmeticType);

		// Create a dimensional and initialize each of its components to the conversion
		// of initializer value to the single component type.
		FValue* Component = Emitter->TryEmitConstruct(TargetArithmeticType->ToScalar(), Initializer);

		// Initialize all result components to the same scalar.
		for (int i = 0; i < TargetArithmeticType->GetNumComponents(); ++i)
		{
			Result->GetComponents()[i] = Component;
		}
		
		return EmitNew(Emitter, Result);
	}
	
	// Construct a vector from another vector. If constructed vector is larger, initialize
	// remaining components to zero. If it's smaller, truncate initializer vector and only use
	// the necessary components.
	if (TargetArithmeticType->IsVector() && InitializerArithmeticType->IsVector())
	{
		int TargetNumComponents = TargetArithmeticType->GetNumComponents();
		int InitializerNumComponents = InitializerArithmeticType->GetNumComponents();

		int MinNumComponents = FMath::Min(TargetNumComponents, InitializerNumComponents);
		int MaxNumComponents = FMath::Max(TargetNumComponents, InitializerNumComponents);

		// Create the result dimensional value.
		FDimensional* Result = NewDimensionalValue(Emitter, TargetArithmeticType);

		// Determine the result component type (scalar).
		FArithmeticTypePtr ResultComponentType = TargetArithmeticType->ToScalar();

		// For iterating over the components of the result dimensional value.
		int Index = 0;

		// Convert components from the initializer vector.
		for (; Index < MinNumComponents; ++Index)
		{
			Result->GetComponents()[Index] = Emitter->TryEmitConstruct(ResultComponentType, Emitter->EmitSubscript(Initializer, Index));
		}

		// Initialize remaining result dimensional components to zero.
		for (; Index < MaxNumComponents; ++Index)
		{
			Result->GetComponents()[Index] = Emitter->EmitConstantScalarZero(ResultComponentType->ScalarKind);
		}

		return EmitNew(Emitter, Result);
	}
	
	// The two arithmetic types are identical matrices that differ only by their scalar type.
	if (TargetArithmeticType->NumRows 	 == InitializerArithmeticType->NumRows &&
		TargetArithmeticType->NumColumns == InitializerArithmeticType->NumColumns)
	{
		check(TargetArithmeticType->IsMatrix());

		// 
		if (FDimensional* DimensionalInitializer = Initializer->As<FDimensional>())
		{
			// Create the result dimensional value.
			FDimensional* Result = NewDimensionalValue(Emitter, TargetArithmeticType);
			
			// Determine the result component type (scalar).
			FArithmeticTypePtr ResultComponentType = TargetArithmeticType->ToScalar();

			// Convert components from the initializer vector.
			for (int Index = 0, Num = TargetArithmeticType->GetNumComponents(); Index < Num; ++Index)
			{
				Result->GetComponents()[Index] = Emitter->TryEmitConstruct(ResultComponentType, DimensionalInitializer->GetComponents()[Index]);
			}

			return EmitNew(Emitter, Result);
		}
		else
		{
			// Initializer is an unknown value, construct target value casting initializer.
			FCast Prototype = InitValue<FCast>(TargetArithmeticType);
			Prototype.Arg = Initializer;
			return EmitPrototype(Emitter, Prototype);
		}
	}
	
	// Initializer value cannot be used to construct this arithmetic type.
	return nullptr;
}

FValue* FEmitter::TryEmitConstruct(FTypePtr Type, FValue* Initializer)
{
	// If target type matches initializer's, simply return the same value.
	FTypePtr InitializerType = Initializer->Type;
	if (InitializerType == Type)
	{
		return Initializer;
	}
	
	FValue* Result{};

	if (FArithmeticTypePtr ArithmeticType = Type->AsArithmetic())
	{
		Result = ConstructArithmeticValue(this, ArithmeticType, Initializer);
	}

	// No other legal conversions applicable. Report error if we haven't converted the value.
	if (!Result)
	{
		Errorf(TEXT("Cannot construct a '%s' from a '%s'."), TEXT(""), TEXT(""));
	}
	
	return Result;
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
