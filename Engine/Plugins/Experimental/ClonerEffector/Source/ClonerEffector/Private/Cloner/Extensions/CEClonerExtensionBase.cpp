// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Extensions/CEClonerExtensionBase.h"

#include "Cloner/CEClonerComponent.h"

#if WITH_EDITOR
FCEExtensionSection UCEClonerExtensionBase::GetExtensionSection() const
{
	return UE::ClonerEffector::EditorSection::GetExtensionSectionFromClass(GetClass());
}
#endif

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

void UCEClonerExtensionBase::UpdateExtensionParameters()
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

		if (EnumHasAnyFlags(ExtensionStatus, ECEClonerSystemStatus::SimulationDirty))
		{
			ClonerComponent->RequestClonerUpdate(/*Immediate*/false);
		}
	}

	ExtensionStatus = ECEClonerSystemStatus::UpToDate;
}

void UCEClonerExtensionBase::MarkExtensionDirty(bool bInUpdateCloner)
{
	EnumAddFlags(ExtensionStatus, ECEClonerSystemStatus::ParametersDirty);

	if (bInUpdateCloner)
	{
		EnumAddFlags(ExtensionStatus, ECEClonerSystemStatus::SimulationDirty);
	}
}

bool UCEClonerExtensionBase::IsExtensionDirty() const
{
	return EnumHasAnyFlags(ExtensionStatus, ECEClonerSystemStatus::ParametersDirty);
}

void UCEClonerExtensionBase::PostEditImport()
{
	Super::PostEditImport();

	MarkExtensionDirty();
}

#if WITH_EDITOR
void UCEClonerExtensionBase::PostEditUndo()
{
	Super::PostEditUndo();

	MarkExtensionDirty();
}
#endif

void UCEClonerExtensionBase::OnExtensionPropertyChanged()
{
	MarkExtensionDirty();
}
