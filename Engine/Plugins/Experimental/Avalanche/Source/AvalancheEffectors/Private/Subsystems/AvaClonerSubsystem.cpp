// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/AvaClonerSubsystem.h"

#include "Cloner/AvaClonerActor.h"
#include "Cloner/Layouts/AvaClonerCircleLayout.h"
#include "Cloner/Layouts/AvaClonerCylinderLayout.h"
#include "Cloner/Layouts/AvaClonerGridLayout.h"
#include "Cloner/Layouts/AvaClonerHoneycombLayout.h"
#include "Cloner/Layouts/AvaClonerLayoutBase.h"
#include "Cloner/Layouts/AvaClonerLineLayout.h"
#include "Cloner/Layouts/AvaClonerMeshLayout.h"
#include "Cloner/Layouts/AvaClonerSphereRandomLayout.h"
#include "Cloner/Layouts/AvaClonerSphereUniformLayout.h"
#include "Cloner/Layouts/AvaClonerSplineLayout.h"
#include "Engine/Engine.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"

UAvaClonerSubsystem::FOnCVarChanged UAvaClonerSubsystem::OnCVarChangedDelegate;
#endif

UAvaClonerSubsystem* UAvaClonerSubsystem::Get()
{
	if (GEngine)
	{
		return GEngine->GetEngineSubsystem<UAvaClonerSubsystem>();
	}
	
	return nullptr;
}

void UAvaClonerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Register them here to match old order of layout enum
	RegisterLayoutClass(UAvaClonerGridLayout::StaticClass());
	RegisterLayoutClass(UAvaClonerLineLayout::StaticClass());
	RegisterLayoutClass(UAvaClonerCircleLayout::StaticClass());
	RegisterLayoutClass(UAvaClonerCylinderLayout::StaticClass());
	RegisterLayoutClass(UAvaClonerSphereUniformLayout::StaticClass());
	RegisterLayoutClass(UAvaClonerHoneycombLayout::StaticClass());
	RegisterLayoutClass(UAvaClonerMeshLayout::StaticClass());
	RegisterLayoutClass(UAvaClonerSplineLayout::StaticClass());
	RegisterLayoutClass(UAvaClonerSphereRandomLayout::StaticClass());

	// Scan for new layouts
	ScanForRegistrableClasses();

#if WITH_EDITOR
	CVarTSRShadingRejectionFlickeringPeriod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TSR.ShadingRejection.Flickering.Period"));

	if (CVarTSRShadingRejectionFlickeringPeriod)
	{
		CVarTSRShadingRejectionFlickeringPeriod->OnChangedDelegate().AddUObject(this, &UAvaClonerSubsystem::OnTSRShadingRejectionFlickeringPeriodChanged);
	}
#endif
}

void UAvaClonerSubsystem::Deinitialize()
{
	Super::Deinitialize();

#if WITH_EDITOR
	if (CVarTSRShadingRejectionFlickeringPeriod)
	{
		CVarTSRShadingRejectionFlickeringPeriod->OnChangedDelegate().RemoveAll(this);
	}
#endif
}

bool UAvaClonerSubsystem::RegisterLayoutClass(const UClass* InClonerLayoutClass)
{
	if (!IsValid(InClonerLayoutClass))
	{
		return false;
	}

	if (!InClonerLayoutClass->IsChildOf(UAvaClonerLayoutBase::StaticClass())
		|| InClonerLayoutClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsLayoutClassRegistered(InClonerLayoutClass))
	{
		return false;
	}

	const UAvaClonerLayoutBase* CDO = InClonerLayoutClass->GetDefaultObject<UAvaClonerLayoutBase>();

	if (!CDO)
	{
		return false;
	}

	// Check niagara asset is valid
	if (!CDO->IsLayoutValid())
	{
		return false;
	}

	// Does not overwrite existing layouts
	const FName LayoutName = CDO->GetLayoutName();
	if (LayoutClasses.Contains(LayoutName))
	{
		return false;
	}
	
	LayoutClasses.Add(LayoutName, CDO->GetClass());
	
	return true;
}

