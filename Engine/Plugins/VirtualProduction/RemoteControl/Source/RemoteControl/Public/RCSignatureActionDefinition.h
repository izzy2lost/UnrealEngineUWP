// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/InstancedStruct.h"
#include "RCSignatureActionDefinition.generated.h"

class FStructOnScope;
struct FRCSignatureAction;
struct FRCSignatureActionContext;
struct FRCSignatureField;

/** Struct containing an Action Instance and handling its Execution */
USTRUCT()
struct FRCSignatureActionDefinition
{
	GENERATED_BODY()

	FRCSignatureActionDefinition() = default;

	REMOTECONTROL_API explicit FRCSignatureActionDefinition(const UScriptStruct* InScriptStruct, const FRCSignatureField& InFieldOwner);

	REMOTECONTROL_API const FRCSignatureAction* GetAction() const;

	REMOTECONTROL_API TSharedRef<FStructOnScope> MakeStructOnScope();

	bool Execute(const FRCSignatureActionContext& InContext) const;

private:
	UPROPERTY()
	TInstancedStruct<FRCSignatureAction> ActionInstance;
};
