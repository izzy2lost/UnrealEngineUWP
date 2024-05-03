// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FFieldVariant;
class FProperty;
class UClass;
class UObject;
class UStruct;

namespace UE { class FPropertyPathNameTree; }

namespace UE
{

/**
 * Query if InstanceDataObject support is enabled for a specific object.
 *
 * Pass nullptr to query if the system is enabled.
 */
bool IsInstanceDataObjectSupportEnabled(UObject* Object = nullptr);

/** Generate a UClass that contains the union of the properties of PropertyTree and OwnerClass. */
UClass* CreateInstanceDataObjectClass(const FPropertyPathNameTree* PropertyTree, UClass* OwnerClass, UObject* Outer);

/** Notify that a property in an struct was set when the struct was deserialized. */
void MarkPropertySetBySerialization(const UStruct* Struct, void* StructData, const FProperty* Property, int32 ArrayIndex = 0);
/** Query whether a property in the struct was set when the struct was deserialized. */
bool WasPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex = 0);
/** Copy whether each property was set by serialization from one IDO to another. */
void CopyPropertySetBySerializationData(const FFieldVariant& OldField, void* OldDataPtr, const FFieldVariant& NewField, void* NewDataPtr);

} // UE
