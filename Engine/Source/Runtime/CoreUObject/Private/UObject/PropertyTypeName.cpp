// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyTypeName.h"

#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/ScopeRWLock.h"

namespace UE
{

static_assert(sizeof(FPropertyTypeNameNode) <= 16);

constexpr static int32 GPropertyTypeNameBlockBits = 13;
constexpr static int32 GPropertyTypeNameBlockOffsetBits = 13;

constexpr static int32 GPropertyTypeNameBlockOffsetCount = 1 << GPropertyTypeNameBlockOffsetBits;
constexpr static int32 GPropertyTypeNameBlockOffsetMask = (1 << GPropertyTypeNameBlockOffsetBits) - 1;

constexpr static int32 GPropertyTypeNameBlockSize = GPropertyTypeNameBlockOffsetCount * sizeof(FPropertyTypeNameNode);
constexpr static int32 GPropertyTypeNameBlockCount = 1 << GPropertyTypeNameBlockBits;

LLM_DEFINE_TAG(FPropertyTypeName);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline uint32 GetTypeHash(const FPropertyTypeNameNode& Node)
{
	return HashCombineFast(GetTypeHashHelper(Node.Name), GetTypeHashHelper(Node.InnerCount));
}

inline bool operator==(const FPropertyTypeNameNode& Lhs, const FPropertyTypeNameNode& Rhs)
{
	return Lhs.Name == Rhs.Name && Lhs.InnerCount == Rhs.InnerCount;
}

inline FArchive& operator<<(FArchive& Ar, FPropertyTypeNameNode& Node)
{
	return Ar << Node.Name << Node.InnerCount;
}

inline const FPropertyTypeNameNode* AppendNode(FStringBuilderBase& Builder, const FPropertyTypeNameNode* Node)
{
	Builder << Node->Name;

	if (int32 Remaining = Node++->InnerCount)
	{
		Builder.AppendChar(TEXT('<'));
		for (; Remaining > 0; --Remaining)
		{
			Node = AppendNode(Builder, Node);
			Builder.AppendChar(TEXT(','));
		}
		Builder.RemoveSuffix(1);
		Builder.AppendChar(TEXT('>'));
	}

	return Node;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct FPropertyTypeNameNodeProxy
{
	const FPropertyTypeNameNode* First = nullptr;

	inline void HashAndMeasure(uint32& OutHash, int32& OutCount) const
	{
		uint32 Hash = 0;
		const FPropertyTypeNameNode* Node = First;
		for (int32 Remaining = 1; Remaining > 0; --Remaining, ++Node)
		{
			Hash = HashCombineFast(Hash, GetTypeHash(*Node));
			Remaining += Node->InnerCount;
		}
		OutHash = Hash;
		OutCount = UE_PTRDIFF_TO_INT32(Node - First);
	}

	friend inline uint32 GetTypeHash(const FPropertyTypeNameNodeProxy& Proxy)
	{
		uint32 Hash = 0;
		int32 Count = 0;
		Proxy.HashAndMeasure(Hash, Count);
		return Hash;
	}

	friend inline bool operator==(const FPropertyTypeNameNodeProxy& Lhs, const FPropertyTypeNameNodeProxy& Rhs)
	{
		const FPropertyTypeNameNode* LhsNode = Lhs.First;
		const FPropertyTypeNameNode* RhsNode = Rhs.First;
		for (int32 Remaining = 1; Remaining > 0; --Remaining, ++LhsNode, ++RhsNode)
		{
			if (*LhsNode == *RhsNode)
			{
				Remaining += LhsNode->InnerCount;
				continue;
			}
			return false;
		}
		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FPropertyTypeNameTable
{
public:
	FPropertyTypeNameTable();

	int32 FindOrAddByName(const FPropertyTypeNameNode* First);

	const FPropertyTypeNameNode* ResolveByIndex(int32 Index) const;

private:
	int32 StoreByIndex(const FPropertyTypeNameNode* Nodes, int32 Count);

	FPropertyTypeNameNode* AllocateBlock(int32 BlockIndex);

	// TODO: Shard the hash table and stop using TMap.
	mutable FRWLock NameMapMutex;
	TMap<FPropertyTypeNameNodeProxy, int32> NameMap;

	std::atomic<int32> NextIndex = 1; // 1 leaves a "None" entry at 0
	std::atomic<FPropertyTypeNameNode*> Blocks[GPropertyTypeNameBlockCount]{};
	FMutex BlockAllocationMutex;
};

FPropertyTypeNameTable GPropertyTypeNameTable;

FPropertyTypeNameTable::FPropertyTypeNameTable()
{
	AllocateBlock(0);
}

int32 FPropertyTypeNameTable::FindOrAddByName(const FPropertyTypeNameNode* First)
{
	const FPropertyTypeNameNodeProxy Proxy{First};

	uint32 Hash;
	int32 Count;
	Proxy.HashAndMeasure(Hash, Count);

	if (FReadScopeLock Lock(NameMapMutex); const int32* Index = NameMap.FindByHash(Hash, Proxy))
	{
		return *Index;
	}

	LLM_SCOPE_BYTAG(FPropertyTypeName);
	FWriteScopeLock Lock(NameMapMutex);
	const int32 Index = StoreByIndex(First, Count);
	NameMap.AddByHash(Hash, {ResolveByIndex(Index)}, Index);
	return Index;
}

const FPropertyTypeNameNode* FPropertyTypeNameTable::ResolveByIndex(int32 Index) const
{
	const int32 BlockIndex = Index >> GPropertyTypeNameBlockOffsetBits;
	const int32 BlockOffset = Index & GPropertyTypeNameBlockOffsetMask;
	return Blocks[BlockIndex].load(std::memory_order_relaxed) + BlockOffset;
}

int32 FPropertyTypeNameTable::StoreByIndex(const FPropertyTypeNameNode* Nodes, int32 Count)
{
	if (Count == 1 && Nodes->Name.IsNone())
	{
		return 0;
	}

	int32 Index;
	int32 BlockIndex;
	int32 BlockOffset;
	do
	{
		Index = NextIndex.fetch_add(Count, std::memory_order_relaxed);
		BlockIndex = Index >> GPropertyTypeNameBlockOffsetBits;
		BlockOffset = Index & GPropertyTypeNameBlockOffsetMask;
	}
	while (UNLIKELY(BlockOffset + Count >= GPropertyTypeNameBlockOffsetCount));

	check(Index + Count < GPropertyTypeNameBlockCount * GPropertyTypeNameBlockOffsetCount);

	FPropertyTypeNameNode* Block = Blocks[BlockIndex].load(std::memory_order_acquire);
	if (UNLIKELY(!Block))
	{
		Block = AllocateBlock(BlockIndex);
	}
	for (FPropertyTypeNameNode* Target = Block + BlockOffset; Count > 0; --Count)
	{
		*Target++ = *Nodes++;
	}
	return Index;
}

FPropertyTypeNameNode* FPropertyTypeNameTable::AllocateBlock(int32 BlockIndex)
{
	TUniqueLock Lock(BlockAllocationMutex);
	FPropertyTypeNameNode* Block = Blocks[BlockIndex].load(std::memory_order_acquire);
	if (!Block)
	{
		Block = (FPropertyTypeNameNode*)FMemory::MallocZeroed(GPropertyTypeNameBlockSize, alignof(FPropertyTypeNameNode));
		Blocks[BlockIndex].store(Block, std::memory_order_release);
	}
	return Block;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FName FPropertyTypeName::GetTypeName() const
{
	const FPropertyTypeNameNode* First = GPropertyTypeNameTable.ResolveByIndex(Index);
	return First->Name;
}

int32 FPropertyTypeName::GetTypeParameterCount() const
{
	const FPropertyTypeNameNode* First = GPropertyTypeNameTable.ResolveByIndex(Index);
	return First->InnerCount;
}

FPropertyTypeName FPropertyTypeName::GetTypeParameter(int32 ParamIndex) const
{
	const FPropertyTypeNameNode* First = GPropertyTypeNameTable.ResolveByIndex(Index);
	if (UNLIKELY(ParamIndex < 0 || ParamIndex >= First->InnerCount))
	{
		return {};
	}
	const FPropertyTypeNameNode* Param = First + 1;
	for (int32 Skip = ParamIndex; Skip > 0; --Skip, ++Param)
	{
		Skip += Param->InnerCount;
	}
	FPropertyTypeName Type = *this;
	Type.Index += UE_PTRDIFF_TO_INT32(Param - First);
	return Type;
}

uint32 GetTypeHash(const FPropertyTypeName& TypeName)
{
	const FPropertyTypeNameNode* First = GPropertyTypeNameTable.ResolveByIndex(TypeName.Index);
	return GetTypeHash(FPropertyTypeNameNodeProxy{First});
}

bool operator==(const FPropertyTypeName& Lhs, const FPropertyTypeName& Rhs)
{
	if (Lhs.Index == Rhs.Index)
	{
		return true;
	}

	const FPropertyTypeNameNode* LhsNode = GPropertyTypeNameTable.ResolveByIndex(Lhs.Index);
	const FPropertyTypeNameNode* RhsNode = GPropertyTypeNameTable.ResolveByIndex(Rhs.Index);
	return FPropertyTypeNameNodeProxy{LhsNode} == FPropertyTypeNameNodeProxy{RhsNode};
}

FArchive& operator<<(FArchive& Ar, FPropertyTypeName& TypeName)
{
	if (!Ar.IsPersistent())
	{
		Ar << TypeName.Index;
	}
	else if (Ar.IsLoading())
	{
		TArray<FPropertyTypeNameNode, TInlineAllocator<16>> Nodes;
		int32 Remaining = 1;
		do 
		{
			FPropertyTypeNameNode& Node = Nodes.AddDefaulted_GetRef();
			Ar << Node;
			Remaining += Node.InnerCount - 1;
		}
		while (Remaining > 0);
		TypeName.Index = GPropertyTypeNameTable.FindOrAddByName(Nodes.GetData());
	}
	else if (Ar.IsSaving())
	{
		const FPropertyTypeNameNode* Node = GPropertyTypeNameTable.ResolveByIndex(TypeName.Index);
		for (int32 Remaining = 1; Remaining > 0; --Remaining, ++Node)
		{
			FPropertyTypeNameNode NodeCopy = *Node;
			Ar << NodeCopy;
			Remaining += NodeCopy.InnerCount;
		}
	}
	return Ar;
}

FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FPropertyTypeName& TypeName)
{
	const FPropertyTypeNameNode* Node = GPropertyTypeNameTable.ResolveByIndex(TypeName.Index);
	AppendNode(Builder, Node);
	return Builder;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FPropertyTypeNameBuilder::BeginTypeParameters()
{
	checkf(!Nodes.IsEmpty(), TEXT("A type name must be added prior to setting type parameters."));
	ActiveIndex = Nodes.Num() - 1;
}

void FPropertyTypeNameBuilder::EndTypeParameters()
{
	ActiveIndex = OuterNodeIndex[ActiveIndex];
}

void FPropertyTypeNameBuilder::AddTypeName(FName Name)
{
	FPropertyTypeNameNode& Node = Nodes.AddDefaulted_GetRef();
	Node.Name = Name;
	Node.InnerCount = 0;

	OuterNodeIndex.Add(ActiveIndex);
	if (ActiveIndex >= 0)
	{
		++Nodes[ActiveIndex].InnerCount;
	}
}

FPropertyTypeName FPropertyTypeNameBuilder::Build() const
{
	FPropertyTypeName Type;
	Type.Index = GPropertyTypeNameTable.FindOrAddByName(Nodes.GetData());
	return Type;
}

void FPropertyTypeNameBuilder::Reset()
{
	Nodes.Reset();
	OuterNodeIndex.Reset();
	ActiveIndex = INDEX_NONE;
}

} // UE
