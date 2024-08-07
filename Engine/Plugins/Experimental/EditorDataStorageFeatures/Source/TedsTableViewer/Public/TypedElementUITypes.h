// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Framework/Views/TableViewTypeTraits.h"

namespace UE::EditorDataStorage
{
	// Wrapper struct around TypedElementDataStorage::RowHandle so we can template speicalize for it without also specializing for uint64
	struct UIRowType
	{
		TypedElementDataStorage::RowHandle RowHandle;
		
		UIRowType()
			: RowHandle(TypedElementDataStorage::InvalidRowHandle)
		{}

		UIRowType(TypedElementDataStorage::RowHandle InRowHandle)
			: RowHandle(InRowHandle)
		{}

		operator TypedElementDataStorage::RowHandle () const
		{
			return RowHandle;
		}

		UIRowType& operator=(TypedElementDataStorage::RowHandle InRowHandle)
		{
			RowHandle = InRowHandle;
			return *this;
		}
		
		friend uint32 GetTypeHash(const UIRowType& Key)
		{
			return GetTypeHash(Key.RowHandle);
		}

	};
}

/* Template declaration to describe how a row handle behaves as a type for slate widgets like SListView, STreeView etc
 * This allows you to use Row Handles with slate widgets that work on pointers by using the wrapper struct e.g
 * SListView<UE::EditorDataStorage::UIRowType>
 */
template <>
struct TListTypeTraits<UE::EditorDataStorage::UIRowType>
{
	typedef UE::EditorDataStorage::UIRowType NullableType;

	using MapKeyFuncs = TDefaultMapHashableKeyFuncs<UE::EditorDataStorage::UIRowType, TSharedRef<ITableRow>, false>;
	using MapKeyFuncsSparse = TDefaultMapHashableKeyFuncs<UE::EditorDataStorage::UIRowType, FSparseItemInfo, false>;
	using SetKeyFuncs = DefaultKeyFuncs<UE::EditorDataStorage::UIRowType>;

	template<typename U>
	static void AddReferencedObjects(FReferenceCollector&,
		TArray<UE::EditorDataStorage::UIRowType>&,
		TSet<UE::EditorDataStorage::UIRowType>&,
		TMap<const U*, UE::EditorDataStorage::UIRowType>&)
	{
	}

	static bool IsPtrValid(const UE::EditorDataStorage::UIRowType& InPtr)
	{
		return InPtr != TypedElementDataStorage::InvalidRowHandle;
	}

	static void ResetPtr(UE::EditorDataStorage::UIRowType& InPtr)
	{
		InPtr = TypedElementDataStorage::InvalidRowHandle;
	}

	static UE::EditorDataStorage::UIRowType MakeNullPtr()
	{
		return TypedElementDataStorage::InvalidRowHandle;
	}

	static UE::EditorDataStorage::UIRowType NullableItemTypeConvertToItemType(const UE::EditorDataStorage::UIRowType& InPtr)
	{
		return InPtr;
	}

	static FString DebugDump(UE::EditorDataStorage::UIRowType InPtr)
	{
		return FString::Printf(TEXT("%llu"), InPtr.RowHandle);
	}

	class SerializerType {};
};

// Template declaration to enable using row handles inside of slate widgets like SListView
template <>
struct TIsValidListItem<UE::EditorDataStorage::UIRowType>
{
	enum
	{
		Value = true
	};
};