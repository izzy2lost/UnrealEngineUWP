// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FXmlFile;

namespace UE::DMX::GDTF
{
	class FDMXGDTFNode;
	class FDMXGDTFFixtureType;

	/**
	 * The Description of a GDTF, corresponds to the description.xml of the GDTF.
	 *
	 * The descritpion and all related members in this module shall follow the currently implemented GDTF standard.
	 * See DMXGDTFVersion for details.
	 */
	class DMXGDTF_API FDMXGDTFDescription
		: public TSharedFromThis<FDMXGDTFDescription>
	{
	public:
		/** Initializes this node and its chilren from a GDTF Description.xml */
		void InitializeFromDescriptionXml(const TSharedRef<FXmlFile>& DescriptionXml);

	private:
		/** The Fixture Type Child Node */
		TSharedPtr<FDMXGDTFFixtureType> FixtureType;
	};
}
