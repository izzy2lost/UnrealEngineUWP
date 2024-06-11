// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/RigUnit_AnimNextBase.h"
#include "RigUnit_AnimNextModuleSelf.generated.h"

class UAnimNextModule;

/** Get a reference to the currently executing module */
USTRUCT(meta=(DisplayName="Self", Category="Module", NodeColor="0, 0, 1", Keywords="Current,This"))
struct ANIMNEXT_API FRigUnit_AnimNextModuleSelf : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	// The currently-executing module
	UPROPERTY(meta=(Output))
	TObjectPtr<UAnimNextModule> Self;
};
