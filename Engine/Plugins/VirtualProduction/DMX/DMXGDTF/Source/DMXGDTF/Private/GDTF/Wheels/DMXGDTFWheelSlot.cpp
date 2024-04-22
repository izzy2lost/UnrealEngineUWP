// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Wheels/DMXGDTFWheelSlot.h"

#include "GDTF/Wheels/DMXGDTFAnimationSystem.h"
#include "GDTF/Wheels/DMXGDTFPrismFacet.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFWheelSlot::FDMXGDTFWheelSlot(const TSharedRef<FDMXGDTFWheel>& InWheel)
		: OuterWheel(InWheel)
	{}

	void FDMXGDTFWheelSlot::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Color"), Color)
			.GetAttribute(TEXT("MediaFileName"), MediaFileName)
			.CreateChildren(TEXT("Facet"), PrismFacetArray)
			.CreateOptionalChild(TEXT("AnimationSystem"), AnimationWheel);
	}
}
