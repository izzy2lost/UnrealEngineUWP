// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdvancedRenamerBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Providers/AdvancedRenamerActorProvider.h"
#include "Slate/SAdvancedRenamerPanel.h"
#include "Templates/SharedPointer.h"
#include "Toolkits/IToolkitHost.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "AdvancedRenamerBlueprintLibrary"

namespace UE::AdvancedRenamer::Private
{
	TSharedRef<SWindow> CreateAdvancedRenamerWindow()
	{
		return
			SNew(SWindow)
			.Title(LOCTEXT("AdvancedRenameWindow", "Rename Actors"))
			.ClientSize(FVector2D(600.0f, 500.0f))
			.SizingRule(ESizingRule::FixedSize)
			.SupportsMaximize(false)
			.SupportsMinimize(false);
	}
}

void UAdvancedRenamerBlueprintLibrary::BP_OpenAdvancedRenamer(UObject* RenameProvider, UUserWidget* UserWidget)
{
	TSharedPtr<SWidget> ParentWidget = nullptr;
	
	if (UserWidget)
	{
		ParentWidget = UserWidget->TakeWidget();
	}

	using namespace UE::AdvancedRenamer::Private;

	TSharedRef<SWindow> AdvancedRenameWindow = CreateAdvancedRenamerWindow();
	AdvancedRenameWindow->SetContent(SNew(SAdvancedRenamerPanel).ObjectProvider(RenameProvider));

	TSharedPtr<SWidget> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(ParentWidget);
	FSlateApplication::Get().AddModalWindow(AdvancedRenameWindow, ParentWindow);
}

void UAdvancedRenamerBlueprintLibrary::OpenAdvancedRenamer(TSharedRef<IAdvancedRenamerProvider> RenameProvider,
	TSharedPtr<IToolkitHost> ToolkitHost)
{
	if (ToolkitHost.IsValid())
	{
		OpenAdvancedRenamer(RenameProvider, ToolkitHost->GetParentWidget());
	}
}

void UAdvancedRenamerBlueprintLibrary::OpenAdvancedRenamer(TSharedRef<IAdvancedRenamerProvider> RenameProvider, TSharedPtr<SWidget> InParentWidget)
{
	using namespace UE::AdvancedRenamer::Private;

	TSharedRef<SWindow> AdvancedRenameWindow = CreateAdvancedRenamerWindow();
	AdvancedRenameWindow->SetContent(SNew(SAdvancedRenamerPanel).SharedProvider(RenameProvider));

	TSharedPtr<SWidget> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(InParentWidget);
	FSlateApplication::Get().AddModalWindow(AdvancedRenameWindow, ParentWindow);
}

void UAdvancedRenamerBlueprintLibrary::BP_OpenAdvancedRenamerForActors(const TArray<AActor*>& Actors, UUserWidget* UserWidget)
{
	TSharedPtr<SWidget> ParentWidget = nullptr;

	if (UserWidget)
	{
		ParentWidget = UserWidget->TakeWidget();
	}

	OpenAdvancedRenamerForActors(Actors, ParentWidget);
}

void UAdvancedRenamerBlueprintLibrary::OpenAdvancedRenamerForActors(const TArray<AActor*>& Actors, TSharedPtr<IToolkitHost> ToolkitHost)
{
	TSharedPtr<SWidget> ParentWidget = nullptr;

	if (ToolkitHost.IsValid())
	{
		ParentWidget = ToolkitHost->GetParentWidget();
	}

	OpenAdvancedRenamerForActors(Actors, ParentWidget);
}

void UAdvancedRenamerBlueprintLibrary::OpenAdvancedRenamerForActors(const TArray<AActor*>& Actors, TSharedPtr<SWidget> ParentWidget)
{
	TArray<TWeakObjectPtr<AActor>> WeakObjects;

	WeakObjects.Reserve(Actors.Num());

	Algo::Transform(
		Actors,
		WeakObjects,
		[](AActor* InActor)
		{
			return TWeakObjectPtr<AActor>(InActor);
		}
	);

	TSharedRef<FAdvancedRenamerActorProvider> ObjectProvider = MakeShared<FAdvancedRenamerActorProvider>();
	ObjectProvider->SetActorList(WeakObjects);

	OpenAdvancedRenamer(ObjectProvider, ParentWidget);
}

TArray<AActor*> UAdvancedRenamerBlueprintLibrary::GetActorsSharingClassesInWorld(const TArray<AActor*>& InActors)
{
	TSet<UClass*> SelectedClasses;
	bool bHasActorClass = false;
	UWorld* World = nullptr;

	// Scan selected items and add valid classes to the selected classes list.
	for (AActor* SelectedActor : InActors)
	{
		if (!IsValid(SelectedActor))
		{
			continue;
		}

		if (!World)
		{
			World = SelectedActor->GetWorld();

			if (!World)
			{
				break;
			}
		}

		UClass* ActorClass = SelectedActor->GetClass();

		/**
		 * If we have a default AActor selected then all actors in the world share a
		 * class with the selected actors. We don't need anything other than the AActor
		 * class to get matches. Empty the array, store that and move on.
		 */
		if (ActorClass == AActor::StaticClass())
		{
			bHasActorClass = true;
			SelectedClasses.Empty();
			break;
		}

		SelectedClasses.Add(ActorClass);
	}

	if (!World)
	{
		return InActors;
	}

	TArray<UClass*> NonInheritingActorClasses;

	if (bHasActorClass)
	{
		NonInheritingActorClasses.Add(AActor::StaticClass());
	}
	else
	{
		for (UClass* ActorClass : SelectedClasses)
		{
			bool bFoundParent = false;

			for (UClass* ActorClassCheck : SelectedClasses)
			{
				if (ActorClass == ActorClassCheck)
				{
					continue;
				}

				if (ActorClass->IsChildOf(ActorClassCheck))
				{
					bFoundParent = true;
					break;
				}
			}

			if (!bFoundParent)
			{
				NonInheritingActorClasses.Add(ActorClass);
			}
		}
	}

	// Create outliner items for all the items matching the class list that are renameable.
	TArray<AActor*> AllActors;
	AllActors.Reserve(InActors.Num());

	for (UClass* ActorClass : NonInheritingActorClasses)
	{
		for (AActor* Actor : TActorRange<AActor>(World, ActorClass))
		{
			AllActors.Add(Actor);
		}
	}

	return AllActors;
}

#undef LOCTEXT_NAMESPACE
