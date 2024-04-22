// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/PhysicalDescriptions/DMXGDTFPhysicalDescriptions.h"

#include "GDTF/PhysicalDescriptions/DMXGDTFColorRenderingIndexGroup.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFColorSpace.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFDMXProfile.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFEmitter.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFFilter.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFGamut.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFProperties.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFPhysicalDescriptions::FDMXGDTFPhysicalDescriptions(const TSharedRef<FDMXGDTFFixtureType>& InFixtureType)
		: OuterFixtureType(InFixtureType)
	{}

	void FDMXGDTFPhysicalDescriptions::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.CreateChildCollection(TEXT("Emitters"), TEXT("Emitter"), Emitters)
			.CreateChildCollection(TEXT("Filters"), TEXT("Filter"), Filters)
			.CreateChildCollection(TEXT("ColorSpaces"), TEXT("ColorSpace"), ColorSpaces)
			.CreateChildCollection(TEXT("AdditionalColorSpaces"), TEXT("ColorSpace"), AdditionalColorSpaces)
			.CreateChildCollection(TEXT("Gamut"), TEXT("Gamuts"), Gamuts)
			.CreateChildCollection(TEXT("DMXProfiles"), TEXT("DMXProfile"), DMXProfiles)
			.CreateChildCollection(TEXT("CRIs"), TEXT("CRIGroup"), CRIs)
			.CreateOptionalChild(TEXT("Properties"), Properties);
	}
}
