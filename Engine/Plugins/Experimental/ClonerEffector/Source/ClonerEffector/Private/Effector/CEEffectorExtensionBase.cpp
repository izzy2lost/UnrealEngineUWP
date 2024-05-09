// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/CEEffectorExtensionBase.h"

#include "Effector/CEEffectorComponent.h"

UCEEffectorComponent* UCEEffectorExtensionBase::GetEffectorComponent() const
{
	return GetTypedOuter<UCEEffectorComponent>();
}

void UCEEffectorExtensionBase::UpdateExtensionParameters(bool bInUpdateLinkedCloners, bool bInImmediate)
{
	if (!IsExtensionActive())
	{
		return;
	}

	if (UCEEffectorComponent* EffectorComponent = GetEffectorComponent())
	{
		if (!EffectorComponent->GetEnabled())
		{
			return;
		}

		OnExtensionParametersChanged(EffectorComponent);

		if (bInUpdateLinkedCloners)
		{
			EffectorComponent->RequestClonerUpdate(bInImmediate);
		}
	}
}

void UCEEffectorExtensionBase::ActivateExtension()
{
	if (!bExtensionActive)
	{
		bExtensionActive = true;
		OnExtensionActivated();
		UpdateExtensionParameters();
	}
}

void UCEEffectorExtensionBase::DeactivateExtension()
{
	if (bExtensionActive)
	{
		bExtensionActive = false;
		OnExtensionDeactivated();
	}
}

void UCEEffectorExtensionBase::PostEditImport()
{
	Super::PostEditImport();

	UpdateExtensionParameters();
}

void UCEEffectorExtensionBase::OnExtensionPropertyChanged()
{
	UpdateExtensionParameters();
}
