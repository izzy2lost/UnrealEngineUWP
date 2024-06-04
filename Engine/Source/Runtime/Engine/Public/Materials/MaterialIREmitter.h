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
	
	/* Analysis */
	FValuePtr GetAndCheckInputTypeKindIs(const FExpressionInput* Input, ETypeKind Kind);
	bool CheckInputTypeIs(const FExpressionInput* Input, FValuePtr InputValue, ETypeKind Kind);

	/* Constants */

	FValuePtr EmitConstantFromShaderValue(const UE::Shader::FValue& InValue);
	FValuePtr EmitConstantScalarZero(EScalarKind Kind);
	FValuePtr EmitConstantFloat1(float InX);
	FValuePtr EmitConstantFloat2(const FVector2f& InValue);
	FValuePtr EmitConstantFloat3(const FVector3f& InValue);
	FValuePtr EmitConstantFloat4(const FVector4f& InValue);
	FValuePtr EmitConstantInt1(int InX);
	FValuePtr EmitConstantInt2(const FIntVector2& InValue);
	FValuePtr EmitConstantInt3(const FIntVector3& InValue);
	FValuePtr EmitConstantInt4(const FIntVector4& InValue);
	FValuePtr EmitVector2(FValuePtr InX, FValuePtr InY);
	FValuePtr EmitVector3(FValuePtr InX, FValuePtr InY, FValuePtr InZ);
	FValuePtr EmitVector4(FValuePtr InX, FValuePtr InY, FValuePtr InZ, FValuePtr InW);

	/* Other Values */

	FValuePtr EmitArithmetic(FArithmeticTypePtr Type, FValuePtr Scalar);

	/* Instructions */

	FSetMaterialOutput* EmitSetMaterialOutput(EMaterialProperty InProperty, FValue* InArgValue);
	FValuePtr EmitBinaryOperator(EBinaryOperator Operator, FValuePtr Lhs, FValuePtr Rhs);
	FValuePtr TryEmitConvert(FValuePtr Value, FTypePtr TargetType);

	/* IO */

	FValuePtr Get(const FExpressionInput* Input);
	void Put(const FExpressionOutput* Output, FValuePtr Value);

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
	FValuePtr ZeroIntValue{};
	FValuePtr ZeroFloatValue{};
	bool bHasExprBuildError = true;

	struct FPrivate;
	friend FMaterialIRModuleBuilder;
};

} // namespace UE::MIR

#endif // #if WITH_EDITOR