bool UAvaClonerSubsystem::UnregisterLayoutClass(const UClass* InClonerLayoutClass)
{
	if (!IsValid(InClonerLayoutClass))
	{
		return false;
	}
	
	for (TMap<FName, TSubclassOf<UAvaClonerLayoutBase>>::TIterator It(LayoutClasses); It; ++It)
	{
		if (It->Value.Get() == InClonerLayoutClass)
		{
			It.RemoveCurrent();
			return true;
		}
	}
	
	return false;
}

bool UAvaClonerSubsystem::IsLayoutClassRegistered(const UClass* InClonerLayoutClass)
{
	if (!IsValid(InClonerLayoutClass))
	{
		return false;
	}
	
	for (const TPair<FName, TSubclassOf<UAvaClonerLayoutBase>>& LayoutClassPair : LayoutClasses)
	{
		if (LayoutClassPair.Value.Get() == InClonerLayoutClass)
		{
			return true;
		}
	}
	
	return false;
}

TArray<FName> UAvaClonerSubsystem::GetLayoutNames() const
{
	TArray<FName> LayoutNames;
	LayoutClasses.GenerateKeyArray(LayoutNames);
	return LayoutNames;
}

FName UAvaClonerSubsystem::FindLayoutName(TSubclassOf<UAvaClonerLayoutBase> InLayoutClass) const
{
	if (const FName* Key = LayoutClasses.FindKey(InLayoutClass))
	{
		return *Key;
	}

	return NAME_None;
}

UAvaClonerLayoutBase* UAvaClonerSubsystem::CreateNewLayout(FName InLayoutName, AAvaClonerActor* InClonerActor)
{
	if (!IsValid(InClonerActor))
	{
		return nullptr;
	}

	TSubclassOf<UAvaClonerLayoutBase> const* LayoutClass = LayoutClasses.Find(InLayoutName);
	
	if (!LayoutClass)
	{
		return nullptr;
	}

	return NewObject<UAvaClonerLayoutBase>(InClonerActor, LayoutClass->Get());
}

void UAvaClonerSubsystem::ScanForRegistrableClasses()
{
	for (const UClass* const Class : TObjectRange<UClass>())
	{
		RegisterLayoutClass(Class);
	}
}

#if WITH_EDITOR
void UAvaClonerSubsystem::EnableNoFlicker()
{
	if (IsNoFlickerEnabled())
	{
		return;
	}

	PreviousCVarValue = CVarTSRShadingRejectionFlickeringPeriod->GetInt();
	CVarTSRShadingRejectionFlickeringPeriod->Set(NoFlicker);
}

void UAvaClonerSubsystem::DisableNoFlicker()
{
	if (!IsNoFlickerEnabled())
	{
		return;
	}
	
	if (PreviousCVarValue.IsSet())
	{
		CVarTSRShadingRejectionFlickeringPeriod->Set(PreviousCVarValue.GetValue());
	}
	else
	{
		CVarTSRShadingRejectionFlickeringPeriod->Set(*CVarTSRShadingRejectionFlickeringPeriod->GetDefaultValue());
	}
}

bool UAvaClonerSubsystem::IsNoFlickerEnabled() const
{
	if (!CVarTSRShadingRejectionFlickeringPeriod)
	{
		return false;
	}

	return CVarTSRShadingRejectionFlickeringPeriod->GetInt() == NoFlicker;
}

void UAvaClonerSubsystem::OnTSRShadingRejectionFlickeringPeriodChanged(IConsoleVariable* InCVar) const
{
	if (InCVar == CVarTSRShadingRejectionFlickeringPeriod)
	{
		OnCVarChangedDelegate.Broadcast();
	}
}
#endif