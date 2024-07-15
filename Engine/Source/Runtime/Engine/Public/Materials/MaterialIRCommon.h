// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SceneTypes.h"
#include "RHIShaderPlatform.h"
#include "Containers/Array.h"
#include "Math/MathFwd.h"

#if WITH_EDITOR

/* Forward declarations */

namespace UE::Shader
{
	struct FValue;
}

namespace ERHIFeatureLevel { enum Type : int; }

class FMaterial;
class FMaterialIRModule;
class FMaterialIRModule;
class FMaterialIRModuleBuilder;
class ITargetPlatform;
class UMaterial;
class UMaterialExpression;
struct FExpressionInput;
struct FExpressionOutput;
struct FShaderCompilerEnvironment;
struct FStaticParameterSet;
struct FMaterialInputDescription;
struct FMaterialInsights;

namespace UE::MIR
{

/* Types*/
struct FType;
struct FArithmeticType;

using FTypePtr = const FType*;
using FArithmeticTypePtr = const FArithmeticType*;

/* IR */
struct FValue;
struct FInstruction;
struct FSetMaterialOutput;
using FValuePtr = const FValue*;
using FInstructionPtr = const FInstruction*;

/* Others */
class FEmitter;
struct FBlock;

}

#define UE_MIR_UNREACHABLE() { check(!"Unreachable"); UE_ASSUME(false); }
#define UE_MIR_TODO() UE_MIR_UNREACHABLE()

#endif // #if WITH_EDITOR
