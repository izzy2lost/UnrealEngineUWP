// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoutCompositeComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "HoldoutCompositeModule.h"
#include "HoldoutCompositeSubsystem.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#define LOCTEXT_NAMESPACE "HoldoutComposite"

UHoldoutCompositeComponent::UHoldoutCompositeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHoldoutCompositeComponent::OnRegister()
{
	Super::OnRegister();

	RegisterCompositeImpl();
}

void UHoldoutCompositeComponent::OnUnregister()
{
	UnregisterCompositeImpl();

	Super::OnUnregister();
}

void UHoldoutCompositeComponent::DetachFromComponent(const FDetachmentTransformRules& DetachmentRules)
{
	// Note: We also unregister here while the attached parent pointer is still valid.
	UnregisterCompositeImpl();

	Super::DetachFromComponent(DetachmentRules);
}

void UHoldoutCompositeComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();

	UnregisterCompositeImpl();

	const USceneComponent* SceneComponent = GetAttachParent();
	if (IsValid(SceneComponent))
	{
		const UPrimitiveComponent* ParentPrimitiveComponent = Cast<UPrimitiveComponent>(SceneComponent);
		if (IsValid(ParentPrimitiveComponent))
		{
			RegisterCompositeImpl();
		}
		else
		{
#if WITH_EDITOR
			FNotificationInfo Info(LOCTEXT("CompositeParentNotification",
				"The composite component must be parented to a primitive component."));
			Info.ExpireDuration = 5.0f;

			FSlateNotificationManager::Get().AddNotification(Info);
#endif
		}
	}
}

void UHoldoutCompositeComponent::RegisterCompositeImpl()
{
	UHoldoutCompositeSubsystem* Subsystem = UWorld::GetSubsystem<UHoldoutCompositeSubsystem>(GetWorld());
	UPrimitiveComponent* ParentPrimitiveComponent = Cast<UPrimitiveComponent>(GetAttachParent());

	if (IsValid(Subsystem) && IsValid(ParentPrimitiveComponent))
	{
		Subsystem->RegisterPrimitive(ParentPrimitiveComponent);
	}
}

void UHoldoutCompositeComponent::UnregisterCompositeImpl()
{
	UHoldoutCompositeSubsystem* Subsystem = UWorld::GetSubsystem<UHoldoutCompositeSubsystem>(GetWorld());
	UPrimitiveComponent* ParentPrimitiveComponent = Cast<UPrimitiveComponent>(GetAttachParent());

	if (IsValid(Subsystem) && IsValid(ParentPrimitiveComponent))
	{
		Subsystem->UnregisterPrimitive(ParentPrimitiveComponent);
	}
}

#undef LOCTEXT_NAMESPACE
