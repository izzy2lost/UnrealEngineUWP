// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "UObject/NameTypes.h"

#define UE_API COREUOBJECT_API

class FArchive;
class FStructuredArchiveSlot;

namespace UE { class FPropertyTypeNameBuilder; }

namespace UE
{

struct FPropertyTypeNameNode
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
 * - TMap<FKeyStruct, EByteEnum> -> MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteEnum,ByteProperty>>
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
	 * Example: MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteEnum,ByteProperty>>
	 * - GetTypeName() -> MapProperty
	 */
	UE_API FName GetTypeName() const;

	/**
	 * Returns the number of type parameters under the root of this property type name.
	 *
	 * Example: MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteEnum,ByteProperty>>
	 * - GetTypeParameterCount() -> 2
	 */
	UE_API int32 GetTypeParameterCount() const;

	/**
	 * Returns the indexed parameter under the root of this property type name.
	 *
	 * An out-of-bounds index will return an empty type name.
	 *
	 * Example: MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteEnum,ByteProperty>>
	 * - GetTypeParameter(0) -> StructProperty<KeyStruct>
	 * - GetTypeParameter(1) -> EnumProperty<ByteEnum,ByteProperty>
	 */
	UE_API FPropertyTypeName GetTypeParameter(int32 ParamIndex = 0) const;

	/**
	 * Returns the indexed parameter type name under the root of this property type name.
	 *
	 * An out-of-bounds index will return a name of None.
	 *
	 * Example: MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteEnum,ByteProperty>>
	 * - GetTypeParameterName(0) -> StructProperty
	 * - GetTypeParameterName(1) -> EnumProperty
	 */
	inline FName GetTypeParameterName(int32 ParamIndex = 0) const
	{
		return GetTypeParameter(ParamIndex).GetTypeName();
	}

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
	UE_API friend bool operator<(const FPropertyTypeName& Lhs, const FPropertyTypeName& Rhs);

	UE_API friend FArchive& operator<<(FArchive& Ar, FPropertyTypeName& TypeName);
	UE_API friend void operator<<(FStructuredArchiveSlot Slot, FPropertyTypeName& TypeName);

	UE_API friend FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FPropertyTypeName& TypeName);

	int32 Index = 0;

	friend FPropertyTypeNameBuilder;
};

/**
 * Builder for FPropertyTypeName.
 *
 * Example: MapProperty<StructProperty<KeyStruct>,EnumProperty<ByteEnum,ByteProperty>>
 *
 * FPropertyTypeNameBuilder Builder;
 * Builder.AddTypeName(NAME_MapProperty));
 * Builder.BeginTypeParameters();
 *   Builder.AddTypeName(NAME_StructProperty);
 *   Builder.BeginTypeParameters();
 *     Builder.AddTypeName(TEXT("KeyStruct"));
 *   Builder.EndTypeParameters();
 *   Builder.AddTypeName(NAME_EnumProperty);
 *   Builder.BeginTypeParameters();
 *     Builder.AddTypeName(TEXT("ByteEnum"));
 *     Builder.AddTypeName(NAME_ByteProperty);
 *   Builder.EndTypeParameters();
 * Builder.EndTypeParameters();
 * FPropertyTypeName Name = Builder.Build();
 */
class FPropertyTypeNameBuilder
{
public:
	/** Add a complete type name with its type parameters. */
	UE_API void AddTypeName(FPropertyTypeName Name);

	/** Add a type name without any type parameters. */
	UE_API void AddTypeName(FName Name);

	/** Mark the beginning of the type parameters for the last added type. */
	UE_API void BeginTypeParameters();

	/** Mark the end of the type parameters for the matching begin. */
	UE_API void EndTypeParameters();

	/** Build from the types that have been added. */
	UE_API FPropertyTypeName Build() const;

	/** Resets the builder to allow it to be reused. */
	UE_API void Reset();

	/**
	 * Try to parse and add a complete type name with its type parameters.
	 *
	 * Builder is restored to its previous state when parsing fails.
	 *
	 * @param Name A name in the format returned by operator<<(FStringBuilderBase&, const FPropertyTypeName&).
	 * @return true if a name was parsed into the builder, false on failure.
	 */
	UE_API bool TryParse(FStringView Name);

private:
	TArray<FPropertyTypeNameNode, TInlineAllocator<8>> Nodes;
	TArray<int32, TInlineAllocator<8>> OuterNodeIndex;
	int32 ActiveIndex = INDEX_NONE;
};

} // UE

#undef UE_API
