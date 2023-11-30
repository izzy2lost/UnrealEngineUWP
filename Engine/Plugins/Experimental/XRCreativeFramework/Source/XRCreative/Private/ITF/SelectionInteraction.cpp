// Copyright Epic Games, Inc. All Rights Reserved.

#include "SelectionInteraction.h"
#include "CollisionQueryParams.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#	include "Editor.h"
#endif


void UXRCreativeSelectionInteraction::Initialize(UTypedElementSelectionSet* InSelectionSet, FActorPredicate InCanSelectCallback)
{
	ensure(InSelectionSet);

	WeakSelectionSet = InSelectionSet;
	CanSelectCallback = MoveTemp(InCanSelectCallback);

	// create click behavior and set ourselves as click target
	ClickBehavior = NewObject<USingleClickInputBehavior>();
	ClickBehavior->Modifiers.RegisterModifier(AddToSelectionModifier, FInputDeviceState::IsShiftKeyDown);
	ClickBehavior->Modifiers.RegisterModifier(ToggleSelectionModifier, FInputDeviceState::IsCtrlKeyDown);
	ClickBehavior->Initialize(this);

	BehaviorSet = NewObject<UInputBehaviorSet>();
	BehaviorSet->Add(ClickBehavior, this);
}


void UXRCreativeSelectionInteraction::Shutdown()
{
	for (const AActor* OwnedElementActor : OwnedElementActors)
	{
		TTypedElementOwner<FActorElementData> Element = ActorElementOwnerStore.UnregisterElementOwner(OwnedElementActor);
		UEngineElementsLibrary::DestroyActorElement(OwnedElementActor, Element);
	}

	OwnedElementActors.Empty();
}


void UXRCreativeSelectionInteraction::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	// update modifier state flags
	if (ModifierID == AddToSelectionModifier)
	{
		bAddToSelectionEnabled = bIsOn;
	}
	else if (ModifierID == ToggleSelectionModifier)
	{
		bToggleSelectionEnabled = bIsOn;
	}
}


FInputRayHit UXRCreativeSelectionInteraction::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	FInputRayHit RayHit;

	FHitResult Result;
	FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
	const bool bBlockingHit = GetWorld()->LineTraceSingleByObjectType(Result,
		ClickPos.WorldRay.Origin, ClickPos.WorldRay.PointAt(999999), QueryParams);
	AActor* HitActor = Result.GetActor();
	if (bBlockingHit && CanSelectCallback(HitActor))
	{
		RayHit.bHit = true;
		RayHit.HitDepth = Result.Distance;
		RayHit.bHasHitNormal = true;
		RayHit.SetHitObject(HitActor);
	}

	return RayHit;
}


FTypedElementHandle UXRCreativeSelectionInteraction::AcquireActorElementHandle(const AActor* Actor, const bool bAllowCreate)
{
	TTypedElementOwnerScopedAccess<FActorElementData> ExistingElement = ActorElementOwnerStore.FindElementOwner(Actor);
	if (ExistingElement)
	{
		return ExistingElement->AcquireHandle();
	}

	if (bAllowCreate)
	{
		TTypedElementOwnerScopedAccess<FActorElementData> NewElement = ActorElementOwnerStore.RegisterElementOwner(Actor, UEngineElementsLibrary::CreateActorElement(Actor));
		if (NewElement)
		{
			OwnedElementActors.Add(Actor);
			return NewElement->AcquireHandle();
		}
	}

	return FTypedElementHandle();
}


void UXRCreativeSelectionInteraction::OnClicked(const FInputDeviceRay& ClickPos)
{
	UTypedElementSelectionSet* SelectionSet = WeakSelectionSet.Get();
	if (!ensure(SelectionSet))
	{
		return;
	}

	FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
	FHitResult Result;
	const bool bBlockingHit = GetWorld()->LineTraceSingleByObjectType(Result,
		ClickPos.WorldRay.Origin, ClickPos.WorldRay.PointAt(999999), QueryParams);
	if (ensure(bBlockingHit))
	{
		ensure(CanSelectCallback(Result.GetActor()));

		FTypedElementHandle HitActorHandle;

#if WITH_EDITOR
		if (GEditor && !GEditor->PlayWorld)
		{
			HitActorHandle = UEngineElementsLibrary::AcquireEditorActorElementHandle(Result.GetActor());
		}
		else
#endif
		{
			HitActorHandle = AcquireActorElementHandle(Result.GetActor(), true);
		}

		// TODO: Handle bAddToSelectionEnabled, bToggleSelectionEnabled
		SelectionSet->ClearSelection(FTypedElementSelectionOptions());
		SelectionSet->SelectElement(HitActorHandle, FTypedElementSelectionOptions());
	}
	else
	{
		SelectionSet->ClearSelection(FTypedElementSelectionOptions());
	}
}
