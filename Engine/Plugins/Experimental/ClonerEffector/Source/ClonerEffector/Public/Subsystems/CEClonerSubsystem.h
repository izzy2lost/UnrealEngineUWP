// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cloner/Layouts/CEClonerLayoutBase.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CEClonerSubsystem.generated.h"

class UCEClonerExtensionBase;

UCLASS(MinimalAPI)
class UCEClonerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static TMulticastDelegateRegistration<void()>& OnSubsystemInitialized()
	{
		return OnSubsystemInitializedDelegate;
	}

	static TMulticastDelegateRegistration<void(const UWorld*, bool, bool)>& OnClonerSetEnabled()
	{
		return OnClonerSetEnabledDelegate;
	}

	/** Get this subsystem instance */
	CLONEREFFECTOR_API static UCEClonerSubsystem* Get();

	CLONEREFFECTOR_API bool RegisterLayoutClass(UClass* InClonerLayoutClass);

	CLONEREFFECTOR_API bool UnregisterLayoutClass(UClass* InClonerLayoutClass);

	CLONEREFFECTOR_API bool IsLayoutClassRegistered(UClass* InClonerLayoutClass);

	/** Get available cloner layout names to use in dropdown */
	TArray<FName> GetLayoutNames() const;

	/** Based on a layout class, find layout name */
	FName FindLayoutName(TSubclassOf<UCEClonerLayoutBase> InLayoutClass) const;

	/** Creates a new layout instance for a cloner */
	UCEClonerLayoutBase* CreateNewLayout(FName InLayoutName, UCEClonerComponent* InCloner);

	DECLARE_DELEGATE_RetVal_OneParam(TArray<AActor*> /** Children */, FOnGetOrderedActors, const AActor* /** InParent */)
	CLONEREFFECTOR_API void RegisterCustomActorResolver(FOnGetOrderedActors InCustomResolver);

	CLONEREFFECTOR_API void UnregisterCustomActorResolver();

	FOnGetOrderedActors& GetCustomActorResolver();

	CLONEREFFECTOR_API bool RegisterExtensionClass(UClass* InClass);

	CLONEREFFECTOR_API bool UnregisterExtensionClass(UClass* InClass);

	CLONEREFFECTOR_API bool IsExtensionClassRegistered(UClass* InClass) const;

	/** Get available cloner extension names to use */
	TArray<FName> GetExtensionNames() const;

	/** Based on a extension class, find extension name */
	FName FindExtensionName(TSubclassOf<UCEClonerExtensionBase> InClass) const;

	/** Creates a new extension instance for a cloner */
	UCEClonerExtensionBase* CreateNewExtension(FName InExtensionName, UCEClonerComponent* InCloner);

	/** Set cloners state and optionally transact */
	CLONEREFFECTOR_API void SetClonersEnabled(const TSet<UCEClonerComponent*>& InCloners, bool bInEnable, bool bInShouldTransact);

	/** Set cloners state in world and optionally transact */
	CLONEREFFECTOR_API void SetLevelClonersEnabled(const UWorld* InWorld, bool bInEnable, bool bInShouldTransact);

#if WITH_EDITOR
	/** Converts cloners simulation to a mesh */
	CLONEREFFECTOR_API void ConvertCloners(const TSet<UCEClonerComponent*>& InCloners, ECEClonerMeshConversion InMeshConversion);

	/** Create cloners linked effector */
	CLONEREFFECTOR_API void CreateLinkedEffector(const TSet<UCEClonerComponent*>& InCloners);
#endif
protected:
	DECLARE_MULTICAST_DELEGATE(FOnSubsystemInitialized)
	CLONEREFFECTOR_API static FOnSubsystemInitialized OnSubsystemInitializedDelegate;

	/** Delegate to change state of cloners in a world */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnClonerSetEnabled, const UWorld* /** InWorld */, bool /** bInEnabled */, bool /** bInTransact */)
	static FOnClonerSetEnabled OnClonerSetEnabledDelegate;

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem

	void ScanForRegistrableClasses();

	/** Linking name to the layout class */
	UPROPERTY()
	TMap<FName, TSubclassOf<UCEClonerLayoutBase>> LayoutClasses;

	/** Linking name to the extension class */
	UPROPERTY()
	TMap<FName, TSubclassOf<UCEClonerExtensionBase>> ExtensionClasses;

	/** Used to gather ordered actors based on parent */
	FOnGetOrderedActors ActorResolver;
};