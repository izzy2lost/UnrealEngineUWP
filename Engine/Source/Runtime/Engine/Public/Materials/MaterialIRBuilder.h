// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"

#if WITH_EDITOR

namespace MaterialIR
{

//
class FBuilder
{
public:

	FBuilder();
	FBuilder(FMaterialIRModule* InModule);

	void SetTargetModule(FMaterialIRModule* InModule);

	/* Constants */

	FValuePtr NewConstantFromShaderValue(const UE::Shader::FValue& InValue);
	FValuePtr NewConstantFloat1(float InX);
	FValuePtr NewConstantFloat2(const FVector2f& InValue);
	FValuePtr NewConstantFloat3(const FVector3f& InValue);
	FValuePtr NewConstantFloat4(const FVector4f& InValue);
	FValuePtr NewConstantInt1(int InX);
	FValuePtr NewConstantInt2(const FIntVector2& InValue);
	FValuePtr NewConstantInt3(const FIntVector3& InValue);
	FValuePtr NewConstantInt4(const FIntVector4& InValue);
	FValuePtr NewVector2(FValuePtr InX, FValuePtr InY);
	FValuePtr NewVector3(FValuePtr InX, FValuePtr InY, FValuePtr InZ);
	FValuePtr NewVector4(FValuePtr InX, FValuePtr InY, FValuePtr InZ, FValuePtr InW);

	/* Instructions */

	FSetMaterialOutputInstr* EmitSetMaterialOutput(EMaterialProperty InProperty, FValue* InArgValue);

private:
	FMaterialIRModule* Module{};

	UE_MIR_PRIVATE();
};

} // namespace MaterialIR

#endif
