// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFLaserGeometry.h"

#include "GDTF/Geometries/DMXGDTFLaserProtocol.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFEmitter.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFPhysicalDescriptions.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	void FDMXGDTFLaserGeometry::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFGeometry::Initialize(XmlNode);

		using namespace UE::DMX::GDTF;

		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("ColorType"), ColorType)
			.GetAttribute(TEXT("Color"), Color)
			.GetAttribute(TEXT("OutputStrength"), OutputStrength)
			.GetAttribute(TEXT("BeamDiameter"), BeamDiameter)
			.GetAttribute(TEXT("Emitter"), Emitter)
			.GetAttribute(TEXT("BeamDivergenceMin"), BeamDivergenceMin)
			.GetAttribute(TEXT("BeamDivergenceMax"), BeamDivergenceMax)
			.GetAttribute(TEXT("ScanAnglePan"), ScanAnglePan)
			.GetAttribute(TEXT("ScanAngleTilt"), ScanAngleTilt)
			.GetAttribute(TEXT("ScanSpeed"), ScanSpeed)
			.CreateChildren(TEXT("Protocol"), ProtocolArray);
	}

	TSharedPtr<FDMXGDTFEmitter> FDMXGDTFLaserGeometry::ResolveEmitter() const
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
}
