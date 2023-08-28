// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/Package.h"
#include "MVVMInstancedViewModelGeneratedClass.generated.h"

/**
 *
 */
UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMInstancedViewModelGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()

public:
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	
#if WITH_EDITOR
	virtual UClass* GetAuthoritativeClass() override;
#endif

public:
	DECLARE_FUNCTION(K2_CallNativeOnRep);

	virtual void OnPropertyReplicated(UObject* Object, const FProperty* Property);
};
