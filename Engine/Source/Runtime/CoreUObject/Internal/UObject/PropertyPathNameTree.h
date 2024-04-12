// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Templates/TypeHash.h"
#include "UObject/PropertyPathName.h"
#include "UObject/PropertyTypeName.h"

#define UE_API COREUOBJECT_API

namespace UE
{

/**
 * A tree of property path names and their associated types.
 *
 * A union of the paths that are added, ignoring any container index.
 */
class FPropertyPathNameTree
{
	/** Key corresponding to a FPropertyPathNameSegment with no index. */
	struct FKey
	{
		FName Name;
		FPropertyTypeName Type;

		[[nodiscard]] friend inline bool operator==(const FKey& Lhs, const FKey& Rhs)
		{
			return Lhs.Name == Rhs.Name && Lhs.Type == Rhs.Type;
		}

		[[nodiscard]] friend inline bool operator<(const FKey& Lhs, const FKey& Rhs)
		{
			if (int32 Compare = Lhs.Name.Compare(Rhs.Name))
			{
				return Compare < 0;
			}
			return Lhs.Type < Rhs.Type;
		}

		[[nodiscard]] friend inline uint32 GetTypeHash(const FKey& Key)
		{
			return HashCombineFast(GetTypeHash(Key.Name), GetTypeHash(Key.Type));
		}
	};

public:
	FPropertyPathNameTree() = default;
	FPropertyPathNameTree(FPropertyPathNameTree&&) = default;
	FPropertyPathNameTree(const FPropertyPathNameTree&) = delete;
	FPropertyPathNameTree& operator=(FPropertyPathNameTree&&) = default;
	FPropertyPathNameTree& operator=(const FPropertyPathNameTree&) = delete;

	/** True if the tree contains no property path names. */
	inline bool IsEmpty() const { return Nodes.IsEmpty(); }

	/** Remove every property path name from the tree. */
	UE_API void Empty();

	/** Adds the path to the tree. Keeps any existing nodes that match both name and type. */
	UE_API void Add(const FPropertyPathName& Path, int32 StartIndex = 0);

	/**
	 * Finds the path within the tree.
	 *
	 * @param OutSubTree   If non-null, this is set to the maybe-null sub-tree at the path if the path is found.
	 * @param Path         Path to search within the tree.
	 * @param StartIndex   Index of the segment within Path at which to start searching.
	 * @return true if the path exists within the tree, otherwise false.
	 */
	UE_API bool Find(FPropertyPathNameTree** OutSubTree, const FPropertyPathName& Path, int32 StartIndex = 0);
	inline bool Find(const FPropertyPathNameTree** OutSubTree, const FPropertyPathName& Path, int32 StartIndex = 0) const
	{
		return const_cast<FPropertyPathNameTree*>(this)->Find(const_cast<FPropertyPathNameTree**>(OutSubTree), Path, StartIndex);
	}

	class FConstIterator
	{
	public:
		inline explicit FConstIterator(const TMap<FKey, TUniquePtr<FPropertyPathNameTree>>::TConstIterator& InNodeIt)
			: NodeIt(InNodeIt)
		{
		}

		inline FConstIterator& operator++() { ++NodeIt; return *this; }

		inline explicit operator bool() const { return (bool)NodeIt; }

		inline bool operator==(const FConstIterator& Rhs) const { return NodeIt == Rhs.NodeIt; }
		inline bool operator!=(const FConstIterator& Rhs) const { return NodeIt != Rhs.NodeIt; }

		inline FName GetName() const { return NodeIt.Key().Name; }
		inline FPropertyTypeName GetType() const { return NodeIt.Key().Type; }
		inline const FPropertyPathNameTree* GetSubTree() const { return NodeIt.Value().Get(); }

	private:
		TMap<FKey, TUniquePtr<FPropertyPathNameTree>>::TConstIterator NodeIt;
	};

	inline FConstIterator CreateConstIterator() const { return FConstIterator(Nodes.CreateConstIterator()); }

private:
	TMap<FKey, TUniquePtr<FPropertyPathNameTree>> Nodes;
};

} // UE

#undef UE_API
