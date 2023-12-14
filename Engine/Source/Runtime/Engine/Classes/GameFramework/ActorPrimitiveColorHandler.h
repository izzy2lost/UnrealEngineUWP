// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

class AActor;
class UWorld;

/**
 * FActorPrimitiveColorHandler is a simple mechanism for custom actor coloration registration. Once an actor color
 * handler is registered, it can automatically be activated with the SHOW ACTORCOLORATION <HANDLERNAME> command.
 */
class ENGINE_API FActorPrimitiveColorHandler
{
	using FPrimitiveColorHandler = TFunction<FLinearColor(AActor*)>;

public:
	FActorPrimitiveColorHandler();
	static FActorPrimitiveColorHandler& Get();
	void RegisterPrimitiveColorHandler(FName InHandlerName, const FPrimitiveColorHandler& InHandler);
	void UnregisterPrimitiveColorHandler(FName InHandlerName);
	bool SetActivePrimitiveColorHandler(FName InHandlerName, UWorld* InWorld);
	FLinearColor GetPrimitiveColor(AActor* InActor);

private:
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FName ActivePrimitiveColorHandlerName;
	FPrimitiveColorHandler* ActivePrimitiveColorHandler;
	TMap<FName, FPrimitiveColorHandler> Handlers;
#endif
};