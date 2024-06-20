// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"

#if WITH_EDITOR

namespace UE::MIR {

//
class FEmitter
{
public:
	FEmitter(FMaterialIRModuleBuilder* InBuilder, UMaterial* InMaterial, FMaterialIRModule* InModule);
	
	/* IO */

	// Gets and returns the value flowing into specified `Input`. If disconnected,
	// it returns nullptr. 
	FValue* Get(const FExpressionInput* Input);

	// Flows specified `Value` out of specified expression `Output`.
	void 	Put(const FExpressionOutput* Output, FValue* Value);

	/* IO Helpers */

	//
	FEmitter& DefaultToFloatZero(const FExpressionInput* Input);

	//
	FEmitter& DefaultTo(const FExpressionInput* Input, float Float);

	// It gets the value flowing into it and checks that its type is float scalar.
	FValue* TryGetFloat(const FExpressionInput* Input);

	//
	FValue* TryGetScalar(const FExpressionInput* Input);

	//
	FValue* TryGetArithmetic(const FExpressionInput* Input);

	// Gets the value flowing into `Input` and returns it after checking that its
	// type matches `Kind`.
	FValue* TryGetOfType(const FExpressionInput* Input, ETypeKind Kind);

	/* Analysis */

	//
	void CheckInputIsScalar(const FExpressionInput* Input, FValue* InputValue);

	//
	void CheckInputIsScalar(const FExpressionInput* Input, FValue* InputValue, EScalarKind Kind);

	// Checks that the type of the value `InputValue` flowing into `Input` is of
	// specified type `Kind. If it isn't it reports an error. You may check whether
	// an error occurred with `IsInvalid()`.
	void CheckInputTypeIs(const FExpressionInput* Input, FValue* InputValue, ETypeKind Kind);

	/* Constants */

	FValue* EmitConstantFromShaderValue(const UE::Shader::FValue& InValue);
	FValue* EmitConstantScalarZero(EScalarKind Kind);
	FValue* EmitConstantFloat1(float InX);
	FValue* EmitConstantFloat2(const FVector2f& InValue);
	FValue* EmitConstantFloat3(const FVector3f& InValue);
	FValue* EmitConstantFloat4(const FVector4f& InValue);
	FValue* EmitConstantInt1(int InX);
	FValue* EmitConstantInt2(const FIntVector2& InValue);
	FValue* EmitConstantInt3(const FIntVector3& InValue);
	FValue* EmitConstantInt4(const FIntVector4& InValue);
	FValue* EmitVector2(FValue* InX, FValue* InY);
	FValue* EmitVector3(FValue* InX, FValue* InY, FValue* InZ);
	FValue* EmitVector4(FValue* InX, FValue* InY, FValue* InZ, FValue* InW);

	/* Other Values */

	FValue* EmitArithmetic(FArithmeticTypePtr Type, FValue* Scalar);

	/* Instructions */

	FSetMaterialOutput* EmitSetMaterialOutput(EMaterialProperty InProperty, FValue* InArgValue);
	FValue* EmitBinaryOperator(EBinaryOperator Operator, FValue* Lhs, FValue* Rhs);
	FValue* EmitBranch(FValue* Condition, FValue* True, FValue* False);
	FValue* TryEmitConvert(FValue* Value, FTypePtr TargetType);

	/* Types */

	FArithmeticTypePtr TryGetCommonArithmeticType(FArithmeticTypePtr A, FArithmeticTypePtr B);

	/* Error reporting */
	bool IsInvalid() const { return bHasExprBuildError; }

	template <int TFormatLength, typename... TArgs>
	void Errorf(const TCHAR (&Format)[TFormatLength], TArgs&&... Args)
	{
		Error(FString::Printf(Format, Forward<TArgs>(Args)...));
	}

	void Error(FString Message);

private:
	UMaterial* Material{};
	FMaterialIRModule* Module{};
	UMaterialExpression* Expression{};
	FMaterialIRModuleBuilder* Builder{};
	bool bHasExprBuildError = false;

	struct FPrivate;
	friend FMaterialIRModuleBuilder;
};

} // namespace UE::MIR

#endif // #if WITH_EDITOR
