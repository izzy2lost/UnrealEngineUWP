// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"

class UWorld;
class UPrimitiveComponent;

#define ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

/**
 * FActorPrimitiveColorHandler is a simple mechanism for custom actor coloration registration. Once an actor color
 * handler is registered, it can automatically be activated with the SHOW ACTORCOLORATION <HANDLERNAME> command.
 */
class ENGINE_API FActorPrimitiveColorHandler
{
	using FFunc = TFunction<FLinearColor(const UPrimitiveComponent*)>;

public:
	struct FPrimitiveColorHandler
	{
		FPrimitiveColorHandler(FName InHandlerName, FText InHandlerText, const FFunc& InHandlerFunc)
			: HandlerName(InHandlerName)
			, HandlerText(InHandlerText)
			, HandlerFunc(InHandlerFunc)
		{}

		FName HandlerName;
		FText HandlerText;
		FFunc HandlerFunc;
	};	

	FActorPrimitiveColorHandler();
	static FActorPrimitiveColorHandler& Get();

	void RegisterPrimitiveColorHandler(FName InHandlerName, FText InHandlerText, const FFunc& InHandlerFunc);
	void UnregisterPrimitiveColorHandler(FName InHandlerName);
	void GetRegisteredPrimitiveColorHandlers(TArray<FPrimitiveColorHandler>& OutPrimitiveColorHandlers) const;

	FName GetActivePrimitiveColorHandler() const;
	bool SetActivePrimitiveColorHandler(FName InHandlerName, UWorld* InWorld);

	void RefreshPrimitiveColorHandler(FName InHandlerName, UWorld* InWorld);
	void RefreshPrimitiveColorHandler(FName InHandlerName, const TArray<AActor*>& InActors);
	void RefreshPrimitiveColorHandler(FName InHandlerName, const TArray<UPrimitiveComponent*>& InPrimitiveComponents);

	FLinearColor GetPrimitiveColor(const UPrimitiveComponent* InPrimitiveComponent) const;

private:
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	FName ActivePrimitiveColorHandlerName;
	FText ActivePrimitiveColorHandlerText;
	FPrimitiveColorHandler* ActivePrimitiveColorHandler;
	TMap<FName, FPrimitiveColorHandler> Handlers;
#endif
};