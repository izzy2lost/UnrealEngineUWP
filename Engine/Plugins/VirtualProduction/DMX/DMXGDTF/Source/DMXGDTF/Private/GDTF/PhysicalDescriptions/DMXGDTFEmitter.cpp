// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/PhysicalDescriptions/DMXGDTFEmitter.h"

#include "GDTF/PhysicalDescriptions/DMXGDTFMeasurement.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFEmitter::FDMXGDTFEmitter(const TSharedRef<FDMXGDTFPhysicalDescriptions>& InPhysicalDescriptions)
		: OuterPhysicalDescriptions(InPhysicalDescriptions)
	{}

	void FDMXGDTFEmitter::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name)
			.GetAttribute(TEXT("ColorCIE"), ColorCIE)
			.GetAttribute(TEXT("DominantWaveLength"), DominantWaveLength)
			.GetAttribute(TEXT("DiodePart"), DiodePart)
			.CreateChildren(TEXT("Measurement"), Measurements);
	}
}
