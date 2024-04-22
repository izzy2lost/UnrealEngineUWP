// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFGeometry.h"

#include "Algo/Find.h"
#include "GDTF/DMXGDTFFixtureType.h"
#include "GDTF/Models/DMXGDTFModel.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFGeometry::FDMXGDTFGeometry(const TSharedRef<FDMXGDTFGeometryCollectBase>& InGeometryCollect)
		: OuterGeometryCollect(InGeometryCollect)
	{}

	void FDMXGDTFGeometry::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFGeometryCollectBase::Initialize(XmlNode);

		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name)
			.GetAttribute(TEXT("Model"), Model)
			.GetAttribute(TEXT("Position"), Position);
	}

	TSharedPtr<FDMXGDTFModel> FDMXGDTFGeometry::ResolveModel() const
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
