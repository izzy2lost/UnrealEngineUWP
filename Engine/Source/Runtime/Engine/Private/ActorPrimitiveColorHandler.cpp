// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/ActorPrimitiveColorHandler.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/LazySingleton.h"
#include "EngineUtils.h"

FActorPrimitiveColorHandler::FActorPrimitiveColorHandler()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	RegisterPrimitiveColorHandler(NAME_None, [](const AActor*)
	{
		return FLinearColor::White;
	});

	ActivePrimitiveColorHandlerName = NAME_None;
	ActivePrimitiveColorHandler = Handlers.Find(NAME_None);
#endif
}

FActorPrimitiveColorHandler& FActorPrimitiveColorHandler::Get()
{
	return TLazySingleton<FActorPrimitiveColorHandler>::Get();
}

void FActorPrimitiveColorHandler::RegisterPrimitiveColorHandler(FName InHandlerName, const FPrimitiveColorHandler& InHandler)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	check(!Handlers.Contains(InHandlerName));
	Handlers.Add(InHandlerName, InHandler);
	
	ActivePrimitiveColorHandler = Handlers.Find(ActivePrimitiveColorHandlerName);
#endif
}

void FActorPrimitiveColorHandler::UnregisterPrimitiveColorHandler(FName InHandlerName)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	check(!InHandlerName.IsNone());
	check(Handlers.Contains(InHandlerName));
	Handlers.Remove(InHandlerName);
	
	if (InHandlerName == ActivePrimitiveColorHandlerName)
	{
		ActivePrimitiveColorHandlerName = NAME_None;
	}

	ActivePrimitiveColorHandler = Handlers.Find(ActivePrimitiveColorHandlerName);
#endif
}

bool FActorPrimitiveColorHandler::SetActivePrimitiveColorHandler(FName InHandlerName, UWorld* InWorld)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (FPrimitiveColorHandler* NewActivePrimitiveColorHandler = Handlers.Find(InHandlerName); NewActivePrimitiveColorHandler && (NewActivePrimitiveColorHandler != ActivePrimitiveColorHandler))
	{
		ActivePrimitiveColorHandlerName = InHandlerName;
		ActivePrimitiveColorHandler = NewActivePrimitiveColorHandler;

		for (TActorIterator<AActor> It(InWorld); It; ++It)
		{
			TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
			It->GetComponents(PrimitiveComponents);

			for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
			{
				if (PrimitiveComponent->IsRegistered())
				{
					PrimitiveComponent->PushPrimitiveColorToProxy(GetPrimitiveColor(*It));
				}
			}
		}

		return true;
	}
#endif

	return false;
}

FLinearColor FActorPrimitiveColorHandler::GetPrimitiveColor(AActor* InActor)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return (*ActivePrimitiveColorHandler)(InActor);
#else
	return FLinearColor::White;
#endif
}