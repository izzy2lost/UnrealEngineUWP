// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class IPropertyHandle;
class IDetailLayoutBuilder;
class FName;

/** Helper Class for CVD Custom Details view */
class FChaosVDDetailsCustomizationUtils
{
public:
	/**
	 * Hides all categories of this view, except the ones provided in the Allowed Categories set
	 * @param DetailBuilder Layout builder of the class we are customizing
	 * @param AllowedCategories Set of category names we do not want to hide 
	 */
	static void HideAllCategories(IDetailLayoutBuilder& DetailBuilder, const TSet<FName>& AllowedCategories = TSet<FName>());

	/**
	 * Marks any property of the provided handles array as hidden if they are not valid CVD properties (meaning they don't have serialized data loaded from a CVD recording)
	 * @param InPropertyHandles Handles of properties to evaluate and hide if needed
	 */
	static void HideInvalidParticleDataProperties(TConstArrayView<TSharedPtr<IPropertyHandle>> InPropertyHandles);
};
