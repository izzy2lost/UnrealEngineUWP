// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/DMXGDTFFixtureType.h"

#include "GDTF/AttributeDefinitions/DMXGDTFAttributeDefinitions.h"
#include "GDTF/DMXModes/DMXGDTFDMXMode.h"
#include "GDTF/Geometries/DMXGDTFGeometryCollect.h"
#include "GDTF/Models/DMXGDTFModel.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFPhysicalDescriptions.h"
#include "GDTF/Protocols/DMXGDTFProtocols.h"
#include "GDTF/Revisions/DMXGDTFRevision.h"
#include "GDTF/Wheels/DMXGDTFWheel.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	void FDMXGDTFFixtureType::Initialize(const FXmlNode& InXmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), InXmlNode)
			.GetAttribute(TEXT("Name"), Name)
			.GetAttribute(TEXT("ShortName"), ShortName)
			.GetAttribute(TEXT("LongName"), LongName)
			.GetAttribute(TEXT("Manufacturer"), Manufacturer)
			.GetAttribute(TEXT("Description"), Description)
			.GetAttribute(TEXT("FixtureTypeID"), FixtureTypeID)
			.GetAttribute(TEXT("Thumbnail"), Thumbnail)
			.GetAttribute(TEXT("ThumbnailOffsetX"), ThumbnailOffsetX)
			.GetAttribute(TEXT("ThumbnailOffsetY"), ThumbnailOffsetY)
			.GetAttribute(TEXT("RefFT"), RefFT)
			.GetAttribute(TEXT("CanHaveChildren"), bCanHaveChildren,
				[](const FString& StringValue)
				{
					// Parse as boolean. If field is empty, default to true.
					return StringValue.IsEmpty() || StringValue == TEXT("Yes");
				})
			.CreateRequiredChild(TEXT("AttributeDefinitions"), AttributeDefinitions)
			.CreateChildCollection(TEXT("Wheels"), TEXT("Wheel"), Wheels)
			.CreateOptionalChild(TEXT("PhysicalDescriptions"), PhysicalDescriptions)
			.CreateChildCollection(TEXT("Models"), TEXT("Model"), Models)
			.CreateOptionalChild(TEXT("Geometries"), GeometryCollect)
			.CreateChildCollection(TEXT("DMXModes"), TEXT("DMXMode"), DMXModes)
			.CreateChildCollection(TEXT("Revisions"), TEXT("Revision"), Revisions)
			.CreateOptionalChild(TEXT("Protocols"), Protocols);
	}
}
