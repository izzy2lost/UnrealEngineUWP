// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"

namespace TypedElementDataStorage
{
	template<typename T>
	IndexHash GenerateIndexHash(const T* Object);
	template<typename T>
	IndexHash GenerateIndexHash(T* Object);
} // namespace TypedElementDataStorage

#include "Elements/Framework/TypedElementIndexHasher.inl"