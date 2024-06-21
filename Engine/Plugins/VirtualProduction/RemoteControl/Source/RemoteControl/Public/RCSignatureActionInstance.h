// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/InstancedStruct.h"
#include "RCSignatureActionInstance.generated.h"

class FStructOnScope;
struct FRCSignatureAction;
struct FRCSignatureActionContext;
struct FRCSignatureField;

/** Struct containing an Action Instance and handling its Execution */
USTRUCT()
struct FRCSignatureActionInstance
{
	GENERATED_BODY()

	FRCSignatureActionInstance() = default;

	REMOTECONTROL_API explicit FRCSignatureActionInstance(const UScriptStruct* InScriptStruct, const FRCSignatureField& InFieldOwner);

	REMOTECONTROL_API const FRCSignatureAction* GetAction() const;

	REMOTECONTROL_API TSharedRef<FStructOnScope> MakeStructOnScope();

	bool Execute(const FRCSignatureActionContext& InContext) const;

private:
	UPROPERTY()
	TInstancedStruct<FRCSignatureAction> ActionInstance;
};
