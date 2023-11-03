// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPin.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#include "PCGActorPropertyOverride.generated.h"

class IPCGAttributeAccessor;
class UPCGData;
struct FPCGContext;

class AActor;

/**
* Represents the override source (to be read) and the actor property (to be written).
*/
USTRUCT(BlueprintType)
struct FPCGActorPropertyOverrideDescription
{
	GENERATED_BODY()

	FPCGActorPropertyOverrideDescription() = default;

	FPCGActorPropertyOverrideDescription(const FPCGAttributePropertyInputSelector& InInputSource, const FString& InPropertyTarget)
		: InputSource(InInputSource)
		, PropertyTarget(InPropertyTarget)
	{}

	/** Provide an attribute or property to read the override value from. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector InputSource;

	/**
	* Provide an actor property name to be overridden. If you have a property "A" on your actor, use "A" as the property target.
	*
	* For example, if you want to override the "Is Editor Only" flag, find it in the details panel, right-click, select 'Copy Internal Name', and paste that as the property target.
	*
	* If you have a component property, such as the static mesh of a static mesh component, use "StaticMeshComponent.StaticMesh".
	*/
	UPROPERTY(EditAnywhere, Category = Settings)
	FString PropertyTarget;
};

namespace PCGActorPropertyOverrideHelpers
{
	/** Create an advanced ParamData pin for capturing property overrides. */
	FPCGPinProperties CreateActorPropertiesOverridePin(FName Label, const FText& Tooltip);
	
	/** Apply property overrides to the TargetActor directly from the ActorPropertiesOverride pin. Use CreateActorPropertiesOverridePin(). */
	void ApplyOverridesFromParams(const TArray<FPCGActorPropertyOverrideDescription>& InActorPropertyOverrideDescriptions, AActor* TargetActor, FName OverridesPinLabel, FPCGContext* Context);
}

/**
* Represents a single property override on the provided actor. Applies an override function to read the InputAccessor
* and write its value to the OutputAccessor.
* 
* The InputAccessor's InputKeys are created from the given SourceData and InputSelector.
*/
struct FPCGActorSingleOverride
{
	/** Initialize the single actor override. Call before using Apply(InputKeyIndex, OutputKey). */
	void Initialize(const FPCGAttributePropertySelector& InputSelector, const FString& OutputProperty, AActor* TemplateActor, const UPCGData* SourceData, FPCGContext* Context);
	
	/** Returns true if initialization succeeded in creating the accessors and accessor keys. */
	bool IsValid() const;

	/** Applies a single property override to the actor by reading from the InputAccessor at the given KeyIndex, and writing to the OutputKey which represents the actor property. */
	bool Apply(int32 InputKeyIndex, IPCGAttributeAccessorKeys& OutputKey);

private:
	TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys;
	TUniquePtr<const IPCGAttributeAccessor> ActorOverrideInputAccessor;
	TUniquePtr<IPCGAttributeAccessor> ActorOverrideOutputAccessor;

	// InputKeyIndex, OutputKeys
	using ApplyOverrideFunction = bool(FPCGActorSingleOverride::*)(int32, IPCGAttributeAccessorKeys&);
	ApplyOverrideFunction ActorOverrideFunction;

	template <typename Type>
	bool ApplyImpl(int32 InputKeyIndex, IPCGAttributeAccessorKeys& OutputKey)
	{
		if (!IsValid())
		{
			return false;
		}

		Type Value{};
		if (ActorOverrideInputAccessor->Get<Type>(Value, InputKeyIndex, *InputKeys.Get(), EPCGAttributeAccessorFlags::AllowBroadcast))
		{
			if (ActorOverrideOutputAccessor->Set<Type>(Value, OutputKey))
			{
				return true;
			}
		}

		return false;
	}
};

/**
* Represents a set of property overrides for the provided actor. Provide a SourceData to read from, and a collection of ActorPropertyOverrides matching the TemplateActor's class properties.
*/
struct FPCGActorOverrides
{
	FPCGActorOverrides(AActor* TemplateActor) : OutputKey(TemplateActor)
	{}

	/** Initialize the actor overrides. Call before using Apply(InputKeyIndex). */
	void Initialize(const TArray<FPCGActorPropertyOverrideDescription>& OverrideDescriptions, AActor* TemplateActor, const UPCGData* SourceData, FPCGContext* Context);

	/** Applies each property override to the actor by reading from the InputAccessor at the given KeyIndex, and writing to the OutputKey which represents the actor property. */
	bool Apply(int32 InputKeyIndex);

private:
	FPCGAttributeAccessorKeysSingleObjectPtr<AActor> OutputKey;
	TArray<FPCGActorSingleOverride> ActorSingleOverrides;
};

USTRUCT(BlueprintType, meta=(Deprecated = "5.4", DeprecationMessage="Use FPCGActorPropertyOverrideDescription instead."))
struct FPCGActorPropertyOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector InputSource;

	UPROPERTY(EditAnywhere, Category = Settings)
	FString PropertyTarget;
};
