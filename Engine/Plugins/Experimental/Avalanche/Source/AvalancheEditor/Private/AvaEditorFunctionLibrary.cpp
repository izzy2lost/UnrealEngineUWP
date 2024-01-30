// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEditorFunctionLibrary.h"
#include "AssetToolsModule.h"
#include "AvaBlueprint.h"
#include "AvaScene.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Factories/WorldFactory.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"

#define LOCTEXT_NAMESPACE "AvaEditorFunctionLibrary"

namespace UE::AvaEditor::Private
{
	void ExportBlueprint(UFactory* InWorldFactory, UAvalancheBlueprint* InBlueprint)
	{
		if (!IsValid(InBlueprint) || !IsValid(InWorldFactory))
		{
			return;
		}

		const UPackage* const BlueprintPackage = InBlueprint->GetPackage();

		const FString Path = FPackageName::GetLongPackagePath(BlueprintPackage->GetName()) + TEXT("/Exported");

		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		UWorld* const NewWorld = Cast<UWorld>(AssetTools.CreateAsset(InBlueprint->GetName()
			, Path, UWorld::StaticClass(), InWorldFactory));

		if (!IsValid(NewWorld))
		{
			return;
		}

		AAvaScene::CreateFromBlueprint(NewWorld->PersistentLevel, InBlueprint);
	}
}

void FAvaEditorFunctionLibrary::ExportAvaBlueprintsToWorld(TArray<TWeakObjectPtr<UAvalancheBlueprint>> InWeakBlueprints)
{
	UFactory* const WorldFactory = NewObject<UWorldFactory>(GetTransientPackage());

	FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(WorldFactory);

	if (!WorldFactory || !WorldFactory->ConfigureProperties())
	{
		return;
	}

	FEditorDelegates::OnNewAssetCreated.Broadcast(WorldFactory);

	FScopedSlowTask SlowTask(InWeakBlueprints.Num(), LOCTEXT("ExportingBlueprintsToWorld", "Exporting Blueprint(s) to World"));
	SlowTask.MakeDialog();

	for (const TWeakObjectPtr<UAvalancheBlueprint>& WeakBlueprint : InWeakBlueprints)
	{
		if (UAvalancheBlueprint* Blueprint = WeakBlueprint.Get())
		{
			SlowTask.EnterProgressFrame(1
				, FText::Format(LOCTEXT("ExportingBlueprintToWorld", "Exporting Blueprint '{0}'")
				, FText::FromString(Blueprint->GetFriendlyName())));

			UE::AvaEditor::Private::ExportBlueprint(WorldFactory, Blueprint);
		}
		else
		{
			SlowTask.EnterProgressFrame(1);
		}
	}
}

#undef LOCTEXT_NAMESPACE
