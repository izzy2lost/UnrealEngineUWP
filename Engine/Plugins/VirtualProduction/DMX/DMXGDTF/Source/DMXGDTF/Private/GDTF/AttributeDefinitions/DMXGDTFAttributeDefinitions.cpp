// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/AttributeDefinitions/DMXGDTFAttributeDefinitions.h"

#include "Algo/Find.h"
#include "GDTF/AttributeDefinitions/DMXGDTFActivationGroup.h"
#include "GDTF/AttributeDefinitions/DMXGDTFAttribute.h"
#include "GDTF/AttributeDefinitions/DMXGDTFFeature.h"
#include "GDTF/AttributeDefinitions/DMXGDTFFeatureGroup.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFAttributeDefinitions::FDMXGDTFAttributeDefinitions(const TSharedRef<FDMXGDTFFixtureType>& InFixtureType)
	{		
		// The outer fixture type is available to all nodes and doesn't need extra initialization.
		// This constructor is only here to enforce the correct outer.
	}

	void FDMXGDTFAttributeDefinitions::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.CreateChildCollection(TEXT("ActivationGroups"), TEXT("ActivationGroup"), ActivationGroups)
			.CreateChildCollection(TEXT("FeatureGroups"), TEXT("FeatureGroup"), FeatureGroups)
			.CreateChildCollection(TEXT("Attributes"), TEXT("Attribute"), Attributes);
	}

	TSharedPtr<FDMXGDTFActivationGroup> FDMXGDTFAttributeDefinitions::FindActivationGroup(const FString& ActivationGroupName) const
	{
		if (const TSharedPtr<FDMXGDTFActivationGroup>* ActivationGroupPtr = Algo::FindBy(ActivationGroups, ActivationGroupName, &FDMXGDTFActivationGroup::Name))
		{
			return *ActivationGroupPtr;
		}

		return nullptr;
	}

	TSharedPtr<FDMXGDTFAttribute> FDMXGDTFAttributeDefinitions::FindAttribute(const FString& AttributeName) const
	{
		if (const TSharedPtr<FDMXGDTFAttribute>* AttributePtr = Algo::FindBy(Attributes, AttributeName, &FDMXGDTFAttribute::Name))
		{
			return *AttributePtr;
		}

		return nullptr;
	}

	TSharedPtr<FDMXGDTFFeature> FDMXGDTFAttributeDefinitions::FindFeature(const FString& FeatureGroupName, const FString& FeatureName) const
	{
		if (const TSharedPtr<FDMXGDTFFeatureGroup>* FeatureGroupPtr = Algo::FindBy(FeatureGroups, FeatureGroupName, &FDMXGDTFFeatureGroup::Name))
		{
			if (const TSharedPtr<FDMXGDTFFeature>* FeaturePtr = Algo::FindBy((*FeatureGroupPtr)->FeatureArray, FeatureName, &FDMXGDTFFeature::Name))
			{
				return *FeaturePtr;
			}
		}
		
		return nullptr;
	}
}
