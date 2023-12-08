// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EBreakBehavior.h"
#include "ConcertPropertySelection.generated.h"

class UStruct;
struct FArchiveSerializedPropertyChain;

/**
 * Describes the path to a FProperty replicated by Concert.
 * 
 * See Concert.Replication.Data.ForEachReplicatableConcertProperty for path examples (just CTRL+SHIFT+F or go to ConcertSyncTest/Replication/ConcertPropertyTests.cpp).
 */
USTRUCT()
struct CONCERTSYNCCORE_API FConcertPropertyChain
{
	GENERATED_BODY()

	/**
	 * The name of inner properties of containers (array, set, map), which are either primitive or native serialized structs.
	 * 
	 * Inner properties are included if they are at the end of the path (and only for primitive or native serialized structs!).
	 * Cases:
	 * - ArrayOfStructs.ArrayOfFloats.Value
	 *	There is an array property called ArrayOfStructs containing structs. The contained struct has an array of floats property
	 *	called ArrayOfFloats. Value corresponds to FArrayProperty::Inner and is called InternalContainerPropertyValueName.
	 * - ArrayOfStructs.ArrayOfFloats
	 *  Similar situation but only the size of the array will be replicated. The elements are default initialized.
	 * - ArrayOfStructs.Value
	 *  There is an array property called ArrayOfStructs and the struct has a native Serialize() function.
	 *  
	 * - BUT not ArrayOfStructs.Value.ArrayOfFloats.Value.
	 *	This path would imply a different situation in which ArrayOfStructs contains a struct with a struct property called Value;
	 *	Value's owning struct property would contain an array of floats called ArrayOfFloats.
	 */
	static const FName InternalContainerPropertyValueName;

	/** Constructs a FConcertPropertyChain from a path if it is valid. If you need to create many paths in one go, use PropertyUtils::BulkConstructConcertChainsFromPaths instead. */
	static TOptional<FConcertPropertyChain> CreateFromPath(const UStruct& Class, const TArray<FName>& NamePath);

	FConcertPropertyChain() = default;
	/**
	 * @param OptionalChain The chain leading up to LeafProperty. If it is a root property it can either be empty or nullptr. This mimics the behaviours of FArchive.
	 * @param LeafProperty The property the chain leads to. It will be the last property in PathToProperty. This can be the inner property of a container but ONLY for primitive types (float, etc.) or structs with a custom Serialize function. 
	 */
	FConcertPropertyChain(const FArchiveSerializedPropertyChain* OptionalChain, const FProperty& LeafProperty);
	
	/** Gets the leaf property, which is the property the path leads towards. */
	FName GetLeafProperty() const { return IsEmpty() ? NAME_None : PathToProperty[PathToProperty.Num() - 1]; }
	FName GetRootProperty() const { return IsEmpty() ? NAME_None : PathToProperty[0]; }
	bool IsRootProperty() const { return PathToProperty.Num() == 1; }
	bool IsEmpty() const { return PathToProperty.IsEmpty(); }

	/** @return Whether this is a parent of ChildToCheck */
	bool IsParentOf(const FConcertPropertyChain& ChildToCheck) const { return ChildToCheck.IsChildOf(*this); }
	
	/** @return Whether the leaf property is a child of the given property chain. */
	bool IsChildOf(const FConcertPropertyChain& ParentToCheck) const;
	/** @return Whether the leaf property is a direct child of the given property chain. */
	bool IsDirectChildOf(const FConcertPropertyChain& ParentToCheck) const;

	/** @return Whether OptionalChain and LeafProperty correspond to this path. */
	bool MatchesExactly(const FArchiveSerializedPropertyChain* OptionalChain, const FProperty& LeafProperty) const;

	/** @return Attempts to resolve this property given the class */
	FProperty* ResolveProperty(UStruct& Class, bool bLogOnFail = true) const;

	const TArray<FName>& GetPathToProperty() const { return PathToProperty; }

	enum class EToStringMethod
	{
		Path,
		LeafProperty
	};
	FString ToString(EToStringMethod Method = EToStringMethod::Path) const;
	
	friend bool operator==(const FConcertPropertyChain& Left, const FConcertPropertyChain& Right)
	{
		return Left.PathToProperty == Right.PathToProperty;
	}
	friend bool operator!=(const FConcertPropertyChain& Left, const FConcertPropertyChain& Right)
	{
		return !(Left == Right);
	}

	friend bool operator==(const FConcertPropertyChain& Left, const TArray<FName>& Path)
	{
		return Left.PathToProperty == Path;
	}
	friend bool operator!=(const FConcertPropertyChain& Left, const TArray<FName>& Path)
	{
		return Left.PathToProperty != Path;
	}
	
	friend bool operator==(const TArray<FName>& Path, const FConcertPropertyChain& Left)
	{
		return Left.PathToProperty == Path;
	}
	friend bool operator!=(const TArray<FName>& Path, const FConcertPropertyChain& Left)
	{
		return Left.PathToProperty != Path;
	}
	
private:
	
