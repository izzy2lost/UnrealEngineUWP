// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/FunctionFwd.h"
#include "HAL/Platform.h" // for 'should be preceded by' error related to API decorator

class UClass;
class FProperty;

namespace UE::Reflection
{
	/**
	 * Returns true if all properties in the provided class's sparse class data match the class's archetype (super class)
	 * Use the filter function to allow list properties that should be compared - if you want to compare all properties
	 * in the sparse class data simply return true in the filter, otherwise return false if the property should be skipped.
	 * Typically transient properties should be skipped, and when saving for a non editor target editor properties should
	 * be skipped. Consider relying on FProperty::ShouldSerializeProperty if you have an archiver available.
	 */
	COREUOBJECT_API bool CompareSparseClassDataToArchetype(const UClass* Class, const TFunctionRef<bool(FProperty*)>& Filter);
}
