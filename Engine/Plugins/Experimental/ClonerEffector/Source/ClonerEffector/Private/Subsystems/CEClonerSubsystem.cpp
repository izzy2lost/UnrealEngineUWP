// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/CEClonerSubsystem.h"

#include "Cloner/CEClonerComponent.h"
#include "Cloner/Extensions/CEClonerExtensionBase.h"
#include "Cloner/Layouts/CEClonerCircleLayout.h"
#include "Cloner/Layouts/CEClonerCylinderLayout.h"
#include "Cloner/Layouts/CEClonerGridLayout.h"
#include "Cloner/Layouts/CEClonerHoneycombLayout.h"
#include "Cloner/Layouts/CEClonerLayoutBase.h"
#include "Cloner/Layouts/CEClonerLineLayout.h"
#include "Cloner/Layouts/CEClonerMeshLayout.h"
#include "Cloner/Layouts/CEClonerSphereRandomLayout.h"
#include "Cloner/Layouts/CEClonerSphereUniformLayout.h"
#include "Cloner/Layouts/CEClonerSplineLayout.h"
#include "Engine/Engine.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

UCEClonerSubsystem::FOnSubsystemInitialized UCEClonerSubsystem::OnSubsystemInitializedDelegate;

UCEClonerSubsystem* UCEClonerSubsystem::Get()
{
	if (GEngine)
	{
		return GEngine->GetEngineSubsystem<UCEClonerSubsystem>();
	}

	return nullptr;
}

void UCEClonerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Register them here to match old order of layout enum
	RegisterLayoutClass(UCEClonerGridLayout::StaticClass());
	RegisterLayoutClass(UCEClonerLineLayout::StaticClass());
	RegisterLayoutClass(UCEClonerCircleLayout::StaticClass());
	RegisterLayoutClass(UCEClonerCylinderLayout::StaticClass());
	RegisterLayoutClass(UCEClonerSphereUniformLayout::StaticClass());
	RegisterLayoutClass(UCEClonerHoneycombLayout::StaticClass());
	RegisterLayoutClass(UCEClonerMeshLayout::StaticClass());
	RegisterLayoutClass(UCEClonerSplineLayout::StaticClass());
	RegisterLayoutClass(UCEClonerSphereRandomLayout::StaticClass());

	// Scan for new layouts
	ScanForRegistrableClasses();

	OnSubsystemInitializedDelegate.Broadcast();
}

bool UCEClonerSubsystem::RegisterLayoutClass(UClass* InClonerLayoutClass)
{
	if (!IsValid(InClonerLayoutClass))
	{
		return false;
	}

	if (!InClonerLayoutClass->IsChildOf(UCEClonerLayoutBase::StaticClass())
		|| InClonerLayoutClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsLayoutClassRegistered(InClonerLayoutClass))
	{
		return false;
	}

	const UCEClonerLayoutBase* CDO = InClonerLayoutClass->GetDefaultObject<UCEClonerLayoutBase>();

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
	if (LayoutName.IsNone() || LayoutClasses.Contains(LayoutName))
	{
		return false;
	}

	LayoutClasses.Add(LayoutName, CDO->GetClass());

	return true;
}

bool UCEClonerSubsystem::UnregisterLayoutClass(UClass* InClonerLayoutClass)
{
	if (!IsValid(InClonerLayoutClass))
	{
		return false;
	}

	TSubclassOf<UCEClonerLayoutBase> LayoutClass(InClonerLayoutClass);
	if (const FName* LayoutName = LayoutClasses.FindKey(LayoutClass))
	{
		LayoutClasses.Remove(*LayoutName);
		return true;
	}

	return false;
}

bool UCEClonerSubsystem::IsLayoutClassRegistered(UClass* InClonerLayoutClass)
{
	if (!IsValid(InClonerLayoutClass))
	{
		return false;
	}

	TSubclassOf<UCEClonerLayoutBase> LayoutClass(InClonerLayoutClass);
	if (const FName* LayoutName = LayoutClasses.FindKey(LayoutClass))
	{
		return true;
	}

	return false;
}

void UCEClonerSubsystem::RegisterCustomActorResolver(FOnGetOrderedActors InCustomResolver)
{
	ActorResolver = InCustomResolver;
}

void UCEClonerSubsystem::UnregisterCustomActorResolver()
{
	ActorResolver = FOnGetOrderedActors();
}

