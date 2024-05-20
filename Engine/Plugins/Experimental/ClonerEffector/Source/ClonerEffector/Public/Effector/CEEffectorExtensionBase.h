// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CEClonerEffectorShared.h"
#include "UObject/Object.h"
#include "CEEffectorExtensionBase.generated.h"

class UCEEffectorComponent;

/** Represents an extension for an effector to apply a custom behavior on cloner */
UCLASS(MinimalAPI, Abstract, BlueprintType, Within=CEEffectorComponent)
class UCEEffectorExtensionBase : public UObject
{
	GENERATED_BODY()

public:
	UCEEffectorExtensionBase()
		: UCEEffectorExtensionBase(
			NAME_None
#if WITH_EDITOR
			, FCEExtensionSection(NAME_None, INDEX_NONE)
#endif
		)
	{}

	UCEEffectorExtensionBase(
		FName InExtensionName
#if WITH_EDITOR
		, const FCEExtensionSection& InExtensionSection
#endif
		)
		: ExtensionName(InExtensionName)
#if WITH_EDITOR
		, ExtensionSection(InExtensionSection)
#endif
	{}

	FName GetExtensionName() const
	{
		return ExtensionName;
	}

#if WITH_EDITOR
	const FCEExtensionSection& GetExtensionSection() const
	{
		return ExtensionSection;
	}
#endif

	/** Get the effector component using this extension */
	UCEEffectorComponent* GetEffectorComponent() const;

	/** Request refresh extension next tick */
	void UpdateExtensionParameters(bool bInUpdateLinkedCloners = false, bool bInImmediate = false);

	/** Enable this extension */
	void ActivateExtension();

	/** Disable this extension */
	void DeactivateExtension();

	bool IsExtensionActive() const
	{
		return bExtensionActive;
	}

protected:
	//~ Begin UObject
	virtual void PostEditImport() override;
	//~ End UObject

	/** Called when extension becomes active */
	virtual void OnExtensionActivated() {}

	/** Called when extension becomes inactive */
	virtual void OnExtensionDeactivated() {}

	/** Called to reapply type parameters */
	virtual void OnExtensionParametersChanged(UCEEffectorComponent* InComponent) {}

	/** Used by PECP to update parameters */
	void OnExtensionPropertyChanged();

private:
	/** Unique extension name used for dropdown and selection */
	UPROPERTY(Transient)
	FName ExtensionName = NAME_None;

	bool bExtensionActive = false;

#if WITH_EDITOR
	FCEExtensionSection ExtensionSection;
#endif
};