// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "UObject/NameTypes.h"

#define UE_API COREUOBJECT_API

class FArchive;

namespace UE { class FPropertyTypeNameBuilder; }

namespace UE
{

struct UE_INTERNAL FPropertyTypeNameNode
{
	FName Name;
	int32 InnerCount = 0;
};

/**
 * Represents the type name of a property, including any containers and underlying types.
 *
 * Examples:
 * - int32 -> IntProperty
 * - TArray<int32> -> ArrayProperty<IntProperty>
 * - TArray<FStructType> -> ArrayProperty<StructProperty<StructType>>
 * - TMap<FKeyStruct, EByteEnum> -> MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteProperty<ByteEnum>>>
 */
class FPropertyTypeName
{
public:
	inline bool IsEmpty() const
	{
		return Index == 0;
	}

	/**
	 * Returns the type at the root of this property type name.
	 *
	 * Example: MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteProperty<ByteEnum>>>
	 * - GetTypeName() -> MapProperty
	 */
	UE_API FName GetTypeName() const;

	/**
	 * Returns the number of type parameters under the root of this property type name.
	 *
	 * Example: MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteProperty<ByteEnum>>>
	 * - GetTypeParameterCount() -> 2
	 */
	UE_API int32 GetTypeParameterCount() const;

	/**
	 * Returns the indexed parameter under the root of this property type name.
	 *
	 * Example: MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteProperty<ByteEnum>>>
	 * - GetTypeParameter(0) -> StructProperty<KeyStruct>
	 * - GetTypeParameter(1) -> EnumProperty<ByteProperty<ByteEnum>>
	 */
	UE_API FPropertyTypeName GetTypeParameter(int32 ParamIndex) const;

	/**
	 * Resets this to an empty type name.
	 */
	inline void Reset()
	{
		Index = 0;
	}

private:
	UE_API friend uint32 GetTypeHash(const FPropertyTypeName& TypeName);

	UE_API friend bool operator==(const FPropertyTypeName& Lhs, const FPropertyTypeName& Rhs);

	UE_API friend FArchive& operator<<(FArchive& Ar, FPropertyTypeName& TypeName);

	UE_API friend FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FPropertyTypeName& TypeName);

	int32 Index = 0;

	friend FPropertyTypeNameBuilder;
};

/**
 * Builder for FPropertyTypeName.
 *
 * Example: MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteProperty<ByteEnum>>>
 *
 * FPropertyTypeNameBuilder Builder;
 * Builder.AddTypeName(TEXT("MapProperty"));
 * Builder.BeginTypeParameters();
 *   Builder.AddTypeName(TEXT("StructProperty"));
 *   Builder.BeginTypeParameters();
 *     Builder.AddTypeName(TEXT("KeyStruct"));
 *   Builder.EndTypeParameters();
 *   Builder.AddTypeName(TEXT("EnumProperty"));
 *   Builder.BeginTypeParameters();
 *     Builder.AddTypeName(TEXT("ByteProperty"));
 *     Builder.BeginTypeParameters();
 *       Builder.AddTypeName(TEXT("ByteEnum"));
 *     Builder.EndTypeParameters();
 *   Builder.EndTypeParameters();
 * Builder.EndTypeParameters();
 * FPropertyTypeName Name = Builder.Build();
 */
class FPropertyTypeNameBuilder
{
public:
	UE_API void AddTypeName(FName Name);

	/** Mark the beginning of the type parameters for the last added type. */
	UE_API void BeginTypeParameters();

	/** Mark the end of the type parameters for the matching begin. */
	UE_API void EndTypeParameters();

	UE_API FPropertyTypeName Build() const;

	/** Resets the builder to allow it to be reused. */
	UE_API void Reset();

private:
	TArray<FPropertyTypeNameNode, TInlineAllocator<8>> Nodes;
	TArray<int32, TInlineAllocator<8>> OuterNodeIndex;
	int32 ActiveIndex = INDEX_NONE;
};

} // UE

#undef UE_API
