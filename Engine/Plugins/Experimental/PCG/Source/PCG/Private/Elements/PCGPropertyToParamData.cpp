// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPropertyToParamData.h"

#include "GameFramework/Actor.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "PCGParamData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGPropertyHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPropertyToParamData)

#define LOCTEXT_NAMESPACE "PCGPropertyToParamDataElement"

#if WITH_EDITOR
void UPCGPropertyToParamDataSettings::GetTrackedActorKeys(FPCGActorSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	OutKeysToSettings.FindOrAdd(ActorSelector.GetAssociatedKey()).Emplace(this, bTrackActorsOnlyWithinBounds);
}

void UPCGPropertyToParamDataSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UPCGPropertyToParamDataSettings, ActorSelector))
	{
		if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FPCGActorSelectorSettings, ActorSelection))
		{
			if (ActorSelector.ActorSelection != EPCGActorSelection::ByClass)
			{
				ActorSelector.ActorSelectionClass = TSubclassOf<AActor>();
			}
		}
	}
}
#endif // WITH_EDITOR

void UPCGPropertyToParamDataSettings::PostLoad()
{
	Super::PostLoad();

	// Migrate deprecated actor selection settings to struct if needed
	if (ActorSelection_DEPRECATED != EPCGActorSelection::ByTag ||
		ActorSelectionTag_DEPRECATED != NAME_None ||
		ActorSelectionName_DEPRECATED != NAME_None ||
		ActorSelectionClass_DEPRECATED != TSubclassOf<AActor>() ||
		ActorFilter_DEPRECATED != EPCGActorFilter::Self ||
		bIncludeChildren_DEPRECATED != false)
	{
		ActorSelector.ActorSelection = ActorSelection_DEPRECATED;
		ActorSelector.ActorSelectionTag = ActorSelectionTag_DEPRECATED;
		ActorSelector.ActorSelectionClass = ActorSelectionClass_DEPRECATED;
		ActorSelector.ActorFilter = ActorFilter_DEPRECATED;
		ActorSelector.bIncludeChildren = bIncludeChildren_DEPRECATED;

		ActorSelection_DEPRECATED = EPCGActorSelection::ByTag;
		ActorSelectionTag_DEPRECATED = NAME_None;
		ActorSelectionName_DEPRECATED = NAME_None;
		ActorSelectionClass_DEPRECATED = TSubclassOf<AActor>();
		ActorFilter_DEPRECATED = EPCGActorFilter::Self;
		bIncludeChildren_DEPRECATED = false;
	}

	if (ActorSelector.ActorSelection != EPCGActorSelection::ByClass)
	{
		ActorSelector.ActorSelectionClass = TSubclassOf<AActor>();
	}
}

FName UPCGPropertyToParamDataSettings::AdditionalTaskName() const
{
#if WITH_EDITOR
	return ActorSelector.GetTaskName(GetDefaultNodeTitle());
#else
	return Super::AdditionalTaskName();
#endif
}


TArray<FPCGPinProperties> UPCGPropertyToParamDataSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}

FPCGElementPtr UPCGPropertyToParamDataSettings::CreateElement() const
{
	return MakeShared<FPCGPropertyToParamDataElement>();
}

bool FPCGPropertyToParamDataElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPropertyToParamDataElement::Execute);

	check(Context);

	const UPCGPropertyToParamDataSettings* Settings = Context->GetInputSettings<UPCGPropertyToParamDataSettings>();
	check(Settings);

	// Early out if arguments are not specified
	if (Settings->PropertyName == NAME_None || (Settings->bSelectComponent && !Settings->ComponentClass))
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("ParametersMissing", "Some parameters are missing, aborting"));
		return true;
	}

#if !WITH_EDITOR
	// If we have no output connected, nothing to do
	// Optimization possibly only in non-editor builds, otherwise we could poison the input-driven cache
	if (!Context->Node || !Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel))
	{
		PCGE_LOG(Verbose, LogOnly, LOCTEXT("UnconnectedNode", "Node is not connected, nothing to do"));
		return true;
	}
#endif

	// First find the actor depending on the selection
	UPCGComponent* OriginalComponent = UPCGBlueprintHelpers::GetOriginalComponent(*Context);
	TFunction<bool(const AActor*)> BoundsCheck = [](const AActor*) -> bool { return true; };
	auto NoSelfIgnoreCheck = [](const AActor*) -> bool { return true; };

	if (OriginalComponent && OriginalComponent->GetOwner() && Settings->ActorSelector.bMustOverlapSelf)
	{
		const FBox ActorBounds = PCGHelpers::GetActorBounds(OriginalComponent->GetOwner());
		BoundsCheck = [Settings, ActorBounds, OriginalComponent](const AActor* OtherActor) -> bool
		{
			const FBox OtherActorBounds = OtherActor ? PCGHelpers::GetGridBounds(OtherActor, OriginalComponent) : FBox(EForceInit::ForceInit);
			return ActorBounds.Intersect(OtherActorBounds);
		};
	}

	AActor* FoundActor = PCGActorSelector::FindActor(Settings->ActorSelector, OriginalComponent, BoundsCheck, NoSelfIgnoreCheck);

	if (!FoundActor)
	{
		PCGE_LOG(Verbose, LogOnly, LOCTEXT("NoActorFound", "No matching actor was found"));
		return true;
	}

	// From there, we either check the actor, or the component attached to it.
	UObject* ObjectToInspect = FoundActor;
	if (Settings->bSelectComponent)
	{
		ObjectToInspect = FoundActor->GetComponentByClass(Settings->ComponentClass);
		if (!ObjectToInspect)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ComponentDoesNotExist", "Component class '{0}' does not exist in the found actor"), FText::FromString(Settings->ComponentClass->GetName())));
			return true;
		}
	}

	FPCGAttributePropertySelector Selector = FPCGAttributePropertySelector::CreateSelectorFromString(Settings->PropertyName.ToString());

	PCGPropertyHelpers::FExtractorParameters Parameters{ ObjectToInspect, ObjectToInspect->GetClass(), Selector, Settings->OutputAttributeName, Settings->bForceObjectAndStructExtraction, /*bPropertyNeedsToBeVisible=*/true};
	if (UPCGParamData* ParamData = PCGPropertyHelpers::ExtractPropertyAsAttributeSet(Parameters, Context))
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = ParamData;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
