// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cloner/Layouts/AvaClonerLayoutBase.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "AvaClonerSubsystem.generated.h"

UCLASS()
class AVALANCHEEFFECTORS_API UAvaClonerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static inline constexpr int32 NoFlicker = 1;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnCVarChanged)
	static FOnCVarChanged OnCVarChangedDelegate;
#endif

	/** Get this subsystem instance */
	static UAvaClonerSubsystem* Get();

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	bool RegisterLayoutClass(const UClass* InClonerLayoutClass);
	bool UnregisterLayoutClass(const UClass* InClonerLayoutClass);
	bool IsLayoutClassRegistered(const UClass* InClonerLayoutClass);

	/** Get available cloner layout names to use in dropdown */
	TArray<FName> GetLayoutNames() const;

	/** Based on a layout class, find layout name */
	FName FindLayoutName(TSubclassOf<UAvaClonerLayoutBase> InLayoutClass) const;

	/** Creates a new layout instance for a cloner actor */
	UAvaClonerLayoutBase* CreateNewLayout(FName InLayoutName, AAvaClonerActor* InClonerActor);

#if WITH_EDITOR
	void EnableNoFlicker();
	void DisableNoFlicker();
	bool IsNoFlickerEnabled() const;

	void OnTSRShadingRejectionFlickeringPeriodChanged(IConsoleVariable* InCVar) const;
#endif

protected:
	void ScanForRegistrableClasses();

	/** Linking name to the layout class */
	UPROPERTY()
	TMap<FName, TSubclassOf<UAvaClonerLayoutBase>> LayoutClasses;

#if WITH_EDITOR
	/** Allows to reduce ghosting artifacts when moving clone instances */
	IConsoleVariable* CVarTSRShadingRejectionFlickeringPeriod = nullptr;

	/** Previous value to restore it when disabled */
	TOptional<int32> PreviousCVarValue;
#endif
};