UCEClonerSubsystem::FOnGetOrderedActors& UCEClonerSubsystem::GetCustomActorResolver()
{
	return ActorResolver;
}

bool UCEClonerSubsystem::RegisterExtensionClass(UClass* InClass)
{
	if (!IsValid(InClass))
	{
		return false;
	}

	if (!InClass->IsChildOf(UCEClonerExtensionBase::StaticClass())
		|| InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsExtensionClassRegistered(InClass))
	{
		return false;
	}

	const UCEClonerExtensionBase* CDO = InClass->GetDefaultObject<UCEClonerExtensionBase>();

	if (!CDO)
	{
		return false;
	}

	const FName ExtensionName = CDO->GetExtensionName();
	if (ExtensionName.IsNone() || ExtensionClasses.Contains(ExtensionName))
	{
		return false;
	}

	ExtensionClasses.Add(ExtensionName, CDO->GetClass());

	return true;
}

bool UCEClonerSubsystem::UnregisterExtensionClass(UClass* InClass)
{
	if (!IsValid(InClass))
	{
		return false;
	}

	TSubclassOf<UCEClonerExtensionBase> ExtensionClass(InClass);
	if (const FName* ExtensionName = ExtensionClasses.FindKey(ExtensionClass))
	{
		ExtensionClasses.Remove(*ExtensionName);
		return true;
	}

	return false;
}

bool UCEClonerSubsystem::IsExtensionClassRegistered(UClass* InClass) const
{
	if (!IsValid(InClass))
	{
		return false;
	}

	TSubclassOf<UCEClonerExtensionBase> ExtensionClass(InClass);
	return !!ExtensionClasses.FindKey(ExtensionClass);
}

TArray<FName> UCEClonerSubsystem::GetExtensionNames() const
{
	TArray<FName> ExtensionNames;
	ExtensionClasses.GenerateKeyArray(ExtensionNames);
	return ExtensionNames;
}

FName UCEClonerSubsystem::FindExtensionName(TSubclassOf<UCEClonerExtensionBase> InClass) const
{
	if (const FName* Key = ExtensionClasses.FindKey(InClass))
	{
		return *Key;
	}

	return NAME_None;
}

UCEClonerExtensionBase* UCEClonerSubsystem::CreateNewExtension(FName InExtensionName, UCEClonerComponent* InCloner)
{
	if (!IsValid(InCloner))
	{
		return nullptr;
	}

	TSubclassOf<UCEClonerExtensionBase> const* ExtensionClass = ExtensionClasses.Find(InExtensionName);

	if (!ExtensionClass)
	{
		return nullptr;
	}

	return NewObject<UCEClonerExtensionBase>(InCloner, ExtensionClass->Get(), NAME_None, RF_Transactional);
}

TArray<FName> UCEClonerSubsystem::GetLayoutNames() const
{
	TArray<FName> LayoutNames;
	LayoutClasses.GenerateKeyArray(LayoutNames);
	return LayoutNames;
}

FName UCEClonerSubsystem::FindLayoutName(TSubclassOf<UCEClonerLayoutBase> InLayoutClass) const
{
	if (const FName* Key = LayoutClasses.FindKey(InLayoutClass))
	{
		return *Key;
	}

	return NAME_None;
}

UCEClonerLayoutBase* UCEClonerSubsystem::CreateNewLayout(FName InLayoutName, UCEClonerComponent* InCloner)
{
	if (!IsValid(InCloner))
	{
		return nullptr;
	}

	TSubclassOf<UCEClonerLayoutBase> const* LayoutClass = LayoutClasses.Find(InLayoutName);

	if (!LayoutClass)
	{
		return nullptr;
	}

	return NewObject<UCEClonerLayoutBase>(InCloner, LayoutClass->Get());
}

void UCEClonerSubsystem::ScanForRegistrableClasses()
{
	{
		TArray<UClass*> DerivedLayoutClasses;
		GetDerivedClasses(UCEClonerLayoutBase::StaticClass(), DerivedLayoutClasses, true);

		for (UClass* LayoutClass : DerivedLayoutClasses)
		{
			RegisterLayoutClass(LayoutClass);
		}
	}

	{
		TArray<UClass*> DerivedExtensionClasses;
		GetDerivedClasses(UCEClonerExtensionBase::StaticClass(), DerivedExtensionClasses, true);

		for (UClass* ExtensionClass : DerivedExtensionClasses)
		{
			RegisterExtensionClass(ExtensionClass);
		}
	}
}