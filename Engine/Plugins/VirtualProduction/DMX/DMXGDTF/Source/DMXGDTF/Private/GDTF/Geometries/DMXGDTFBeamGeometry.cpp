// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFBeamGeometry.h"

#include "Algo/Find.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFEmitter.h"
#include "GDTF/PhysicalDescriptions/DMXGDTFPhysicalDescriptions.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	void FDMXGDTFBeamGeometry::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFGeometry::Initialize(XmlNode);

		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("LampType"), LampType)
			.GetAttribute(TEXT("PowerConsumption"), PowerConsumption)
			.GetAttribute(TEXT("LuminousFlux"), LuminousFlux)
			.GetAttribute(TEXT("ColorTemperature"), ColorTemperature)
			.GetAttribute(TEXT("BeamAngle"), BeamAngle)
			.GetAttribute(TEXT("FieldAnge"), FieldAngle)
			.GetAttribute(TEXT("ThrowRatio"), ThrowRatio)
			.GetAttribute(TEXT("RectangleRatio"), RectangleRatio)
			.GetAttribute(TEXT("BeamRadius"), BeamRadius)
			.GetAttribute(TEXT("LampType"), BeamType)
			.GetAttribute(TEXT("ColorRenderingIndex"), ColorRenderingIndex)
			.GetAttribute(TEXT("EmitterSpectrum"), EmitterSpectrum);
	}

	TSharedPtr<FDMXGDTFEmitter> FDMXGDTFBeamGeometry::ResolveEmitterSpectrum() const
	{
		const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin();
		const TSharedPtr<FDMXGDTFPhysicalDescriptions> PhysicalDescriptions = FixtureType.IsValid() ? FixtureType->PhysicalDescriptions : nullptr;
		if (PhysicalDescriptions.IsValid())
		{
			if (const TSharedPtr<FDMXGDTFEmitter>* EmitterPtr = Algo::FindBy(PhysicalDescriptions->Emitters, EmitterSpectrum, &FDMXGDTFEmitter::Name))
			{
				return *EmitterPtr;
			}
		}
		return nullptr;
	}
}
