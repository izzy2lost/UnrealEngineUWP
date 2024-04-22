// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFGeometryReference.h"

#include "Algo/Find.h"
#include "GDTF/DMXGDTFFixtureType.h"
#include "GDTF/Geometries/DMXGDTFBreak.h"
#include "GDTF/Geometries/DMXGDTFGeometry.h"
#include "GDTF/Geometries/DMXGDTFGeometryCollect.h"
#include "GDTF/Models/DMXGDTFModel.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFGeometryReference::FDMXGDTFGeometryReference(const TSharedRef<FDMXGDTFGeometryCollectBase>& InGeometryCollect)
		: OuterGeometryCollect(InGeometryCollect)
	{}

	void FDMXGDTFGeometryReference::Initialize(const FXmlNode& XmlNode)
	{
		using namespace UE::DMX::GDTF;

		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name)
			.CreateChildren(TEXT("Break"), BreakArray)
			.GetAttribute(TEXT("Geometry"), Geometry)
			.GetAttribute(TEXT("Model"), Model);
	}

	TSharedPtr<FDMXGDTFGeometry> FDMXGDTFGeometryReference::ResolveGeometry() const
	{
		// Only top level geometries are allowed
		const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin();
		const TSharedPtr<FDMXGDTFGeometryCollect> GeometryCollect = FixtureType.IsValid() ? FixtureType->GeometryCollect : nullptr;
		if (GeometryCollect.IsValid())
		{
			const TSharedPtr<FDMXGDTFGeometry>* GeometryPtr = Algo::FindBy(GeometryCollect->GeometryArray, Geometry, &FDMXGDTFGeometry::Name);
			if (GeometryPtr)
			{
				return *GeometryPtr;
			}
		}

		return nullptr;
	}

	TSharedPtr<FDMXGDTFModel> FDMXGDTFGeometryReference::ResolveModel() const
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
