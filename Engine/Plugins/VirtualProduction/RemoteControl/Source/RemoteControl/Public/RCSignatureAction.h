// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "RCSignatureAction.generated.h"

class URemoteControlPreset;
struct FRCSignatureField;
struct FRemoteControlProperty;

/** The context for a Signature Action to execute */
struct FRCSignatureActionContext
{
	/** The preset where the Signature is being applied */
	TObjectPtr<URemoteControlPreset> Preset = nullptr;

	/** The object that the Signature is applying */
	TObjectPtr<UObject> Object = nullptr;

	/** The exposed property from the Signature */
	TSharedPtr<FRemoteControlProperty> Property;
};

#if WITH_EDITOR
/**
 * Editor information on the Icon of a given Action
 * Names are explicitly used over FSlateIcon to avoid a dependency on Slate Core API
 */
struct FRCSignatureActionIcon
{
	/** Name of the style set the icon can be found in */
	FName StyleSetName;

	/** Name of the style for the icon */
	FName StyleName;

	/** Name of the style for the overlay icon (if any) */
	FName OverlayStyleName;

	/** Color of the Base Icon */
	FLinearColor BaseColor = FLinearColor::White;

	/** Optional Color of the Overlay Icon (if any). Uses the Icon Color if not set */
	TOptional<FLinearColor> OverlayColor;
};
#endif

/** Base class of Actions that execute when Applying a Signature Field */
USTRUCT(meta=(Hidden))
struct FRCSignatureAction
{
	GENERATED_BODY()

	virtual ~FRCSignatureAction() = default;

	/**
	 * Called only once when the Signature Action is first added to the action list
	 * @param InFieldOwner the owner of the action list
	 */
	virtual void Initialize(const FRCSignatureField& InFieldOwner)
	{
	}

	/**
	 * Executes the Action logic
	 * @param InContext context of the action (preset, exposed property and uobject being used)
	 * @return true if the action did anything to the preset
	 */
	virtual bool Execute(const FRCSignatureActionContext& InContext) const
	{
		return false;
	}

#if WITH_EDITOR
	/** Retrieves the icon to use to represent this Signature Action */
	virtual FRCSignatureActionIcon GetIcon() const
	{
		return FRCSignatureActionIcon();
	}
#endif
};
