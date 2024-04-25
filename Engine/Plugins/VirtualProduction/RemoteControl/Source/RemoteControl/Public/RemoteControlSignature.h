// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteControlFieldPath.h"
#include "RemoteControlSignature.generated.h"

class URemoteControlPreset;

USTRUCT()
struct FRCSignatureField
{
	GENERATED_BODY()

	FRCSignatureField() = default;

	bool operator==(const FRCSignatureField& InOtherField) const
	{
		return FieldPath == InOtherField.FieldPath
			&& ObjectRelativePath == InOtherField.ObjectRelativePath
			&& SupportedClass == InOtherField.SupportedClass;
	}

	/** Path Info for this Field */
	UPROPERTY()
	FRCFieldPathInfo FieldPath;

	/** Optional: Relative path from an owner (e.g. Actor) to the object owning the property (e.g. an Actor Component) */
	UPROPERTY()
	FString ObjectRelativePath;

	/** Object class holding the property */
	UPROPERTY()
	FSoftClassPath SupportedClass;

	/** Whether to consider this field when applying a Signature */
	UPROPERTY()
	bool bEnabled = true;
};

USTRUCT()
struct FRCSignature
{
	GENERATED_BODY()

	bool operator==(const FGuid& InSignatureId) const
	{
		return Id == InSignatureId;
	}

	/**
	 * Gathers the Field Entities from the given preset and field ids and adds them as Signature Fields to this instance
	 * @param InPreset the preset to check for
	 * @param InFieldEntityIds the ids of the entities to find and add
	 */
	REMOTECONTROL_API int32 AddFieldsFromEntities(URemoteControlPreset* InPreset, TConstArrayView<FGuid> InFieldEntityIds);

	/**
	 * Applies this Signature to the given Actors by exposing all this Signature's fields to the given preset
	 * @param InPreset the preset where these properties will be exposed to
	 * @param InActors the actors whose properties to expose
	 */
	REMOTECONTROL_API int32 ApplySignature(URemoteControlPreset* InPreset, TConstArrayView<TWeakObjectPtr<AActor>> InActors) const;

	/** User facing friendly name. Used as the Label when exposing */
	UPROPERTY()
	FText DisplayName;

	/** Unique Id identifying this Signature */
	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	FGuid Id;

	/** The fields owned by this Signature */
	UPROPERTY()
	TArray<FRCSignatureField> Fields;

	/** Whether this Signature can be applied */
	UPROPERTY()
	bool bEnabled = true;
};