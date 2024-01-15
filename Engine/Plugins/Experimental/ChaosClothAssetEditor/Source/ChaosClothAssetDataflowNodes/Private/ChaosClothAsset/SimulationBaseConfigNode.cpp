// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowInputOutput.h"

#define LOCTEXT_NAMESPACE "ChaosClothAssetSimulationBaseConfigNode"

namespace UE::Chaos::ClothAsset::Private
{
	static void LogAndToastDuplicateProperty(const FDataflowNode& DataflowNode, const FName& PropertyName)
	{
		using namespace UE::Chaos::ClothAsset;

		static const FText Headline = LOCTEXT("DuplicatePropertyHeadline", "Duplicate property.");

		const FText Details = FText::Format(
			LOCTEXT(
				"DuplicatePropertyDetails",
				"Cloth collection property '{0}' was already set in an upstream node.\n"
				"Its values have now been overridden."),
			FText::FromName(PropertyName));

		FClothDataflowTools::LogAndToastWarning(DataflowNode, Headline, Details);
	}

	static void LogAndToastSimilarProperty(const FDataflowNode& DataflowNode, const FName& PropertyName, const FName& SimilarPropertyName)
	{
		using namespace UE::Chaos::ClothAsset;

		static const FText Headline = LOCTEXT("SimilarPropertyHeadline", "Similar property.");

		const FText Details = FText::Format(
			LOCTEXT(
				"SimilarPropertyDetails",
				"Cloth collection property '{0}' is similar to the property '{1}' already set in an upstream node.\n"
				"This might result in an undefined simulation behavior."),
			FText::FromName(PropertyName),
			FText::FromName(SimilarPropertyName));

		FClothDataflowTools::LogAndToastWarning(DataflowNode, Headline, Details);
	}
}

FChaosClothAssetSimulationBaseConfigNode::FChaosClothAssetSimulationBaseConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{}

void FChaosClothAssetSimulationBaseConfigNode::RegisterCollectionConnections()
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
}

void FChaosClothAssetSimulationBaseConfigNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace Chaos::Softs;
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));

		FCollectionPropertyMutableFacade Properties(ClothCollection);
		Properties.DefineSchema();

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		AddProperties(Context, Properties);  // Deprecated 5.4
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		FPropertyHelper PropertyHelper(*this, Context, Properties);
		AddProperties(PropertyHelper);

		if (FCollectionClothFacade(ClothCollection).IsValid())  // Can only act on the collection if it is a valid cloth collection
		{
			EvaluateClothCollection(Context, ClothCollection);
		}

		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
}

int32 FChaosClothAssetSimulationBaseConfigNode::AddPropertyHelper(
	::Chaos::Softs::FCollectionPropertyMutableFacade& Properties,
	const FName& PropertyName,
	const TArray<FName>& SimilarPropertyNames,
	ECollectionPropertyFlags PropertyFlags) const
{
	using namespace UE::Chaos::ClothAsset;

	int32 KeyIndex = Properties.GetKeyIndex(PropertyName.ToString());
	if (KeyIndex == INDEX_NONE)
	{
		KeyIndex = Properties.AddProperty(PropertyName.ToString());
	}
	else if (!Properties.IsLegacy(KeyIndex))  // Only warns of duplicates when the property hasn't been set using a legacy Chaos config
	{
		Private::LogAndToastDuplicateProperty(*this, PropertyName);
	}
	// else don't warn and hope people know what they are doing

	// Set/clear flags
	Properties.SetFlags(KeyIndex, ECollectionPropertyFlags::Enabled | PropertyFlags); // Always enabled when then node is active, no longer a legacy property after being set by this node

	// Check for similar properties
	for (const FName& SimilarPropertyName : SimilarPropertyNames)
	{
		const int32 SimilarPropertyKeyIndex = Properties.GetKeyIndex(SimilarPropertyName.ToString());
		if (SimilarPropertyKeyIndex != INDEX_NONE && !Properties.IsLegacy(SimilarPropertyKeyIndex))  // Only warns of overrides when the property hasn't been set using a legacy Chaos config
		{
			Private::LogAndToastSimilarProperty(*this, PropertyName, SimilarPropertyName);
		}
	}

	return KeyIndex;
}

FChaosClothAssetSimulationBaseConfigNode::FPropertyHelper::FPropertyHelper(const FChaosClothAssetSimulationBaseConfigNode& InConfigNode, Dataflow::FContext& InContext, ::Chaos::Softs::FCollectionPropertyMutableFacade& InProperties)
	: ConfigNode(InConfigNode)
	, Context(InContext)
	, Properties(InProperties)
{}

int32 FChaosClothAssetSimulationBaseConfigNode::FPropertyHelper::SetPropertyBool(
	const FName& PropertyName,
	bool PropertyValue,
	const TArray<FName>& SimilarPropertyNames,
	ECollectionPropertyFlags PropertyFlags)
{
	checkf(!PropertyName.ToString().StartsWith(TEXT("b")), TEXT("Unlike its C++ counterpart the Boolean property name should not start with a 'b'."));
	const int32 PropertyKeyIndex = ConfigNode.AddPropertyHelper(Properties, PropertyName, SimilarPropertyNames, PropertyFlags);
	Properties.SetValue(PropertyKeyIndex, PropertyValue);
	return PropertyKeyIndex;
}

