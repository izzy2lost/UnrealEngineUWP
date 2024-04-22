// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/DMXModes/DMXGDTFChannelSet.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFChannelSet::FDMXGDTFChannelSet(const TSharedRef<FDMXGDTFChannelFunction>& InChannelFunction)
		: OuterChannelFunction(InChannelFunction)
	{}

	void FDMXGDTFChannelSet::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name)
			.GetAttribute(TEXT("DMXFrom"), DMXFrom)
			.GetAttribute(TEXT("PhysicalFrom"), PhysicalFrom)
			.GetAttribute(TEXT("PhysicalTo"), PhysicalTo)
			.GetAttribute(TEXT("WheelSlotIndex"), WheelSlotIndex);
	}
}