	/**
	 * Path from root of UObject to leaf property. Includes the leaf property.
	 * This property is kept private to force the use of the exposed constructors.
	 *
	 * See Concert.Replication.Data.ForEachReplicatableConcertProperty for path examples (just CTRL+SHIFT+F or go to ConcertSyncTest/Replication/ConcertPropertyTests.cpp).
	 *
	 * Suppose:
	 * class AFooActor
	 * {
	 *		UPROPERTY()
	 *		FFooStruct Struct;
	 * 
	 *		UPROPERTY()
	 *		TArray<FFooStruct> ArrayOfStructs;
	 *
	 *		UPROPERTY()
	 *		TMap<int32, FFooStruct> MapOfStructs;
	 *
	 *		UPROPERTY()
	 *		TMap<int32, float> MapIntToFloat;
	 *
	 *		UPROPERTY(Instanced)
	 *		TArray<UFooSubobject*> UnsupportedInstanced;
	 * };
	 *
	 * struct FFooStruct
	 * {
	 *		UPROPERTY()
	 *		int32 Foo;
	 *
	 *		UPROPERTY()
	 *		int32 Bar;
	 * };
	 *
	 * Listing some paths:
	 *  - { "Struct", "Foo" }
	 *  - { "ArrayOfStructs", "Foo" }
	 *  - { "MapOfStructs" } > Replicate all keys but not the struct values
	 *  - { "MapOfStructs", "Foo" } > Replicate all keys and the Foo struct value.
	 *	- { "MapIntToFloat" } > Replicate all keys but not the values
	 *	- { "MapIntToFloat", "Value" } > Replicate all the keys and the values. Special non-ustruct exception where the inner property is listed! 
	 *  
	 * The inner properties of containers, e.g. FArrayProperty::Inner, FMapProperty::KeyProp, etc., generally do not appear in the path
	 * but there is one special case for TMap where it does.
	 * More details:
	 *	- TArray, TSet: The inner properties are "obvious" so we can shorten the path.
	 *	- TMap:
	 *	  - Key: The key is required. We assume reasonably that all properties of the key property must be replicated. Hence, there is no point in listing
	 *	  the key inner property nor any of its subproperties. Example: If key is FSoftObjectPath, we do not list FSoftObjectPath::AssetPath, etc.
	 *	  - Value:
	 *	    - If the inner value property is is ustruct, then it is not listed. The child properties of the inner property are listed.
	 *	    - If the inner value property is not a ustruct (meaning a primitive float, int, etc.), then the child property is listed and called "Value" (see InternalContainerPropertyValueName).
	 *	      This is needed to differentiate between the cases "serializes only the keys" and "serializes the keys and the values".
	 *	  - Final word about FConcertPropertySelection:
	 *		- { "MapOfStructs", "Foo" } means all keys and the Foo property is serialized (nothing is said about Bar - it is replicated if it is also in the property selection).
	 *		- { "MapOfStructs" } means only the keys are serialized and the values are skipped (Foo and Bar will be value-initialized)
	 *
	 * Paths do not include the child properties of uStructs implementing a custom Serialize function with struct ops. In that case, the user can only specify
	 * include the struct or skip it.
	 *
	 * FConcertPropertyChains do NOT cross the UObject border. For example, there would be no thing as [1] "UnsupportedInstanced" [x] ...
	 * You'd start a new FConcertPropertyChain for properties for objects of type UFooSubobject.
	 */
	UPROPERTY()
	TArray<FName> PathToProperty;
};

/** List of properties to be replicated for a given object */
USTRUCT()
struct CONCERTSYNCCORE_API FConcertPropertySelection
{
	GENERATED_BODY()

	/** List of replicated properties. */
	UPROPERTY()
	TArray<FConcertPropertyChain> ReplicatedProperties;

	/** @return Whether this and Other contain at least one property that is the same. */
	bool OverlapsWith(const FConcertPropertySelection& Other) const { return EnumeratePropertyOverlaps(ReplicatedProperties, Other.ReplicatedProperties); }

	/** @return Whether this includes all properties of Other */
	bool Includes(const FConcertPropertySelection& Other) const;
	
	/**
	 * Determines all properties that overlap.
	 * This algorithm is strictly O(n^2) but runs O(n) on average.
	 * 
	 * @return Whether there were any property overlaps.
	 */
	static bool EnumeratePropertyOverlaps(
		TConstArrayView<FConcertPropertyChain> First,
		TConstArrayView<FConcertPropertyChain> Second,
		TFunctionRef<EBreakBehavior(const FConcertPropertyChain&)> Callback = [](const FConcertPropertyChain&){ return EBreakBehavior::Break; }
		);
	
	friend bool operator==(const FConcertPropertySelection& Left, const FConcertPropertySelection& Right)
	{
		return Left.ReplicatedProperties == Right.ReplicatedProperties;
	}
	friend bool operator!=(const FConcertPropertySelection& Left, const FConcertPropertySelection& Right)
	{
		return !(Left == Right);
	}
};

CONCERTSYNCCORE_API uint32 GetTypeHash(const FConcertPropertyChain& Chain);