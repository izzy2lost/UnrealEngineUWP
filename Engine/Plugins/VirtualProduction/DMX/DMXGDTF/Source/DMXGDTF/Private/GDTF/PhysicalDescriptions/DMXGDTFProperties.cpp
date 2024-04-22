// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/PhysicalDescriptions/DMXGDTFProperties.h"

#include "GDTF/PhysicalDescriptions/DMXGDTFLegHeight.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFOperatingTemperature.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFWeight.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFProperties::FDMXGDTFProperties(const TSharedRef<FDMXGDTFPhysicalDescriptions>& InPhysicalDescriptions)
		: OuterPhysicalDescriptions(InPhysicalDescriptions)
	{}

	void FDMXGDTFProperties::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.CreateOptionalChild(TEXT("OperatingTemperature"), OperatingTemperature)
			.CreateOptionalChild(TEXT("Weigth"), Weigth)
			.CreateOptionalChild(TEXT("LegHeight"), LegHeight);
	}
}
