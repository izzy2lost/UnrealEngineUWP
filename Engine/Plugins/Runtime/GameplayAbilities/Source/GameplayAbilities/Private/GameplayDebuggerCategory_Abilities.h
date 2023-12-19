// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU
#include "GameplayTagContainer.h"
#include "GameplayDebuggerCategory.h"

class AActor;
class APlayerController;
class UPackageMap;

class FGameplayDebuggerCategory_Abilities : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Abilities();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	void OnShowGameplayTagsToggle();
	void OnShowGameplayAbilitiesToggle();
	void OnShowGameplayEffectsToggle();
		
protected:

	void DrawGameplayTags(FGameplayDebuggerCanvasContext& CanvasContext, const APlayerController* OwnerPC) const;
	void DrawGameplayAbilities(FGameplayDebuggerCanvasContext& CanvasContext, const APlayerController* OwnerPC) const;
	void DrawGameplayEffects(FGameplayDebuggerCanvasContext& CanvasContext, const APlayerController* OwnerPC) const;


	struct FRepData
	{
		// to aid in NetSerialize
		TWeakObjectPtr<UPackageMap>	ClientPackageMap;

		FGameplayTagContainer OwnedTags;

		struct FGameplayAbilityDebug
		{
			FString Ability;
			FString Source;
			int32 Level;
			bool bIsActive;
		};
		TArray<FGameplayAbilityDebug> Abilities;

		struct FGameplayEffectDebug
		{
			FString Effect;
			FString Context;
			float Duration;
			float Period;
			int32 Stacks;
			float Level;
		};
		TArray<FGameplayEffectDebug> GameplayEffects;

		void Serialize(FArchive& Ar);
	};
	FRepData DataPack;

	bool WrapStringAccordingToViewport(const FString& iStr, FString& oStr, FGameplayDebuggerCanvasContext& CanvasContext, float ViewportWitdh) const;

private:
	bool bShowGameplayTags = true;
	bool bShowGameplayAbilities = true;
	bool bShowGameplayEffects = true;
};

#endif // WITH_GAMEPLAY_DEBUGGER_MENU
