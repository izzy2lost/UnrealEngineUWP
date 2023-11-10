// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "MassSubsystemBase.generated.h"


/** 
 * The sole responsibility of this world subsystem class is to serve functionality common to all 
 * Mass-related UWorldSubsystem-based subsystems, like whether the subsystems should get created at all. 
 */
UCLASS(Abstract)
class MASSENTITY_API UMassSubsystemBase : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static bool AreRuntimeMassSubsystemsAllowed(UObject* Outer);

protected:
	//~USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~End of USubsystem interface
};

/**
 * The sole responsibility of this tickable world subsystem class is to serve functionality common to all
 * Mass-related UTickableWorldSubsystem-based subsystems, like whether the subsystems should get created at all.
 */
UCLASS(Abstract)
class MASSENTITY_API UMassTickableSubsystemBase : public UTickableWorldSubsystem
{
	GENERATED_BODY()

protected:
	//~USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~End of USubsystem interface
};
