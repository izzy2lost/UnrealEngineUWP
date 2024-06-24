// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR 

#include "DMXAttributeToDefaultPhyiscalProperties.h"

#include "Library/DMXEntityFixtureType.h"

namespace UE::DMX
{
	void FDMXAttributeToDefaultPhyiscalProperties::ResetToDefaultPhysicalProperties(FDMXFixtureFunction& InOutFunction)
	{
		const FDefaultPhyicalProperties* AttributeNameToDefaultPhysicalProperitesPtr = AttributeNameToDefaultPhysicalProperitesMap.Find(InOutFunction.Attribute.Name);
		if (AttributeNameToDefaultPhysicalProperitesPtr)
		{
			InOutFunction.SetPhysicalUnit(AttributeNameToDefaultPhysicalProperitesPtr->PhysicalUnit);
			InOutFunction.SetPhysicalValueRange(AttributeNameToDefaultPhysicalProperitesPtr->PhysicalFrom, AttributeNameToDefaultPhysicalProperitesPtr->PhysicalTo);
		}
	}
}

#endif // WITH_EDITOR
