// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/CEClonerLibrary.h"

#include "Containers/Set.h"
#include "Subsystems/CEClonerSubsystem.h"

void UCEClonerLibrary::GetClonerLayoutClasses(TSet<TSubclassOf<UCEClonerLayoutBase>>& OutLayoutClasses)
{
	OutLayoutClasses.Empty();

	if (const UCEClonerSubsystem* ClonerSubsystem = UCEClonerSubsystem::Get())
	{
		OutLayoutClasses = ClonerSubsystem->GetLayoutClasses();
	}
}

void UCEClonerLibrary::GetClonerExtensionClasses(TSet<TSubclassOf<UCEClonerExtensionBase>>& OutExtensionClasses)
{
	OutExtensionClasses.Empty();

	if (const UCEClonerSubsystem* ClonerSubsystem = UCEClonerSubsystem::Get())
	{
		OutExtensionClasses = ClonerSubsystem->GetExtensionClasses();
	}
}
