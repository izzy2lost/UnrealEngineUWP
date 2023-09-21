// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <type_traits>
#include "UObject/Object.h"

namespace TypedElementDataStorage
{
	template<typename T>
	IndexHash GenerateIndexHash(const T* Object)
	{
		if constexpr (std::is_base_of_v<UObject, T>)
		{
			return Object->GetUniqueID();
		}
		else
		{
			return reinterpret_cast<IndexHash>(Object);
		}
	}

	template<typename T>
	IndexHash GenerateIndexHash(T* Object)
	{
		return GenerateIndexHash(const_cast<const T*>(Object));
	}
} // namespace TypedElementDataStorage
