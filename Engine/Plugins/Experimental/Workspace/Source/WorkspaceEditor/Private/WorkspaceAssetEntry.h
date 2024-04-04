// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorkspaceAssetEntry.generated.h"

UCLASS()
class UWorkspaceAssetEntry : public UObject
{
	GENERATED_BODY()
public:	
	virtual bool IsAsset() const override;

	UPROPERTY()
	TSoftObjectPtr<UObject> Asset;	
};
