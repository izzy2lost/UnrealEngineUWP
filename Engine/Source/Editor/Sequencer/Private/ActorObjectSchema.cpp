// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorObjectSchema.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FActorSchema"

namespace UE::Sequencer
{

FText FActorSchema::GetPrettyName(const UObject* Object) const
{
	if (const AActor* Actor = Cast<const AActor>(Object))
	{
		return FText::FromString(Actor->GetActorLabel());
	}
	return FText::FromString(Object->GetName());
}

UObject* FActorSchema::GetParentObject(UObject* Object) const
{
	if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		return Component->GetOwner();
	}

	return nullptr;
}

FObjectSchemaRelevancy FActorSchema::GetRelevancy(const UObject* InObject) const
{
	if (InObject->IsA<AActor>())
	{
		return AActor::StaticClass();
	}
	else if (InObject->IsA<UActorComponent>())
	{
		return UActorComponent::StaticClass();
	}
	return FObjectSchemaRelevancy();
}

TSharedPtr<FExtender> FActorSchema::ExtendObjectBindingMenu(TSharedRef<FUICommandList> CommandList, TWeakPtr<ISequencer> WeakSequencer, TArrayView<UObject* const> ContextSensitiveObjects) const
{
	TArray<AActor*> Actors;
	for (UObject* Object : ContextSensitiveObjects)
	{
		if (AActor* Actor = Cast<AActor>(Object))
		{
			Actors.Add(Actor);
		}
	}

	if (Actors.Num() > 0)
	{
		TSharedRef<FExtender> AddTrackMenuExtender = MakeShared<FExtender>();
		AddTrackMenuExtender->AddMenuExtension(
			SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection,
			EExtensionHook::Before,
			CommandList,
			FMenuExtensionDelegate::CreateRaw(this, &FActorSchema::HandleTrackMenuExtensionAddTrack, WeakSequencer, Actors));
		return AddTrackMenuExtender;
	}

	return nullptr;
}

void FActorSchema::HandleTrackMenuExtensionAddTrack(FMenuBuilder& AddTrackMenuBuilder, TWeakPtr<ISequencer> WeakSequencer, TArray<AActor*> Actors) const
{
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	const TSharedPtr<IClassViewerFilter>& GlobalClassFilter = ClassViewerModule.GetGlobalClassViewerFilter();
	TSharedRef<FClassViewerFilterFuncs> ClassFilterFuncs = ClassViewerModule.CreateFilterFuncs();
	FClassViewerInitializationOptions ClassViewerOptions = {};

	TSet<FName> AllComponentNames;
	AddTrackMenuBuilder.BeginSection("Components", LOCTEXT("ComponentsSection", "Components"));
	{
		for (AActor* Actor : Actors)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!Component)
				{
					continue;
				}

				bool bValidComponent = !Component->IsVisualizationComponent();

				if (GlobalClassFilter.IsValid())
				{
					// Hack - forcibly allow USkeletalMeshComponentBudgeted until FORT-527888
					static const FName SkeletalMeshComponentBudgetedClassName(TEXT("SkeletalMeshComponentBudgeted"));
					if (Component->GetClass()->GetName() == SkeletalMeshComponentBudgetedClassName)
					{
						bValidComponent = true;
					}
					else
					{
						bValidComponent = GlobalClassFilter->IsClassAllowed(ClassViewerOptions, Component->GetClass(), ClassFilterFuncs);
					}
				}

				if (bValidComponent)
				{
					AllComponentNames.Add(Component->GetFName());
				}
			}
		}

		TArray<FName> SortedComponentNames = AllComponentNames.Array();
		Algo::Sort(SortedComponentNames, FNameLexicalLess());

		for (FName ComponentName : SortedComponentNames)
		{
			FUIAction AddComponentAction(FExecuteAction::CreateSP(this, &FActorSchema::HandleAddComponentActionExecute, ComponentName, WeakSequencer, Actors));
			FText AddComponentLabel = FText::FromName(ComponentName);
			FText AddComponentToolTip = FText::Format(LOCTEXT("ComponentToolTipFormat", "Add {0} component"), AddComponentLabel);
			AddTrackMenuBuilder.AddMenuEntry(AddComponentLabel, AddComponentToolTip, FSlateIcon(), AddComponentAction);
		}
	}
	AddTrackMenuBuilder.EndSection();
}

void FActorSchema::HandleAddComponentActionExecute(FName ComponentName, TWeakPtr<ISequencer> WeakSequencer, TArray<AActor*> Actors) const
{
	const FScopedTransaction Transaction(LOCTEXT("AddComponent", "Add Component"));

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer)
	{
		return;
	}

	for (AActor* Actor : Actors)
	{
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component->GetFName() == ComponentName)
			{
				Sequencer->GetHandleToObject(Component);
			}
		}
	}
}

} // namespace UE::Sequencer

#undef LOCTEXT_NAMESPACE