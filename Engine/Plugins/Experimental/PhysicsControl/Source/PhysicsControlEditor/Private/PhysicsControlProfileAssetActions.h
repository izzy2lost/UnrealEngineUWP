// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

#define ENABLE_PHYSICS_CONTROL_PROFILE_ASSET

// Enable/disable the ability to actually create control profile assets in editor
#define ENABLE_PHYSICS_CONTROL_PROFILE_EDITOR 0

class FPhysicsControlProfileAssetActions : public FAssetTypeActions_Base
{
public:
	UClass* GetSupportedClass() const override;
	FText GetName() const override;
	FColor GetTypeColor() const override;
	uint32 GetCategories() override;

#if ENABLE_PHYSICS_CONTROL_PROFILE_EDITOR
	void OpenAssetEditor(
		const TArray<UObject*>& InObjects, 
		TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
#endif
};
