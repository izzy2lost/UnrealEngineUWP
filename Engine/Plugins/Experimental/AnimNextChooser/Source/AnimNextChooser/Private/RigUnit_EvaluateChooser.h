// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Chooser.h"
#include "ControlRigDefines.h"
#include "RigVMFunctions/Animation/RigVMFunction_AnimBase.h"
#include "Units/RigUnit.h"

#include "RigUnit_EvaluateChooser.generated.h"

/*
 * Evaluates a Chooser Table and outputs the selected UObject
 */
USTRUCT(meta = (DisplayName = "Evaluate Chooser", Keywords="Evaluate Chooser", Category = "Chooser", Varying, NodeColor="0.737911 0.099899 0.099899"))
struct FRigUnit_EvaluateChooser : public FRigVMFunction_AnimBase
{
	GENERATED_BODY()

	FRigUnit_EvaluateChooser()
	{
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TObjectPtr<UObject> ContextObject;
	
	UPROPERTY(EditAnywhere, Category = "Chooser", meta = (Input, Constant))
	TObjectPtr<UChooserTable> Chooser;

	UPROPERTY(meta = (Output))
	TObjectPtr<UObject> Result;
};