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
	UClass* CreatePropertyBagArchetypeClass(const FPropertyBag* PropertyBag, UStruct* TemplateStruct, UObject* Outer);

	// notify that a property in an object was set when the object was deserialized
	void MarkPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path);
	
	// query whether a property in an object was set when the object was deserialized
	bool WasPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path);
	// query whether a property in Struct was set when the struct was deserialized
	bool WasPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex = 0);
}
