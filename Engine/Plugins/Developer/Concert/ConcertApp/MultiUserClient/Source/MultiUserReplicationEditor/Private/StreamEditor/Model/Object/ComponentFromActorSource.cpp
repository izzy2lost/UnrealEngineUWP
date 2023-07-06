// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComponentFromActorSource.h"

#include "StreamEditor/Model/DisplayUtils.h"
#include "StreamEditor/Model/Object/IObjectSelectionSourceModel.h"

#include "Algo/AnyOf.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet2/ComponentEditorUtils.h"

#define LOCTEXT_NAMESPACE "FComponentFromActorSource"

namespace UE::MultiUserReplicationEditor
{
	bool FComponentFromActorSource_Base::TrySetActorIfValid(AActor* InComponentSource)
	{
		if (InComponentSource
			// Excludes e.g. foliage and world settings
			&& InComponentSource->IsListedInSceneOutliner())
		{
			ComponentSource = InComponentSource;
			return true;
		}
		return false;
	}

	uint32 FComponentFromActorSource_Base::GetNumSelectableItems() const
	{
		return ComponentSource.IsValid() ? ComponentSource->GetComponents().Num() : 0;
	}

	void FComponentFromActorSource_Base::EnumerateSelectableItems(TFunctionRef<EBreakBehavior(const FSelectableObjectInfo& SelectableOption)> Delegate) const
	{
		if (!ComponentSource.IsValid())
		{
			return;
		}

		for (UActorComponent* Component : ComponentSource->GetComponents())
		{
			if (IsComponentValidReplicationObject(Component)
				&& Delegate({ *Component }) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}
	
	bool FComponentFromActorSource_Base::IsComponentValidReplicationObject(UActorComponent* Component)
	{
		const USceneComponent* AsSceneComponent = Cast<USceneComponent>(Component);
		const bool bIsParentValid = AsSceneComponent
			&& AsSceneComponent->GetAttachParent()
			&& Component->GetOwner() != nullptr
			&& AsSceneComponent->GetAttachParent()->GetOwner() == Component->GetOwner();
		const USceneComponent* Parent = bIsParentValid ? AsSceneComponent->GetAttachParent() : nullptr;

		constexpr bool bAllowUserContructionScriptComps = true;
		return  Component && FComponentEditorUtils::CanEditComponentInstance(Component, Parent, bAllowUserContructionScriptComps);
	}

	ConcertSharedSlate::FSourceDisplayInfo FComponentFromActorSource_ContextMenu::GetDisplayInfo() const
	{
		const FString ActorLabel = GetComponentSource().IsValid() ? DisplayUtils::GetObjectDisplayString(*GetComponentSource().Get()) : TEXT("None");
		const FText ActorLabelText = FText::FromString(ActorLabel);
		return
		{
			{
			LOCTEXT("ContextMenu.Label", "Add component"),
			FText::Format(LOCTEXT("ContextMenu.Tooltip", "Adds a component from {0}"), ActorLabelText),
				FSlateIcon()
			},
			ConcertSharedSlate::ESourceType::ShowAsList
		};
	}

	void FComponentFromActorSource_Root::AddComponentsAdditionToRootCategory(IObjectSourceModel& ActorSource, FObjectSourceCategory& Category)
	{
		FObjectSourceCategory SubCategory
		{
			{ LOCTEXT("AddComponent.Label", "Add Component"), LOCTEXT("AddComponent.Tooltip", "Add a component from an actor to the list.") },
			{}
		};

		// The attribute's get function is called everytime the sub-category's menu entry is hovered.
		// Once a lazy adder has been created once, avoid recreating it again.
		using FActorToSourceMap = TMap<TWeakObjectPtr<AActor>, TSharedRef<FComponentFromActorSource_Root>>; 
		TSharedRef<FActorToSourceMap> CachedLazilyCreatedComponentAdders = MakeShared<FActorToSourceMap>();
		
		ActorSource.EnumerateSelectableItems([&SubCategory, CachedLazilyCreatedComponentAdders](const FSelectableObjectInfo& SelectableOption) mutable
		{
			if (!SelectableOption.Object.IsValid())
			{
				return EBreakBehavior::Continue;
			}
			
			AActor* Actor = Cast<AActor>(SelectableOption.Object.Get());
			if (!Actor)
			{
				return EBreakBehavior::Continue;
			}

			const bool bHasAnyAddeableComponent = Algo::AnyOf(Actor->GetComponents(), [](UActorComponent* Component)
			{
				return IsComponentValidReplicationObject(Component);
			});
			if (!bHasAnyAddeableComponent)
			{
				return EBreakBehavior::Continue;
			}

			// The IObjectSourceModel should be created lazily because there may by thousands of actors in the world
			TAttribute<TSharedPtr<IObjectSourceModel>> LazilyCreatedComponentSource = TAttribute<TSharedPtr<IObjectSourceModel>>::CreateLambda([WeakActor = TWeakObjectPtr<AActor>(Actor), CachedLazilyCreatedComponentAdders = MoveTemp(CachedLazilyCreatedComponentAdders)]()
			{
				if (const TSharedRef<FComponentFromActorSource_Root>* Cached = CachedLazilyCreatedComponentAdders->Find(WeakActor))
				{
					return *Cached;
				}
				
				const TSharedRef<FComponentFromActorSource_Root> ComponentAdder = MakeShared<FComponentFromActorSource_Root>();
				if (WeakActor.IsValid())
				{
					ComponentAdder->TrySetActorIfValid(WeakActor.Get());
				}

				CachedLazilyCreatedComponentAdders->Add(WeakActor, ComponentAdder);
				return ComponentAdder;
			});
			SubCategory.Options.Emplace(LazilyCreatedComponentSource);
			
			return EBreakBehavior::Continue;
		});
		
		Category.SubCategories.Emplace(MoveTemp(SubCategory));
	}

	ConcertSharedSlate::FSourceDisplayInfo FComponentFromActorSource_Root::GetDisplayInfo() const
	{
		const FString ActorLabel = GetComponentSource().IsValid() ? DisplayUtils::GetObjectDisplayString(*GetComponentSource().Get()) : TEXT("None");
		const FText ActorLabelText = FText::FromString(ActorLabel);
		const FSlateIcon SlateIcon = GetComponentSource().IsValid() ? DisplayUtils::GetObjectIcon(*GetComponentSource().Get()) : FSlateIcon();
		return {
			{
				ActorLabelText,
			FText::Format(LOCTEXT("ContextMenu.Tooltip", "Adds a component from {0}"), ActorLabelText),
				SlateIcon
			},
			ConcertSharedSlate::ESourceType::ShowAsList
		};
	}
}

#undef LOCTEXT_NAMESPACE