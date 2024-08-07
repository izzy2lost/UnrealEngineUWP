// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Elements/Common/TypedElementHandles.h"

#include "TypedElementCommonTypes.generated.h"

/**
 * Base for the data structures for a column.
 */
USTRUCT()
struct FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

/**
 * Base for the data structures that act as tags to rows. Tags should not have any data.
 */
USTRUCT()
struct FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

namespace UE
{
	// Work around missing header/implementations on some platforms
	namespace detail
	{
		template<typename T, typename U>
		concept SameHelper = std::is_same_v<T, U>;
	}
	template<typename T, typename U>
	concept same_as = detail::SameHelper<T, U> && detail::SameHelper<U, T>;

	template<typename From, typename To>
	concept convertible_to = std::is_convertible_v<From, To> && requires { static_cast<To>(std::declval<From>()); };

	template<typename Derived, typename Base>
	concept derived_from = std::is_base_of_v<Base, Derived> && std::is_convertible_v<const volatile Derived*, const volatile Base*>;
}

namespace UE
{
	namespace Editor::DataStorage
	{
		/**
		 * Defines a dynamic type for a dynamic tag
		 * Example:
		 *   FDynamicTag ColorTagType(TEXT("Color"));
		 *   FDynamicTag DirectionTagType(TEXT("Direction"));
		 * A dynamic tag can take on different values for each type.  This is set up when a tag is added to a row.
		 */
		class FDynamicTag
		{
		public:
			explicit FDynamicTag(const FName& InName);
			
			const FName& GetName() const;
			bool operator==(const FDynamicTag& Other) const;
		private:
			friend uint32 GetTypeHash(const FDynamicTag& InName)
			{
				return GetTypeHash(InName.Name);
			}
			FName Name;
		};

		inline FDynamicTag::FDynamicTag(const FName& InTypeName)
			: Name(InTypeName)
		{}

		inline const FName& FDynamicTag::GetName() const
		{
			return Name;
		}

		inline bool FDynamicTag::operator==(const FDynamicTag& Other) const
		{
			return Other.Name == Name;
		}

	}
}



namespace TypedElementDataStorage
{
	// Standard callbacks.

	using RowCreationCallbackRef = TFunctionRef<void(TypedElementDataStorage::RowHandle Row)>;
	using ColumnCreationCallbackRef = TFunctionRef<void(void* Column, const UScriptStruct& ColumnType)>;
	using ColumnListCallbackRef = TFunctionRef<void(const UScriptStruct& ColumnType)>;
	using ColumnListWithDataCallbackRef = TFunctionRef<void(void* Column, const UScriptStruct& ColumnType)>;
	using ColumnCopyOrMoveCallback = void (*)(const UScriptStruct& ColumnType, void* Destination, void* Source);



	// Template concepts to enforce type correctness.

	template<typename T>
	concept TDataColumnType = UE::derived_from<T, FTypedElementDataStorageColumn>;

	template<typename T>
	concept TTagColumnType = UE::derived_from<T, FTypedElementDataStorageTag>;

	template<typename T>
	concept TColumnType = TDataColumnType<T> || TTagColumnType<T>;

	template<typename T>
	concept TEnumType = std::is_enum_v<T>;

} // namespace TypedElementDataStorage