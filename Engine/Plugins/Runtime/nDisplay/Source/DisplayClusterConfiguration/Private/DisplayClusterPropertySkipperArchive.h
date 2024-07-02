// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/ArchiveProxy.h"
#include "UObject/Object.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"


/**
* This is a custom archive to address the issues around instanced property propagation (UE-211513).
* It skips loading the given property by name unless it is the archetype, such that the instances always get the 
* archetype default values.
*/
class FDisplayClusterPropertySkipperArchive : public FArchiveProxy
{

public:

	FDisplayClusterPropertySkipperArchive(FArchive& InInnerArchive)
		: FArchiveProxy(InInnerArchive)
	{
	}

	//~ Begin FArchive interface
	virtual bool ShouldSkipProperty(const FProperty* Property) const override
	{
		// Skip loading the property unless it is the archetype.

		bool bShouldSkip = false;

		if (IsLoading() && PropertiesToSkip.Contains(MakePropertyData(Property)))
		{
			const FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext();
			const UObject* SerializedObject = SerializeContext ? SerializeContext->SerializedObject : nullptr;

			// We assume that if the SerializedObject is null, it is indicative of a template.
			const bool bIsArchetype = SerializedObject ? SerializedObject->IsTemplate() : true;

			bShouldSkip |= !bIsArchetype;
		}

		return bShouldSkip || FArchiveProxy::ShouldSkipProperty(Property);
	}
	//~ End FArchive interface

	/** Adds a new property to skip. */
	bool AddPropertyToSkip(UStruct* Struct, FName PropertyName)
	{
		if (const FProperty* Property = FindFProperty<FProperty>(Struct, PropertyName)) // FindFProperty handles null Structs.
		{
			PropertiesToSkip.Add(MakePropertyData(Property));
			return true;
		}

		return false;
	}

private:

	/** Creates the data needed for a property when matching properties to be skipped */
	TPair<FName, FName> MakePropertyData(const FProperty* Property) const
	{
		if (!Property)
		{
			return TPair<FName, FName>(NAME_None, NAME_None);
		}

		const FName PropertyOwnerName = Property->GetOwnerUField() ? Property->GetOwnerUField()->GetFName() : NAME_None;

		return TPair<FName, FName>(PropertyOwnerName, Property->GetName());
	}

private:

	/** Names of properties to skip except for archetypes */
	TSet<TPair<FName,FName>> PropertiesToSkip;
};