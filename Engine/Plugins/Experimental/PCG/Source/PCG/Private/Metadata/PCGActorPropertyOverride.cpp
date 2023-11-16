// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGActorPropertyOverride.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGActorPropertyOverride"

namespace PCGActorPropertyOverrideHelpers
{
	FPCGPinProperties CreateActorPropertiesOverridePin(FName Label, const FText& Tooltip)
	{
		FPCGPinProperties ActorOverridePinProperties(Label, EPCGDataType::Param, /*bAllowMultipleConnections=*/false, /*bAllowMultipleData=*/false, Tooltip);
		ActorOverridePinProperties.bAdvancedPin = true;
		return ActorOverridePinProperties;
	}

	void ApplyOverridesFromParams(const TArray<FPCGActorPropertyOverrideDescription>& InActorPropertyOverrideDescriptions, AActor* TargetActor, FName OverridesPinLabel, FPCGContext* Context)
	{
		if (!Context)
		{
			return;
		}

		const TArray<FPCGTaggedData> OverrideInputs = Context->InputData.GetInputsByPin(OverridesPinLabel);

		if (OverrideInputs.Num() == 0)
		{
			return;
		}
		else if (OverrideInputs.Num() > 1)
		{
			PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("MoreThanOneData", "More than one data was found on pin '{0}'. Only using the first one."), FText::FromName(OverridesPinLabel)), Context);
		}

		const UPCGParamData* ParamData = Cast<const UPCGParamData>(OverrideInputs[0].Data);

		if (!ParamData)
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("InvalidActorOverrideData", "Invalid input data type for Actor Property Overrides pin, must be of type Param."), Context);
			return;
		}

		FPCGActorOverrides ActorOverrides(TargetActor);
		ActorOverrides.Initialize(InActorPropertyOverrideDescriptions, TargetActor, ParamData, Context);
		if (!ActorOverrides.Apply(/*InputKeyIndex=*/0)) // Use the First Entry of the param data for override (similar to what is done in Parameter Overrides in FPCGContext)
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("ApplyOverrideFailed", "Failed to apply property overrides to actor '%s' from an attribute set."), FText::FromName(TargetActor->GetClass()->GetFName())), Context);
		}
	}
}

void FPCGActorSingleOverride::Initialize(const FPCGAttributePropertySelector& InputSelector, const FString& OutputProperty, AActor* TemplateActor, const UPCGData* SourceData, FPCGContext* Context)
{
	InputKeys = PCGAttributeAccessorHelpers::CreateConstKeys(SourceData, InputSelector);
	ActorOverrideInputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(SourceData, InputSelector);
	FPCGAttributePropertySelector OutputSelector = FPCGAttributePropertySelector::CreateSelectorFromString(OutputProperty);
	const TArray<FString>& ExtraNames = OutputSelector.GetExtraNames();
	if (ExtraNames.IsEmpty())
	{
		ActorOverrideOutputAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(FName(OutputProperty), TemplateActor->GetClass());
	}
	else
	{
		TArray<FName> PropertyNames;
		PropertyNames.Reserve(ExtraNames.Num() + 1);
		PropertyNames.Add(OutputSelector.GetAttributeName());
		for (const FString& Name : ExtraNames)
		{
			PropertyNames.Add(FName(Name));
		}

		ActorOverrideOutputAccessor = PCGAttributeAccessorHelpers::CreatePropertyChainAccessor(PropertyNames, TemplateActor->GetClass());
	}

	if (!ActorOverrideInputAccessor.IsValid() || !ActorOverrideOutputAccessor.IsValid())
	{
		PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("OverrideInvalid", "ActorOverride from input '{0}' or output '{1}' is invalid or unsupported. Will be skipped."), InputSelector.GetDisplayText(), OutputSelector.GetDisplayText()), Context);
		return;
	}

	if (!PCG::Private::IsBroadcastableOrConstructible(ActorOverrideInputAccessor->GetUnderlyingType(), ActorOverrideOutputAccessor->GetUnderlyingType()))
	{
		PCGLog::LogWarningOnGraph(
			FText::Format(LOCTEXT("TypesIncompatible", "ActorOverride cannot set input '{0}' to output '{1}'. Cannot convert type '{2}' to type '{3}'. Will be skipped."),
				InputSelector.GetDisplayText(),
				OutputSelector.GetDisplayText(),
				PCG::Private::GetTypeNameText(ActorOverrideInputAccessor->GetUnderlyingType()),
				PCG::Private::GetTypeNameText(ActorOverrideOutputAccessor->GetUnderlyingType())),
			Context);

		ActorOverrideInputAccessor.Reset();
		ActorOverrideOutputAccessor.Reset();
		return;
	}

	auto CreateGetterSetter = [this](auto Dummy)
	{
		using Type = decltype(Dummy);

		ActorOverrideFunction = &FPCGActorSingleOverride::ApplyImpl<Type>;
	};

	PCGMetadataAttribute::CallbackWithRightType(ActorOverrideOutputAccessor->GetUnderlyingType(), CreateGetterSetter);
}

bool FPCGActorSingleOverride::IsValid() const
{
	return InputKeys.IsValid() && ActorOverrideInputAccessor.IsValid() && ActorOverrideOutputAccessor.IsValid() && ActorOverrideFunction;
}

bool FPCGActorSingleOverride::Apply(int32 InputKeyIndex, IPCGAttributeAccessorKeys& OutputKey)
{
	return Invoke(ActorOverrideFunction, this, InputKeyIndex, OutputKey);
}

void FPCGActorOverrides::Initialize(const TArray<FPCGActorPropertyOverrideDescription>& OverrideDescriptions, AActor* TemplateActor, const UPCGData* SourceData, FPCGContext* Context)
{
	if (!TemplateActor)
	{
		PCGLog::LogErrorOnGraph(LOCTEXT("InitializeOverrideFailedNoActor", "Failed to initialize property overrides. No template actor was provided."), Context);
		return;
	}

	ActorSingleOverrides.Reserve(OverrideDescriptions.Num());

	for (int32 i = 0; i < OverrideDescriptions.Num(); ++i)
	{
		FPCGAttributePropertyInputSelector InputSelector = OverrideDescriptions[i].InputSource.CopyAndFixLast(SourceData);
		const FString& OutputProperty = OverrideDescriptions[i].PropertyTarget;

		FPCGActorSingleOverride Override;
		Override.Initialize(InputSelector, OutputProperty, TemplateActor, SourceData, Context);

		if (Override.IsValid())
		{
			ActorSingleOverrides.Add(std::move(Override));
		}
		else
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("InitializeOverrideFailed", "Failed to initialize override '{0}' for property {1} on actor '{2}'."), InputSelector.GetDisplayText(), FText::FromString(OutputProperty), FText::FromName(TemplateActor->GetClass()->GetFName())), Context);
		}
	}
}

bool FPCGActorOverrides::Apply(int32 InputKeyIndex)
{
	bool bAllSucceeded = true;

	for (FPCGActorSingleOverride& ActorSingleOverride : ActorSingleOverrides)
	{
		bAllSucceeded &= ActorSingleOverride.Apply(InputKeyIndex, OutputKey);
	}

	return bAllSucceeded;
}

#undef LOCTEXT_NAMESPACE
