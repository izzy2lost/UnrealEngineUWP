// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaTranslucentPriorityModifier.h"

#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Framework/AvalancheGameInstance.h"
#include "Modifiers/ActorModifierCoreBase.h"
#include "Shared/AvaTranslucentPriorityModifierShared.h"

#if WITH_EDITOR
#include "AvaOutliner.h"
#include "AvaOutlinerSubsystem.h"
#include "AvaOutlinerUtils.h"
#endif

#define LOCTEXT_NAMESPACE "AvaTranslucentPriorityModifier"

void UAvaTranslucentPriorityModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("TranslucentPriority"));
	InMetadata.SetCategory(TEXT("Rendering"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Set the translucency sort priority of a primitive component based on different modes"));
#endif

	InMetadata.SetCompatibilityRule([this](const AActor* InActor)
	{
		if (!InActor)
		{
			return false;
		}

		const bool bResult = ForEachComponent<UPrimitiveComponent>([](const UPrimitiveComponent* InComponent)->bool
		{
			/** Stop when we have a primitive component otherwise keep going */
			if (InComponent)
			{
				return false;
			}

			return true;
		}
		, EActorModifierCoreComponentType::All
		, EActorModifierCoreLookup::SelfAndAllChildren
		, InActor);

		return !bResult;
	});
}

void UAvaTranslucentPriorityModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	if (InReason == EActorModifierCoreEnableReason::User)
	{
		CameraActorWeak = GetDefaultCameraActor();
		Mode = EAvaTranslucentPriorityModifierMode::AutoOutlinerTree;
	}
}

void UAvaTranslucentPriorityModifier::SavePreState()
{
	Super::SavePreState();

	UAvaTranslucentPriorityModifierShared* SharedObject = GetShared<UAvaTranslucentPriorityModifierShared>(true);

	AActor* ActorModified = GetModifiedActor();

	ChildrenActorsWeak.Reset();
	PrimitiveComponentsWeak.Reset();
	ForEachComponent<UPrimitiveComponent>([this, &SharedObject, &ActorModified](UPrimitiveComponent* InComponent)->bool
		{
			// Component is already linked to a modifier
			if (const UAvaTranslucentPriorityModifier* ModifierContext = SharedObject->FindModifierContext(InComponent))
			{
				const AActor* ModifierActor = ModifierContext->GetModifiedActor();

				// Skip if a translucent priority modifier is attached on the component owner since it has priority over us
				// or if the modifier handles children and is attached to the current owning actor
				if (ModifierContext != this
					&& (ModifierActor == InComponent->GetOwner()
					|| (ModifierContext->bIncludeChildren && ModifierActor->IsAttachedTo(ActorModified))))
				{
					return true;
				}
			}

			PrimitiveComponentsWeak.Add(InComponent);
			ChildrenActorsWeak.Add(InComponent->GetOwner());

			return true;
		}
		, EActorModifierCoreComponentType::All
		, bIncludeChildren ? EActorModifierCoreLookup::SelfAndAllChildren : EActorModifierCoreLookup::Self);

	SharedObject->SetComponentsState(this, PrimitiveComponentsWeak);

	if (FAvaTransformUpdateModifierExtension* TransformExtension = GetExtension<FAvaTransformUpdateModifierExtension>())
	{
		TransformExtension->TrackActors(ChildrenActorsWeak, true);
	}
}

void UAvaTranslucentPriorityModifier::RestorePreState()
{
	Super::RestorePreState();

	if (FAvaTransformUpdateModifierExtension* TransformExtension = GetExtension<FAvaTransformUpdateModifierExtension>())
	{
		TransformExtension->UntrackActors(ChildrenActorsWeak);
	}

	if (UAvaTranslucentPriorityModifierShared* SharedObject = GetShared<UAvaTranslucentPriorityModifierShared>(false))
	{
		SharedObject->RestoreComponentsState(this, PrimitiveComponentsWeak, false);
	}
}

