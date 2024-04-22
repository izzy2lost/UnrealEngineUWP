// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/DMXModes/DMXGDTFDMXChannel.h"

#include "DMXGDTFLog.h"
#include "GDTF/DMXGDTFFixtureType.h"
#include "GDTF/DMXModes/DMXGDTFChannelFunction.h"
#include "GDTF/DMXModes/DMXGDTFDMXMode.h"
#include "GDTF/DMXModes/DMXGDTFDMXValue.h"
#include "GDTF/DMXModes/DMXGDTFLogicalChannel.h"
#include "GDTF/Geometries/DMXGDTFGeometry.h"
#include "GDTF/Geometries/DMXGDTFGeometryCollect.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFDMXChannel::FDMXGDTFDMXChannel(const TSharedRef<FDMXGDTFDMXMode>& InDMXMode)
		: OuterDMXMode(InDMXMode)
	{}

	void FDMXGDTFDMXChannel::Initialize(const FXmlNode& XmlNode)
	{
		constexpr bool bOnlyTopLevelGeoemtry = false;

		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("DMXBreak"), DMXBreak)
			.GetAttribute(TEXT("Offset"), Offset, this, &FDMXGDTFDMXChannel::ParseOffset)
			.GetAttribute(TEXT("InitialFunction"), InitialFunction)
			.GetAttribute(TEXT("DMXValue"), DMXValue)
			.GetAttribute(TEXT("Geometry"), Geometry)
			.CreateChildren(TEXT("LogicalChannel"), LogicalChannelArray);
	}

	TSharedPtr<FDMXGDTFChannelFunction> FDMXGDTFDMXChannel::ResolveInitialFunction() const
	{
		if (InitialFunction.IsEmpty() && !LogicalChannelArray.IsEmpty() && LogicalChannelArray[0].IsValid())
		{
			// Default value is the first channel function of the first logical function of this DMX channel.
			return LogicalChannelArray[0]->ChannelFunctionArray.IsEmpty() ? nullptr : LogicalChannelArray[0]->ChannelFunctionArray[0];
		}

		if (const TSharedPtr<FDMXGDTFDMXMode> DMXMode = OuterDMXMode.Pin())
		{
			TSharedPtr<FDMXGDTFDMXChannel> Dummy;
			TSharedPtr<FDMXGDTFChannelFunction> ChannelFunction;
			DMXMode->ResolveChannel(InitialFunction, Dummy, ChannelFunction);

			if (ChannelFunction.IsValid())
			{
				return ChannelFunction;
			}
			else if (!LogicalChannelArray.IsEmpty() && LogicalChannelArray[0].IsValid())
			{
				// Fall back to the default value if the link cannot be resolved
				return LogicalChannelArray[0]->ChannelFunctionArray.IsEmpty() ? nullptr : LogicalChannelArray[0]->ChannelFunctionArray[0];
			}
		}

		return nullptr;
	}

	TArray<uint32> FDMXGDTFDMXChannel::ParseOffset(const FString& GDTFString) const
	{
		TArray<FString> Substrings;
		GDTFString.ParseIntoArray(Substrings, TEXT(","));

		TArray<uint32> Result;
		for (const FString& Substring : Substrings)
		{
			uint32 Value;
			if (LexTryParseString(Value, *Substring))
			{
				Result.Add(Value);
			}
		}

		return Result;
	}
}
