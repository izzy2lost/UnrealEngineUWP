// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/AttributeDefinitions/DMXGDTFFeature.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFFeature::FDMXGDTFFeature(const TWeakPtr<FDMXGDTFFeatureGroup>& InFeatureGroup)
		: OuterFeatureGroup(InFeatureGroup)
	{}

	void FDMXGDTFFeature::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name);
	}
}