void UAvaTranslucentPriorityModifier::Apply()
{
	const UAvaTranslucentPriorityModifierShared* SharedObject = GetShared<UAvaTranslucentPriorityModifierShared>(false);

	if (!SharedObject)
	{
		Fail(LOCTEXT("InvalidModifierSharedObject", "The modifier shared object is invalid"));
		return;
	}

	// Used cached component state if present
	if (CachedSortedComponentStates.IsEmpty())
	{
		CachedSortedComponentStates = SharedObject->GetSortedComponentStates(this);
	}

	if (Mode == EAvaTranslucentPriorityModifierMode::Manual)
	{
		// Sets all components with the same priority
		for (const FAvaTranslucentPriorityModifierComponentState* SortedComponentState : CachedSortedComponentStates)
		{
			if (SortedComponentState && SortedComponentState->ModifierWeak == this)
			{
				if (UPrimitiveComponent* Component = SortedComponentState->PrimitiveComponentWeak.Get())
				{
					Component->SetTranslucentSortPriority(SortPriority);
				}
			}
		}
	}
	else
	{
		int32 TranslucentSortPriority = 1;

		// Increment sort priority for each component that this modifier handles
		for (const FAvaTranslucentPriorityModifierComponentState* SortedComponentState : CachedSortedComponentStates)
		{
			if (!SortedComponentState
				|| !SortedComponentState->PrimitiveComponentWeak.IsValid()
				|| !SortedComponentState->ModifierWeak.IsValid())
			{
				continue;
			}

			UPrimitiveComponent* Component = SortedComponentState->PrimitiveComponentWeak.Get();

			if (SortedComponentState->ModifierWeak == this)
			{
				Component->SetTranslucentSortPriority(TranslucentSortPriority);
			}
			else if (UAvaTranslucentPriorityModifier* OtherModifier = SortedComponentState->ModifierWeak.Get())
			{
				if (Component->TranslucencySortPriority != TranslucentSortPriority)
				{
					// Cache to avoid doing the same query for the same result
					OtherModifier->CachedSortedComponentStates = CachedSortedComponentStates;
					OtherModifier->MarkModifierDirty();
				}
			}

			TranslucentSortPriority++;
		}
	}

	CachedSortedComponentStates.Empty();

	Next();
}

void UAvaTranslucentPriorityModifier::OnModifierDisabled(EActorModifierCoreDisableReason InReason)
{
	Super::OnModifierDisabled(InReason);

	if (UAvaTranslucentPriorityModifierShared* SharedObject = GetShared<UAvaTranslucentPriorityModifierShared>(false))
	{
		SharedObject->RestoreComponentsState(this, true);
	}

	if (FAvaTransformUpdateModifierExtension* TransformExtension = GetExtension<FAvaTransformUpdateModifierExtension>())
	{
		TransformExtension->UntrackActors(ChildrenActorsWeak);
	}
}

void UAvaTranslucentPriorityModifier::OnModifiedActorTransformed()
{
	if (Mode == EAvaTranslucentPriorityModifierMode::AutoCameraDistance)
	{
		MarkModifierDirty();
	}
}

void UAvaTranslucentPriorityModifier::PostLoad()
{
	Super::PostLoad();

	// Transfer previous sort priorities and component to shared object
	if (!PreviousSortPriorities.IsEmpty())
	{
		if (UAvaTranslucentPriorityModifierShared* SharedObject = GetShared<UAvaTranslucentPriorityModifierShared>(true))
		{
			for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, int32>& PreviousSortPriority : PreviousSortPriorities)
			{
				if (UPrimitiveComponent* PrimitiveComponent = PreviousSortPriority.Key.Get())
				{
					PrimitiveComponent->TranslucencySortPriority = PreviousSortPriority.Value;
					SharedObject->SaveComponentState(this, PrimitiveComponent, true);
				}
			}

			PreviousSortPriorities.Empty();
		}
	}
}

#if WITH_EDITOR
void UAvaTranslucentPriorityModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	static const FName ModeName = GET_MEMBER_NAME_CHECKED(UAvaTranslucentPriorityModifier, Mode);
	static const FName CameraActorName = GET_MEMBER_NAME_CHECKED(UAvaTranslucentPriorityModifier, CameraActorWeak);
	static const FName SortPriorityName = GET_MEMBER_NAME_CHECKED(UAvaTranslucentPriorityModifier, SortPriority);
	static const FName IncludeChildrenName = GET_MEMBER_NAME_CHECKED(UAvaTranslucentPriorityModifier, bIncludeChildren);

	if (MemberName == ModeName)
	{
		OnModeChanged();
	}
	else if (MemberName == CameraActorName)
	{
		OnCameraActorChanged();
	}
	else if (MemberName == SortPriorityName)
	{
		OnSortPriorityChanged();
	}
	else if (MemberName == IncludeChildrenName)
	{
		OnIncludeChildrenChanged();
	}
}
#endif

void UAvaTranslucentPriorityModifier::SetMode(EAvaTranslucentPriorityModifierMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}

	Mode = InMode;
	OnModeChanged();
}

