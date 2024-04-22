// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/DMXModes/DMXGDTFChannelFunction.h"

#include "Algo/Find.h"
#include "GDTF/AttributeDefinitions/DMXGDTFAttributeDefinitions.h"
#include "GDTF/DMXGDTFFixtureType.h"
#include "GDTF/DMXModes/DMXGDTFChannelSet.h"
#include "GDTF/DMXModes/DMXGDTFDMXChannel.h"
#include "GDTF/DMXModes/DMXGDTFDMXMode.h"
#include "GDTF/DMXModes/DMXGDTFLogicalChannel.h"
#include "GDTF/DMXModes/DMXGDTFSubchannelSet.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFColorSpace.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFDMXProfile.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFEmitter.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFFilter.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFGamut.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFPhysicalDescriptions.h"
#include "GDTF/Wheels/DMXGDTFWheel.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFChannelFunction::FDMXGDTFChannelFunction(const TSharedRef<FDMXGDTFLogicalChannel>& InLogicalChannel)
		: OuterLogicalChannel(InLogicalChannel)
	{}

	void FDMXGDTFChannelFunction::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name)
			.GetAttribute(TEXT("Attribute"), Attribute)
			.GetAttribute(TEXT("OriginalAttribute"), OriginalAttribute)
			.GetAttribute(TEXT("DMXFrom"), DMXFrom)
			.GetAttribute(TEXT("Default"), Default)
			.GetAttribute(TEXT("PhysicalFrom"), PhysicalFrom)
			.GetAttribute(TEXT("PhysicalTo"), PhysicalTo)
			.GetAttribute(TEXT("RealFade"), RealFade)
			.GetAttribute(TEXT("RealAcceleration"), RealAcceleration)
			.GetAttribute(TEXT("Wheel"), Wheel)
			.GetAttribute(TEXT("Emitter"), Emitter)
			.GetAttribute(TEXT("Filter"), Filter)
			.GetAttribute(TEXT("ColorSpace"), ColorSpace)
			.GetAttribute(TEXT("Gamut"), Gamut)
			.GetAttribute(TEXT("ModeMaster"), ModeMaster)
			.GetAttribute(TEXT("ModeFrom"), ModeFrom)
			.GetAttribute(TEXT("ModeTo"), ModeTo)
			.GetAttribute(TEXT("DMXProfile"), DMXProfile)
			.GetAttribute(TEXT("Min"), Min)
			.GetAttribute(TEXT("Max"), Max)
			.CreateChildren(TEXT("ChannelSet"), ChannelSetArray)
			.CreateChildren(TEXT("SubchannelSet"), SubchannelSetArray);
	}

	TSharedPtr<FDMXGDTFAttribute> FDMXGDTFChannelFunction::ResolveAttribute() const
	{
		const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin();
		const TSharedPtr<FDMXGDTFAttributeDefinitions> AttributeDefinitions = FixtureType.IsValid() ? FixtureType->AttributeDefinitions : nullptr;
		
		return AttributeDefinitions.IsValid() ? AttributeDefinitions->FindAttribute(Attribute) : nullptr;
	}

	TSharedPtr<FDMXGDTFWheel> FDMXGDTFChannelFunction::ResolveWheel() const
	{
		if (const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin())
		{
			const TSharedPtr<FDMXGDTFWheel>* WheelPtr = Algo::FindBy(FixtureType->Wheels, Wheel, &FDMXGDTFWheel::Name);
			if (WheelPtr)
			{
				return *WheelPtr;
			}
		}

		return nullptr;
	}

	TSharedPtr<FDMXGDTFEmitter> FDMXGDTFChannelFunction::ResolveEmitter() const
	{
		const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin();
		const TSharedPtr<FDMXGDTFPhysicalDescriptions> PhysicalDescriptions = FixtureType.IsValid() ? FixtureType->PhysicalDescriptions : nullptr;
		if (PhysicalDescriptions.IsValid())
		{
			if (const TSharedPtr<FDMXGDTFEmitter>* EmitterPtr = Algo::FindBy(PhysicalDescriptions->Emitters, Emitter, &FDMXGDTFEmitter::Name))
			{
				return *EmitterPtr;
			}
		}
		return nullptr;
	}

	TSharedPtr<FDMXGDTFFilter> FDMXGDTFChannelFunction::ResolveFilter() const
	{
		const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin();
		const TSharedPtr<FDMXGDTFPhysicalDescriptions> PhysicalDescriptions = FixtureType.IsValid() ? FixtureType->PhysicalDescriptions : nullptr;
		if (PhysicalDescriptions.IsValid())
		{
			if (const TSharedPtr<FDMXGDTFFilter>* FilterPtr = Algo::FindBy(PhysicalDescriptions->Filters, Filter, &FDMXGDTFFilter::Name))
			{
				return *FilterPtr;
			}
		}
		return nullptr;
	}

	TSharedPtr<FDMXGDTFColorSpace> FDMXGDTFChannelFunction::ResolveColorSpace() const
	{
		const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin();
		const TSharedPtr<FDMXGDTFPhysicalDescriptions> PhysicalDescriptions = FixtureType.IsValid() ? FixtureType->PhysicalDescriptions : nullptr;
		if (PhysicalDescriptions.IsValid())
		{
			if (const TSharedPtr<FDMXGDTFColorSpace>* ColorSpacePtr = Algo::FindBy(PhysicalDescriptions->ColorSpaces, ColorSpace, &FDMXGDTFColorSpace::Name))
			{
				return *ColorSpacePtr;
			}
		}
		return nullptr;
	}

	TSharedPtr<FDMXGDTFGamut> FDMXGDTFChannelFunction::ResolveGamut() const
	{
		const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin();
		const TSharedPtr<FDMXGDTFPhysicalDescriptions> PhysicalDescriptions = FixtureType.IsValid() ? FixtureType->PhysicalDescriptions : nullptr;
		if (PhysicalDescriptions.IsValid())
		{
			if (const TSharedPtr<FDMXGDTFGamut>* GamutPtr = Algo::FindBy(PhysicalDescriptions->Gamuts, Gamut, &FDMXGDTFGamut::Name))
			{
				return *GamutPtr;
			}
		}
		return nullptr;
	}

	TSharedPtr<FDMXGDTFDMXProfile> FDMXGDTFChannelFunction::ResolveDMXProfile() const
	{
		const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin();
		const TSharedPtr<FDMXGDTFPhysicalDescriptions> PhysicalDescriptions = FixtureType.IsValid() ? FixtureType->PhysicalDescriptions : nullptr;
		if (PhysicalDescriptions.IsValid())
		{
			if (const TSharedPtr<FDMXGDTFDMXProfile>* DMXProfilePtr = Algo::FindBy(PhysicalDescriptions->DMXProfiles, DMXProfile, &FDMXGDTFDMXProfile::Name))
			{
				return *DMXProfilePtr;
			}
		}
		return nullptr;
	}

	void FDMXGDTFChannelFunction::ResolveModeMaster(TSharedPtr<FDMXGDTFDMXChannel>& OutDMXChannel, TSharedPtr<FDMXGDTFChannelFunction>& OutChannelFunction) const
	{
		const TSharedPtr<FDMXGDTFLogicalChannel> LogicalChannel = OuterLogicalChannel.Pin();
		const TSharedPtr<FDMXGDTFDMXChannel> DMXChannel = LogicalChannel.IsValid() ? LogicalChannel->OuterDMXChannel.Pin() : nullptr;
		const TSharedPtr<FDMXGDTFDMXMode> DMXMode = DMXChannel.IsValid() ? DMXChannel->OuterDMXMode.Pin() : nullptr;
		if (DMXMode.IsValid())
		{
			return DMXMode->ResolveChannel(ModeMaster, OutDMXChannel, OutChannelFunction);
		}
	}

	void FDMXGDTFChannelFunction::ResolveModePrimary(TSharedPtr<FDMXGDTFDMXChannel>& OutDMXChannel, TSharedPtr<FDMXGDTFChannelFunction>& OutChannelFunction) const
	{
		ResolveModeMaster(OutDMXChannel, OutChannelFunction);
	}
}
