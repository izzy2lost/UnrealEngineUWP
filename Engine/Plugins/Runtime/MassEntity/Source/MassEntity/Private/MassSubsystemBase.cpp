// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassSubsystemBase.h"
#include "HAL/IConsoleManager.h"


namespace UE::Mass::Private
{
bool bRuntimeSubsystemsEnabled = true;

namespace
{
	FAutoConsoleVariableRef AnonymousCVars[] =
	{
		{ TEXT("mass.RuntimeSubsystemsEnabled")
		, bRuntimeSubsystemsEnabled
		, TEXT("true by default, setting to false will prevent auto-creation of game-time Mass-related subsystems. Needs to be set before world loading.")
		, ECVF_Default }
	};
}
} // UE::Mass::Private


//-----------------------------------------------------------------------------
// UMassSubsystemBase
//-----------------------------------------------------------------------------
bool UMassSubsystemBase::AreRuntimeMassSubsystemsAllowed(UObject* Outer)
{
	return UE::Mass::Private::bRuntimeSubsystemsEnabled;
}

bool UMassSubsystemBase::ShouldCreateSubsystem(UObject* Outer) const 
{
	return UMassSubsystemBase::AreRuntimeMassSubsystemsAllowed(Outer) && Super::ShouldCreateSubsystem(Outer);
}

//-----------------------------------------------------------------------------
// UMassTickableSubsystemBase
//-----------------------------------------------------------------------------
bool UMassTickableSubsystemBase::ShouldCreateSubsystem(UObject* Outer) const
{
	return UMassSubsystemBase::AreRuntimeMassSubsystemsAllowed(Outer) && Super::ShouldCreateSubsystem(Outer);
}
