// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Extensions/CEClonerExtensionBase.h"

#include "Cloner/CEClonerComponent.h"

UCEClonerComponent* UCEClonerExtensionBase::GetClonerComponent() const
{
	return GetTypedOuter<UCEClonerComponent>();
}

UCEClonerComponent* UCEClonerExtensionBase::GetClonerComponentChecked() const
{
	UCEClonerComponent* ClonerComponent = GetClonerComponent();

	if (IsValid(ClonerComponent))
	{
		return ClonerComponent;
	}

	checkf(false, TEXT("Cloner component is invalid"))

	return nullptr;
}

UCEClonerLayoutBase* UCEClonerExtensionBase::GetClonerLayout() const
{
	if (UCEClonerComponent* ClonerComponent = GetClonerComponent())
	{
		return ClonerComponent->GetActiveLayout();
	}

	return nullptr;
}

void UCEClonerExtensionBase::ActivateExtension()
{
	if (!bExtensionActive)
	{
		bExtensionActive = true;
		OnExtensionActivated();
	}
}

void UCEClonerExtensionBase::DeactivateExtension()
{
	if (bExtensionActive)
	{
		bExtensionActive = false;
		OnExtensionDeactivated();
	}
}

void UCEClonerExtensionBase::UpdateExtensionParameters(bool bInUpdateCloner, bool bInImmediate)
{
	if (!IsExtensionActive())
	{
		return;
	}

	if (UCEClonerComponent* ClonerComponent = GetClonerComponent())
	{
		if (!ClonerComponent->GetEnabled())
		{
			return;
		}

		OnExtensionParametersChanged(ClonerComponent);

		if (bInUpdateCloner)
		{
			ClonerComponent->RequestClonerUpdate(bInImmediate);
		}
	}
}

void UCEClonerExtensionBase::PostEditImport()
{
	Super::PostEditImport();

	UpdateExtensionParameters();
}

#if WITH_EDITOR
void UCEClonerExtensionBase::PostEditUndo()
{
	Super::PostEditUndo();

	UpdateExtensionParameters();
}
#endif

void UCEClonerExtensionBase::OnExtensionPropertyChanged()
{
	UpdateExtensionParameters();
}
