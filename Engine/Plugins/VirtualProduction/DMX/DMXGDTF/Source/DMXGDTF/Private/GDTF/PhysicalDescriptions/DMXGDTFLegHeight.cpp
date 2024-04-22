// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/PhysicalDescriptions/DMXGDTFLegHeight.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFLegHeight::FDMXGDTFLegHeight(const TSharedRef<FDMXGDTFProperties>& InProperties)
		: OuterProperties(InProperties)
	{}

	void FDMXGDTFLegHeight::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Value"), Value);
	}
}
