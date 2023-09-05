// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

namespace PropertyEditorUtils
{
	/**
	 * Calculates the possible drop-down options for the specified property path
	 * @param	InOutContainers		The container objects to resolve the property path against
	 * @param	InOutPropertyPath	The property path
	 * @param	InOutOptions		The resulting options
	 */
	PROPERTYEDITOR_API void GetPropertyOptions(TArray<UObject*>& InOutContainers, FString& InOutPropertyPath, TArray<TSharedPtr<FString>>& InOutOptions);
}
