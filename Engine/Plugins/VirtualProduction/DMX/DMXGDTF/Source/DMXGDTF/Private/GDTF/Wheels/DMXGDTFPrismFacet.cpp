// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Wheels/DMXGDTFPrismFacet.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFPrismFacet::FDMXGDTFPrismFacet(const TSharedRef<FDMXGDTFWheelSlot>& InWheelSlot)
		: OuterWheelSlot(InWheelSlot)
	{}

	void FDMXGDTFPrismFacet::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Color"), Color)
			.GetAttribute(TEXT("Rotation"), Rotation);
	}
}
