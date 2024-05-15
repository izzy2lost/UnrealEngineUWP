// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GDTF/Geometries/DMXGDTFGeometryCollectBase.h"
#include "Math/Transform.h" 

namespace UE::DMX::GDTF
{
	class FDMXGDTFModel;

	/**
	 * It is a basic geometry type without specification (XML node <Geometry>).
	 *
	 * UE specific: Base class for all geometry nodes.
	 */
	class DMXGDTF_API FDMXGDTFGeometry
		: public FDMXGDTFGeometryCollectBase
	{
	public:
		FDMXGDTFGeometry(const TSharedRef<FDMXGDTFGeometryCollectBase>& InGeometryCollect);

		//~ Begin DMXGDTFGeneralGeometryNode interface
		virtual const TCHAR* GetXmlTag() const override { return TEXT("Geometry"); }
		virtual void Initialize(const FXmlNode& XmlNode) override;
		virtual FXmlNode* CreateXmlNode(FXmlNode& Parent) override;
		//~ End DMXGDTFGeneralGeometryNode interface

		/** The unique name of geometry. See standard for details of specific instances, e.g. Beam, Shaper Filter etc. */
		FName Name;

		/** Link to the corresponding model. */
		FString Model;

		/** Relative position of geometry */
		FTransform Position = FTransform::Identity;

		/** The outer geometry collect */
		const TWeakPtr<FDMXGDTFGeometryCollectBase> OuterGeometryCollect;

		/** Resolves the linked model. Returns the model, or nullptr if no model is linked */
		TSharedPtr<FDMXGDTFModel> ResolveModel() const;
	};
}
