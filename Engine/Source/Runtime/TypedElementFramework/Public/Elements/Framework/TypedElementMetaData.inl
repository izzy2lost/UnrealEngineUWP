// Copyright Epic Games, Inc. All Rights Reserved.

namespace TypedElementDataStorage
{
	namespace Internal
	{
		template<typename TContainer, typename T>
		void AddDataToContainer(TContainer& Container, FName Name, T Value)
		{
			using BaseType = std::remove_const_t<std::remove_reference_t<T>>;

			if constexpr (std::is_arithmetic_v<BaseType>)
			{
				if constexpr (std::is_integral_v<BaseType>)
				{
					if constexpr (std::is_signed_v<BaseType>)
					{
						Container.Add(Name, MetaDataType(TInPlaceType<int64>(), static_cast<int64>(Value)));
					}
					else
					{
						Container.Add(Name, MetaDataType(TInPlaceType<uint64>(), static_cast<uint64>(Value)));
					}
				}
				else if constexpr (std::is_floating_point_v<BaseType>)
				{
					Container.Add(Name, MetaDataType(TInPlaceType<double>(), static_cast<double>(Value)));
				}
				else
				{
					Container.Add(Name, MetaDataType(TInPlaceType<bool>(), static_cast<bool>(Value)));
				}
			}
			else if constexpr (std::is_convertible_v<BaseType, FString>)
			{
				Container.Add(Name, MetaDataType(TInPlaceType<FString>(), Forward<T>(Value)));
			}
			else
			{
				// !std::is_same_v<T, T> is used to force evaluation of the type so it's not always called, which would happen if "false" is used instead.
				static_assert(!std::is_same_v<T, T>, "Provided meta data type can't be stored in the Typed Element Data Storage.");
			}
		}
	}

	ENUM_CLASS_FLAGS(TypedElementDataStorage::FColumnMetaData::EFlags)

		template<typename T>
	bool FMetaDataBase::AddImmutableData(FName Name, T&& Value)
	{
		if (!ImmutableData.Contains(Name))
		{
			Internal::AddDataToContainer(ImmutableData, Name, Forward<T>(Value));
			return true;
		}
		else
		{
			return false;
		}
	}

	template<typename T>
	void FMetaDataBase::AddOrSetMutableData(FName Name, T&& Value)
	{
		Internal::AddDataToContainer(MutableData, Name, Forward<T>(Value));
	}



	//
	// FMetaDataEntryView
	//

	template<typename T>
	FMetaDataEntryView::FMetaDataEntryView(T&& MetaDataValue)
		: DataView(TInPlaceType<T>(), Forward<T>(MetaDataValue))
	{}

	template<typename T>
	bool FMetaDataEntryView::IsType() const
	{
		return DataView.IsType<T>();
	}

	template<typename T>
	const T* FMetaDataEntryView::TryGetExact() const
	{
		if constexpr (std::is_same_v<T, const FString&>)
		{
			return DataView.TryGet<const FString*>();
		}
		return DataView.TryGet<T>();
	}



	//
	// FMetaDataView
	//

	template<typename Column>
	FMetaDataEntryView FMetaDataView::FindForColumn(FName AttributeName, ESearchScope Scope) const
	{
		return FindForColumn(Column::StaticStruct(), AttributeName, Scope);
	}
} // namespace TypedElementDataStorage
