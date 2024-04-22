// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFWiringObjectGeometry.h"

#include "Algo/Find.h"
#include "GDTF/Geometries/DMXGDTFWiringObjectPinPatch.h"
#include "GDTF/Models/DMXGDTFModel.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	void FDMXGDTFWiringObjectGeometry::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFGeometry::Initialize(XmlNode);

		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("ConnectorType"), ConnectorType)
			.GetAttribute(TEXT("Matrix"), Matrix)
			.GetAttribute(TEXT("ComponentType"), ComponentType)
			.GetAttribute(TEXT("SignalType"), SignalType)
			.GetAttribute(TEXT("PinCount"), PinCount)
			.GetAttribute(TEXT("ElectricalPayLoad"), ElectricalPayLoad)
			.GetAttribute(TEXT("VoltageRangeMax"), VoltageRangeMax)
			.GetAttribute(TEXT("VoltageRangeMin"), VoltageRangeMin)
			.GetAttribute(TEXT("FrequencyRangeMax"), FrequencyRangeMax)
			.GetAttribute(TEXT("FrequencyRangeMin"), FrequencyRangeMin)
			.GetAttribute(TEXT("MaxPayLoad"), MaxPayLoad)
			.GetAttribute(TEXT("Voltage"), Voltage)
			.GetAttribute(TEXT("SignalLayer"), SignalLayer)
			.GetAttribute(TEXT("CosPhi"), CosPhi)
			.GetAttribute(TEXT("FuseCurrent"), FuseCurrent)
			.GetAttribute(TEXT("FuseRating"), FuseRating)
			.GetAttribute(TEXT("Orientation"), Orientation)
			.GetAttribute(TEXT("WireGroup"), WireGroup)
			.CreateChildren(TEXT("PinPatch"), PinPatchArray);
	}

	TSharedPtr<FDMXGDTFModel> FDMXGDTFWiringObjectGeometry::ResolveModel() const
	{
		if (const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin())
		{
			const TSharedPtr<FDMXGDTFModel>* ModelPtr = Algo::FindBy(FixtureType->Models, Model, &FDMXGDTFModel::Name);
			if (ModelPtr)
			{
				return *ModelPtr;
			}
		}

		return nullptr;
	}
}