void UAvaTranslucentPriorityModifier::SetCameraActorWeak(const TWeakObjectPtr<ACameraActor>& InCameraActor)
{
	if (CameraActorWeak == InCameraActor)
	{
		return;
	}

	if (Mode != EAvaTranslucentPriorityModifierMode::AutoCameraDistance)
	{
		return;
	}

	CameraActorWeak = InCameraActor;
	OnCameraActorChanged();
}

void UAvaTranslucentPriorityModifier::SetSortPriority(int32 InSortPriority)
{
	if (SortPriority == InSortPriority)
	{
		return;
	}

	if (Mode != EAvaTranslucentPriorityModifierMode::Manual)
	{
		return;
	}

	SortPriority = InSortPriority;
	OnSortPriorityChanged();
}

void UAvaTranslucentPriorityModifier::SetIncludeChildren(bool bInIncludeChildren)
{
	if (bIncludeChildren == bInIncludeChildren)
	{
		return;
	}

	bIncludeChildren = bInIncludeChildren;
	OnIncludeChildrenChanged();
}

void UAvaTranslucentPriorityModifier::OnSceneTreeTrackedActorChildrenChanged(int32 InIdx,
	const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors,
	const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors)
{
	if (bIncludeChildren)
	{
		MarkModifierDirty();
	}
}

void UAvaTranslucentPriorityModifier::OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx,
	const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors,
	const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors)
{
	// Overwrite parent, don't do anything when children order changed
}

void UAvaTranslucentPriorityModifier::OnSceneTreeTrackedActorRearranged(int32 InIdx, AActor* InRearrangedActor)
{
	Super::OnSceneTreeTrackedActorRearranged(InIdx, InRearrangedActor);

	if (Mode == EAvaTranslucentPriorityModifierMode::AutoOutlinerTree)
	{
		MarkModifierDirty();
	}
}

void UAvaTranslucentPriorityModifier::OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent)
{
	Super::OnRenderStateUpdated(InActor, InComponent);

	const AActor* const ActorModified = GetModifiedActor();

	if (!IsValid(ActorModified))
	{
		return;
	}

	if (!bIncludeChildren && InActor != ActorModified)
	{
		return;
	}

	if (bIncludeChildren && !ChildrenActorsWeak.Contains(InActor))
	{
		const UAvaTranslucentPriorityModifierShared* SharedObject = GetShared<UAvaTranslucentPriorityModifierShared>(false);
		if (!SharedObject)
		{
			return;
		}

		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(InComponent);
		if (!PrimitiveComponent)
		{
			return;
		}

		// If the component is already handled by another modifier, return early
		const UAvaTranslucentPriorityModifier* OtherModifier = SharedObject->FindModifierContext(PrimitiveComponent);
		if (OtherModifier && OtherModifier != this)
		{
			return;
		}
	}

	MarkModifierDirty();
}

void UAvaTranslucentPriorityModifier::OnTransformUpdated(AActor* InActor, bool bInParentMoved)
{
	Super::OnTransformUpdated(InActor, bInParentMoved);

	const AActor* const ActorModified = GetModifiedActor();

	if (!IsValid(ActorModified)
		|| (!bIncludeChildren && InActor != ActorModified)
		|| (bIncludeChildren && !ChildrenActorsWeak.Contains(InActor)))
	{
		return;
	}

	MarkModifierDirty();
}

void UAvaTranslucentPriorityModifier::OnModeChanged()
{
	MarkModifierDirty();
}

void UAvaTranslucentPriorityModifier::OnCameraActorChanged()
{
	if (Mode == EAvaTranslucentPriorityModifierMode::AutoCameraDistance)
	{
		MarkModifierDirty();
	}
}

void UAvaTranslucentPriorityModifier::OnSortPriorityChanged()
{
	if (Mode == EAvaTranslucentPriorityModifierMode::Manual)
	{
		MarkModifierDirty();
	}
}

void UAvaTranslucentPriorityModifier::OnIncludeChildrenChanged()
{
	MarkModifierDirty();
}

ACameraActor* UAvaTranslucentPriorityModifier::GetDefaultCameraActor() const
{
	const AActor* const ActorModified = GetModifiedActor();
	const UWorld* const World = IsValid(ActorModified) ? ActorModified->GetWorld() : nullptr;

	if (IsValid(World))
	{
		for (ACameraActor* const WorldCameraActor : TActorRange<ACameraActor>(World))
		{
			if (!IsValid(WorldCameraActor))
			{
				continue;
			}

			return WorldCameraActor;
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
