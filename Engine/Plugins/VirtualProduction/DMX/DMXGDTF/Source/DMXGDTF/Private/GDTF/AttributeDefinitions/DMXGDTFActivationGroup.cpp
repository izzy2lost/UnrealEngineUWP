// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/AttributeDefinitions/DMXGDTFActivationGroup.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFActivationGroup::FDMXGDTFActivationGroup(const TSharedRef<FDMXGDTFAttributeDefinitions>& InAttributeDefinitions)
		: OuterAttributeDefinitions(InAttributeDefinitions)
	{}

	void FDMXGDTFActivationGroup::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name);
	}
}
