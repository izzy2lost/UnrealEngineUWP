// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IHasContext.h"
#include "IChooserParameterBase.generated.h"

USTRUCT()
struct FChooserParameterBase
{
	GENERATED_BODY()

	virtual void GetDisplayName(FText& OutName) const { }

	virtual void PostLoad() {};
	virtual void Compile(IHasContextClass* Owner, bool bForce) {};

	virtual ~FChooserParameterBase() {}
};