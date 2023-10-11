// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"
#include "UObject/Object.h"

#include "GraphHandle.generated.h"

class UGraphElement;
class UGraphVertex;
class UGraphEdge;
class UGraphIsland;

DECLARE_LOG_CATEGORY_EXTERN(LogGameplayGraph, Log, All);

USTRUCT()
struct GAMEPLAYGRAPH_API FGraphUniqueIndex
{
	GENERATED_BODY()
public:
	FGraphUniqueIndex(bool InIsTemp = false)
		: UniqueIndex(FGuid())
		, bIsTemporary(InIsTemp) 
	{}

	FGraphUniqueIndex(FGuid InUniqueIndex, bool InIsTemp = false)
		: UniqueIndex(InUniqueIndex)
		, bIsTemporary(InIsTemp) 
	{}


	bool IsValid() const
	{
		return UniqueIndex.IsValid();
	}

	bool IsTemporary() const
	{
		return bIsTemporary;
	}

	void SetTemporary(bool InTemp)
	{
		bIsTemporary = InTemp;
	}

	FGraphUniqueIndex NextUniqueIndex() const
	{
		return FGraphUniqueIndex(FGuid::NewGuid(),bIsTemporary);
	}

	static FGraphUniqueIndex CreateUniqueIndex(bool InIsTemp = false)
	{
		return FGraphUniqueIndex(FGuid::NewGuid(), InIsTemp);
	}

	bool operator==(const FGraphUniqueIndex& Other) const
	{
		return UniqueIndex == Other.UniqueIndex;
	}

	bool operator!=(const FGraphUniqueIndex& Other) const
	{
		return UniqueIndex != Other.UniqueIndex;
	}

	bool operator<(const FGraphUniqueIndex& Other) const
	{
		return UniqueIndex < Other.UniqueIndex;
	}

	friend uint32 GAMEPLAYGRAPH_API GetTypeHash(const FGraphUniqueIndex& InUniqueIndex)
	{
		return uint32(CityHash64((char*)&InUniqueIndex.UniqueIndex, sizeof(FGuid)));
	}

	FString ToString() const
	{
		return UniqueIndex.ToString();
	}


private:
	/** Unique identifier within a graph. */
	UPROPERTY(SaveGame)
	FGuid UniqueIndex = FGuid();

	/** Temporary Status for index */
	UPROPERTY(Transient)
	bool bIsTemporary = false;

};


/**
 * For persistence, every node in a graph is given a unique index.
 * A FGraphHandle encapsulates that index to make it easy to go from
 * the index to the node and vice versa.
 */
USTRUCT()
struct GAMEPLAYGRAPH_API FGraphHandle
{
	GENERATED_BODY()
public:
	FGraphHandle();
	FGraphHandle(FGraphUniqueIndex InUniqueIndex, TObjectPtr<UGraphElement> InElement);

	void Clear();

	/** Whether or not this handle has been initialized. */
	bool IsValid() const;
	bool HasElement() const;
	bool IsComplete() const;

	FGraphUniqueIndex GetUniqueIndex() const { return UniqueIndex; }

	void SetElement(TObjectPtr<UGraphElement> InElement);
	TObjectPtr<UGraphElement> GetElement() const;

	bool operator==(const FGraphHandle& Other) const;
	bool operator!=(const FGraphHandle& Other) const;
	bool operator<(const FGraphHandle& Other) const;

	friend uint32 GAMEPLAYGRAPH_API GetTypeHash(const FGraphHandle& Handle);
private:
	/** Unique identifier within a graph. */
	UPROPERTY(SaveGame)
	FGraphUniqueIndex UniqueIndex = FGraphUniqueIndex();

	/** Pointer to the graph */
	UPROPERTY(Transient)
	TWeakObjectPtr<UGraphElement> Element;
};

USTRUCT()
struct GAMEPLAYGRAPH_API FGraphVertexHandle : public FGraphHandle
{
	GENERATED_BODY()

	FGraphVertexHandle();
	FGraphVertexHandle(FGraphUniqueIndex InUniqueIndex, TObjectPtr<UGraphElement> InElement = nullptr)
		: FGraphHandle(InUniqueIndex, InElement)
	{}

	TObjectPtr<UGraphVertex> GetVertex() const;
};

USTRUCT()
struct GAMEPLAYGRAPH_API FGraphEdgeHandle : public FGraphHandle
{
	GENERATED_BODY()

	FGraphEdgeHandle();
	FGraphEdgeHandle(FGraphUniqueIndex InUniqueIndex, TObjectPtr<UGraphElement> InElement)
		: FGraphHandle(InUniqueIndex, InElement)
	{}

	TObjectPtr<UGraphEdge> GetEdge() const;
};

USTRUCT()
struct GAMEPLAYGRAPH_API FGraphIslandHandle : public FGraphHandle
{
	GENERATED_BODY()

	FGraphIslandHandle();
	FGraphIslandHandle(FGraphUniqueIndex InUniqueIndex, TObjectPtr<UGraphElement> InElement)
		: FGraphHandle(InUniqueIndex, InElement)
	{}

	TObjectPtr<UGraphIsland> GetIsland() const;
};