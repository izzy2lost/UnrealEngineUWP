// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/AttributeDefinitions/DMXGDTFAttribute.h"

#include "GDTF/AttributeDefinitions/DMXGDTFAttributeDefinitions.h"
#include "GDTF/AttributeDefinitions/DMXGDTFSubphysicalUnit.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFAttribute::FDMXGDTFAttribute(const TSharedRef<FDMXGDTFAttributeDefinitions>& InAttributeDefinitions)
		: OuterAttributeDefinitions(InAttributeDefinitions)
	{}

	void FDMXGDTFAttribute::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name)
			.GetAttribute(TEXT("Pretty"), Pretty)
			.GetAttribute(TEXT("PhysicalUnit"), PhysicalUnit)
			.GetAttribute(TEXT("ActivationGroup"), ActivationGroup)
			.GetAttribute(TEXT("Feature"), Feature)
			.GetAttribute(TEXT("MainAttribute"), MainAttribute)
			.GetAttribute(TEXT("Color"), Color)
			.CreateChildren(TEXT("SubphysicalUnit"), SubpyhsicalUnitArray);
	}

	TSharedPtr<FDMXGDTFActivationGroup> FDMXGDTFAttribute::ResolveActivationGroup() const
	{
		if (const TSharedPtr<FDMXGDTFAttributeDefinitions> AttributeDefinitions = OuterAttributeDefinitions.Pin())
		{
			return AttributeDefinitions->FindActivationGroup(ActivationGroup);
		}

		return nullptr;
	}

	TSharedPtr<FDMXGDTFFeature> FDMXGDTFAttribute::ResolveFeature() const
	{
		if (const TSharedPtr<FDMXGDTFAttributeDefinitions> AttributeDefinitions = OuterAttributeDefinitions.Pin())
		{
			TArray<FString> Link;
			Feature.ParseIntoArray(Link, TEXT("."));
			if (Link.Num() == 2)
			{
				return AttributeDefinitions->FindFeature(Link[0], Link[1]);
			}
		}

		return nullptr;
	}

	TSharedPtr<FDMXGDTFAttribute> FDMXGDTFAttribute::ResolveMainAttribute() const
	{
		if (const TSharedPtr<FDMXGDTFAttributeDefinitions> AttributeDefinitions = OuterAttributeDefinitions.Pin())
		{
			return AttributeDefinitions->FindAttribute(MainAttribute);
		}

		return nullptr;
	}
}
