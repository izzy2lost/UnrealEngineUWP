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

class ITargetPlatform;
class FMaterialIRModule;
class FMaterial;
struct FStaticParameterSet;
struct FShaderCompilerEnvironment;
class FMaterialIRModule;
class FMaterialIRModuleBuilder;
struct FExpressionInput;

namespace MaterialIR
{

/* Types*/
struct FType;
struct FArithmeticType;

using FTypePtr = const FType*;
using FArithmeticTypePtr = const FArithmeticType*;

/* IR */
struct FValue;
using FValuePtr = const FValue*;
struct FScalarValue;
struct FVector;
struct FSetMaterialOutputInstr;

/* Others */
class FBuilder;

//
template <typename TKindType>
struct FIRNode
{
	TKindType Kind;

	bool IsA(TKindType InKind) const
	{
		return (static_cast<uint64>(Kind) & static_cast<uint64>(InKind)) == static_cast<uint64>(InKind);
	}

	template <typename T>
	const T* Cast() const
	{
 		return this && IsA(T::TypeKind) ? static_cast<const T*>(this) : nullptr;
	}
};

}

#define UE_MIR_UNREACHABLE() { check(!"Unreachable"); UE_ASSUME(false); }

#define UE_MIR_PRIVATE() \
		struct FPrivate; \
		friend FPrivate; \
		FPrivate* AsPrivate() { return reinterpret_cast<FPrivate*>(this); }\
		const FPrivate* AsPrivate() const { return reinterpret_cast<const FPrivate*>(this); }

#define UE_MIR_BEGIN_PRIVATE(TypeName)\
		struct TypeName::FPrivate : TypeName {

#define UE_MIR_END_PRIVATE()\
		}

#endif // #if WITH_EDITOR
