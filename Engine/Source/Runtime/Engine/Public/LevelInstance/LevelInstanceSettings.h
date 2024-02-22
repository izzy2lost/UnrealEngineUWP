// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreFwd.h"
#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"
#include "LevelInstance/LevelInstancePropertyOverridePolicy.h"
#include "LevelInstanceSettings.generated.h"

UCLASS(config = Engine, defaultconfig)
class ENGINE_API ULevelInstanceSettings : public UObject
{
	GENERATED_BODY()

public:
	ULevelInstanceSettings();
	
	static ULevelInstanceSettings* Get() { return CastChecked<ULevelInstanceSettings>(ULevelInstanceSettings::StaticClass()->GetDefaultObject()); }

#if WITH_EDITOR
	bool IsPropertyOverrideEnabled() const;

	void DisableLevelInstanceSupport() { bIsLevelInstanceDisabled = true; }
	bool IsLevelInstanceDisabled() const { return bIsLevelInstanceDisabled; }
private:
	friend class ULevelInstanceSubsystem;
	friend class ULevelStreamingLevelInstanceEditorPropertyOverride;
		
	void UpdatePropertyOverridePolicy();
	ULevelInstancePropertyOverridePolicy* GetPropertyOverridePolicy() const { return PropertyOverridePolicy; }
#endif

protected:
	// Keep out of WITH_EDITORONLY_DATA so that it can be properly set in -game
	UPROPERTY(config)
	FString PropertyOverridePolicyClass;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<ULevelInstancePropertyOverridePolicy> PropertyOverridePolicy;

	UPROPERTY(Transient)
	bool bIsLevelInstanceDisabled;
#endif
};