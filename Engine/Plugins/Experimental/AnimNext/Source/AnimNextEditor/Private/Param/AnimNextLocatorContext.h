// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UniversalObjectLocatorEditorContext.h"
#include "Component/AnimNextComponent.h"

namespace UE::AnimNext
{

class FAnimNextLocatorContext : public UE::UniversalObjectLocator::ILocatorFragmentEditorContext
{
	// ILocatorFragmentEditorContext interface
	virtual UObject* GetContext(const IPropertyHandle& InPropertyHandle) const override
	{
		// TODO: This needs to defer to project/schedule/workspace defaults similar to SParameterPicker
		return UAnimNextComponent::StaticClass()->GetDefaultObject();
	}

	virtual bool IsFragmentAllowed(FName InFragmentName) const override
	{
		return (InFragmentName == "Actor" ||
				InFragmentName == "Asset" ||
				InFragmentName == "AnimNextScope" ||
				InFragmentName == "AnimNextGraph" ||
				InFragmentName == "AnimNextObjectFunction" ||
				InFragmentName == "AnimNextObjectProperty" ||
				InFragmentName == "AnimNextObjectCast");
	}
};

}