int32 FChaosClothAssetSimulationBaseConfigNode::FPropertyHelper::SetPropertyString(
	const FName& PropertyName,
	const FString& PropertyValue,
	const TArray<FName>& SimilarPropertyNames,
	ECollectionPropertyFlags PropertyFlags)
{
	const int32 PropertyKeyIndex = ConfigNode.AddPropertyHelper(Properties, PropertyName, SimilarPropertyNames, PropertyFlags);
	Properties.SetStringValue(PropertyKeyIndex, PropertyValue);
	return PropertyKeyIndex;
}

int32 FChaosClothAssetSimulationBaseConfigNode::FPropertyHelper::SetPropertyWeighted(
	const FName& PropertyName,
	const FChaosClothAssetWeightedValue& PropertyValue,
	const TArray<FName>& SimilarPropertyNames,
	ECollectionPropertyFlags PropertyFlags)
{
	ensureMsgf(!EnumHasAnyFlags(PropertyFlags, ECollectionPropertyFlags::Animatable), TEXT("Animatable flag ignored. Weighted properties are set animatable through FChaosClothAssetWeightedValue::bIsAnimatable."));
	if (PropertyValue.bIsAnimatable)
	{
		EnumAddFlags(PropertyFlags, ECollectionPropertyFlags::Animatable);  // Animatable
	}
	else
	{
		EnumRemoveFlags(PropertyFlags, ECollectionPropertyFlags::Animatable);  // Non-animatable
	}
	const int32 PropertyKeyIndex = ConfigNode.AddPropertyHelper(Properties, PropertyName, SimilarPropertyNames, PropertyFlags);
	Properties.SetWeightedValue(PropertyKeyIndex, PropertyValue.Low, PropertyValue.High);
	Properties.SetStringValue(PropertyKeyIndex, ConfigNode.GetValue<FString>(Context, &PropertyValue.WeightMap));
	PropertyValue.WeightMap_Override = ConfigNode.GetValue<FString>(Context, &PropertyValue.WeightMap, UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden);
	return PropertyKeyIndex;
}

int32 FChaosClothAssetSimulationBaseConfigNode::FPropertyHelper::SetPropertyWeighted(
	const FName& PropertyName,
	const FChaosClothAssetWeightedValueNonAnimatable& PropertyValue,
	const TArray<FName>& SimilarPropertyNames,
	ECollectionPropertyFlags PropertyFlags)
{
	ensureMsgf(!EnumHasAnyFlags(PropertyFlags, ECollectionPropertyFlags::Animatable), TEXT("Animatable flag ignored. This code is for a non animatable weighted property."));
	EnumRemoveFlags(PropertyFlags, ECollectionPropertyFlags::Animatable);  // Non animatable
	const int32 PropertyKeyIndex = ConfigNode.AddPropertyHelper(Properties, PropertyName, SimilarPropertyNames, PropertyFlags);
	Properties.SetWeightedValue(PropertyKeyIndex, PropertyValue.Low, PropertyValue.High);
	Properties.SetStringValue(PropertyKeyIndex, ConfigNode.GetValue<FString>(Context, &PropertyValue.WeightMap));
	PropertyValue.WeightMap_Override = ConfigNode.GetValue<FString>(Context, &PropertyValue.WeightMap, UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden);
	return PropertyKeyIndex;
}

int32 FChaosClothAssetSimulationBaseConfigNode::FPropertyHelper::SetPropertyWeighted(
	const FName& PropertyName,
	const FChaosClothAssetWeightedValueNonAnimatableNoLowHighRange& PropertyValue,
	const TArray<FName>& SimilarPropertyNames,
	ECollectionPropertyFlags PropertyFlags)
{
	ensureMsgf(!EnumHasAnyFlags(PropertyFlags, ECollectionPropertyFlags::Animatable), TEXT("Animatable flag ignored. This code is for a non animatable weighted property."));
	EnumRemoveFlags(PropertyFlags, ECollectionPropertyFlags::Animatable);  // Non animatable
	const int32 PropertyKeyIndex = ConfigNode.AddPropertyHelper(Properties, PropertyName, SimilarPropertyNames, PropertyFlags);
	constexpr float Low = 0.f;
	constexpr float High = 1.f;
	Properties.SetWeightedValue(PropertyKeyIndex, Low, High);
	Properties.SetStringValue(PropertyKeyIndex, ConfigNode.GetValue<FString>(Context, &PropertyValue.WeightMap));
	PropertyValue.WeightMap_Override = ConfigNode.GetValue<FString>(Context, &PropertyValue.WeightMap, UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden);
	return PropertyKeyIndex;
}

#undef LOCTEXT_NAMESPACE
