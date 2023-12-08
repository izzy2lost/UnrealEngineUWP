// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"

class UClass;
class UObject;
class UStruct;
class FProperty;

namespace UE
{
	class FPropertyBag;
	class FPropertyPathName;

	// generate a UClass that unions the properties of PropertyBag and TemplateStruct
	COREUOBJECT_API UClass* CreatePropertyBagArchetypeClass(const FPropertyBag* PropertyBag, UStruct* TemplateStruct, UObject* Outer);

	// copy the values from a property bag to an archetype
	COREUOBJECT_API void CopyPropertyBagValuesToArchetype(const FPropertyBag* PropertyBag, const UObject* Destination);

	// notify that a property in an object was set when the object was deserialized
	COREUOBJECT_API void MarkPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path);
	// notify that a property in in Struct was set when the struct was deserialized
	COREUOBJECT_API void MarkPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex = 0);
	
	// query whether a property in an object was set when the object was deserialized
	COREUOBJECT_API bool WasPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path);
	// query whether a property in Struct was set when the struct was deserialized
	COREUOBJECT_API bool WasPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex = 0);
}

