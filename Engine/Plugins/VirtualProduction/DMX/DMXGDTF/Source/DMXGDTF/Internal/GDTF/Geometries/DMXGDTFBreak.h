// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GDTF/DMXGDTFNode.h"
#include "GDTF/Geometries/DMXGDTFAddress.h"

namespace UE::DMX::GDTF
{
	class FDMXGDTFGeometryReference;

	/** This XML node specifies the DMX offset for the DMX channel of the referenced geometry (XML node <Break>) */
	class DMXGDTF_API FDMXGDTFBreak
		: public FDMXGDTFNode
	{
	public:
		FDMXGDTFBreak(const TSharedRef<FDMXGDTFGeometryReference>& InGeometryReference);

		//~ Begin DMXGDTFGeneralGeometryNode interface
		virtual const TCHAR* GetXmlTag() const override { return TEXT("Break"); }
		virtual void Initialize(const FXmlNode& XmlNode) override;
		//~ End DMXGDTFGeneralGeometryNode interface

		/** DMX offset; Default value:1 (Means no offset for the corresponding DMX Channel) */
		FDMXGDTFAddress DMXAddress;

		/** Defines the unique number of the DMX Break for which the Offset is given. Size: 1 byte; Default value 1 */
		uint8 DMXBreak = 1;

		/** The outer geometry reference */
		const TWeakPtr<FDMXGDTFGeometryReference> OuterGeometryReference;
	};
}
