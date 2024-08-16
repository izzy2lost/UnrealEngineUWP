// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Framework/Views/TableViewTypeTraits.h"

namespace UE::Editor::DataStorage
{
	// Wrapper struct around RowHandle so we can template specialize for it without also specializing for uint64
	struct UIRowType
	{
		RowHandle Row;
		
		UIRowType()
			: Row(InvalidRowHandle)
		{}

		UIRowType(RowHandle InRowHandle)
			: Row(InRowHandle)
		{}

		operator RowHandle() const
		{
			return Row;
		}

		UIRowType& operator=(RowHandle InRowHandle)
		{
			Row = InRowHandle;
			return *this;
		}
		
		friend uint32 GetTypeHash(const UIRowType& Key)
		{
			return GetTypeHash(Key.Row);
		}

	};
}

/* Template declaration to describe how a row handle behaves as a type for slate widgets like SListView, STreeView etc
 * This allows you to use Row Handles with slate widgets that work on pointers by using the wrapper struct e.g
 * SListView<UE::EditorDataStorage::UIRowType>
 */
template <>
struct TListTypeTraits<UE::Editor::DataStorage::UIRowType>
{
	using NullableType = UE::Editor::DataStorage::UIRowType;

	using MapKeyFuncs = TDefaultMapHashableKeyFuncs<UE::Editor::DataStorage::UIRowType, TSharedRef<ITableRow>, false>;
	using MapKeyFuncsSparse = TDefaultMapHashableKeyFuncs<UE::Editor::DataStorage::UIRowType, FSparseItemInfo, false>;
	using SetKeyFuncs = DefaultKeyFuncs<UE::Editor::DataStorage::UIRowType>;

	template<typename U>
	static void AddReferencedObjects(FReferenceCollector&,
		TArray<UE::Editor::DataStorage::UIRowType>&,
		TSet<UE::Editor::DataStorage::UIRowType>&,
		TMap<const U*, UE::Editor::DataStorage::UIRowType>&)
	{
	}

	static bool IsPtrValid(const UE::Editor::DataStorage::UIRowType& InPtr)
	{
		return InPtr != UE::Editor::DataStorage::InvalidRowHandle;
	}

	static void ResetPtr(UE::Editor::DataStorage::UIRowType& InPtr)
	{
		InPtr = UE::Editor::DataStorage::InvalidRowHandle;
	}

	static UE::Editor::DataStorage::UIRowType MakeNullPtr()
	{
		return UE::Editor::DataStorage::InvalidRowHandle;
	}

	static UE::Editor::DataStorage::UIRowType NullableItemTypeConvertToItemType(const UE::Editor::DataStorage::UIRowType& InPtr)
	{
		return InPtr;
	}

	static FString DebugDump(UE::Editor::DataStorage::UIRowType InPtr)
	{
		return FString::Printf(TEXT("%llu"), InPtr.Row);
	}

	class SerializerType {};
};

// Template declaration to enable using row handles inside of slate widgets like SListView
template <>
struct TIsValidListItem<UE::Editor::DataStorage::UIRowType>
{
	enum
	{
		Value = true
	};
};
