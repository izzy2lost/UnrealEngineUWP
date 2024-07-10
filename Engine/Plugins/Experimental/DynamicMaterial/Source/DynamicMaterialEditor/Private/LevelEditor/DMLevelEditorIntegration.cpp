// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMLevelEditorIntegration.h"
#include "DMLevelEditorIntegrationInstance.h"
#include "DynamicMaterialEditorModule.h"
#include "LevelEditor.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Modules/ModuleManager.h"
#include "Slate/SDMEditor.h"

namespace UE::DynamicMaterialEditor::Private
{
	FDelegateHandle LevelEditorCreatedHandle;
	FDelegateHandle LevelEditorMapChangeHandle;

	FLevelEditorModule& GetLevelEditorModule()
	{
		return FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	}

	FLevelEditorModule* GetLevelEditorModulePtr()
	{
		return FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
	}

	FLevelEditorModule& LoadLevelEditorModuleChecked()
	{
		return FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	}

	void OnMapTearDown(UWorld* InWorld)
	{
		FDynamicMaterialEditorModule::Get().RemoveBuildRequestForOuter(InWorld);

		FDMLevelEditorIntegrationInstance* Instance = FDMLevelEditorIntegrationInstance::GetMutableIntegrationForWorld(InWorld);

		if (!Instance)
		{
			return;
		}

		Instance->SetLastAssetOpenPartialPath("");

		TSharedPtr<SDMEditor> Editor = Instance->GetEditor();

		if (!Editor.IsValid())
		{
			return;
		}

		UDynamicMaterialModelBase* MaterialModelBase = Editor->GetMaterialModelBase();

		if (!MaterialModelBase)
		{
			return;
		}

		Editor->ResetEditor();

		const FString WorldPath = InWorld->GetPathName();
		const int WorldPathLength = WorldPath.Len();
		const FString ModelPath = MaterialModelBase->GetPathName();

		if (ModelPath.Len() > WorldPathLength && ModelPath.StartsWith(WorldPath))
		{
			switch (ModelPath[WorldPathLength])
			{
				case '.':
				case '/':
				case ':':
					Instance->SetLastAssetOpenPartialPath(ModelPath.RightChop(WorldPath.Len()));
					break;
			}
		}
	}

	void OnMapLoad(UWorld* InWorld)
	{
		FDMLevelEditorIntegrationInstance* Instance = FDMLevelEditorIntegrationInstance::GetMutableIntegrationForWorld(InWorld);

		if (!Instance)
		{
			return;
		}

		const FString PartialAssetPath = Instance->GetLastOpenAssetPartialPath();
		Instance->SetLastAssetOpenPartialPath("");

		if (PartialAssetPath.IsEmpty())
		{
			return;
		}

		TSharedPtr<SDMEditor> Editor = Instance->GetEditor();

		if (!Editor.IsValid())
		{
			return;
		}

		const FString ModelPath = InWorld->GetPathName() + PartialAssetPath;

		if (UObject* Object = FindObject<UObject>(nullptr, *ModelPath, false))
		{
			if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(Object))
			{
				Editor->SetMaterialModelBase(MaterialModel);

				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					FDynamicMaterialEditorModule::Get().AddBuildRequest(EditorOnlyData, false);
				}
			}
		}
	}
}

void FDMLevelEditorIntegration::Initialize()
{
	using namespace UE::DynamicMaterialEditor::Private;

	FLevelEditorModule& LevelEditorModule = LoadLevelEditorModuleChecked();

	LevelEditorCreatedHandle = LevelEditorModule.OnLevelEditorCreated().AddLambda(
		[](TSharedPtr<ILevelEditor> InLevelEditor)
		{
			if (InLevelEditor.IsValid())
			{
				FDMLevelEditorIntegrationInstance::AddIntegration(InLevelEditor.ToSharedRef());
			}
		}
	);

	LevelEditorMapChangeHandle = LevelEditorModule.OnMapChanged().AddLambda(
		[](UWorld* InWorld, EMapChangeType InMapChangeType)
		{
			switch (InMapChangeType)
			{
				case EMapChangeType::TearDownWorld:
					OnMapTearDown(InWorld);
					break;

				case EMapChangeType::LoadMap:
					OnMapLoad(InWorld);
					break;
			}
		}
	);
}

void FDMLevelEditorIntegration::Shutdown()
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (LevelEditorCreatedHandle.IsValid())
	{
		if (FLevelEditorModule* ModulePtr = GetLevelEditorModulePtr())
		{
			ModulePtr->OnLevelEditorCreated().Remove(LevelEditorCreatedHandle);
			LevelEditorCreatedHandle.Reset();

			ModulePtr->OnMapChanged().Remove(LevelEditorMapChangeHandle);
			LevelEditorMapChangeHandle.Reset();
		}
	}

	FDMLevelEditorIntegrationInstance::RemoveIntegrations();
}

TSharedPtr<SDMEditor> FDMLevelEditorIntegration::GetEditorForWorld(UWorld* InWorld)
{
	// If we have an invalid world, return the first level editor integration (for assets)... if possible
	if (!IsValid(InWorld))
	{
		using namespace UE::DynamicMaterialEditor::Private;

		if (TSharedPtr<ILevelEditor> FirstLevelEditor = GetLevelEditorModule().GetFirstLevelEditor())
		{
			InWorld = FirstLevelEditor->GetWorld();
		}

		if (!IsValid(InWorld))
		{
			return nullptr;
		}
	}

	if (const FDMLevelEditorIntegrationInstance* Integration = FDMLevelEditorIntegrationInstance::GetIntegrationForWorld(InWorld))
	{
		return Integration->GetEditor();
	}

	return nullptr;
}

TSharedPtr<SDockTab> FDMLevelEditorIntegration::InvokeTabForWorld(UWorld* InWorld)
{
	// If we have an invalid world, return the first level editor integration (for assets)... if possible
	if (!IsValid(InWorld))
	{
		using namespace UE::DynamicMaterialEditor::Private;

		if (TSharedPtr<ILevelEditor> FirstLevelEditor = GetLevelEditorModule().GetFirstLevelEditor())
		{
			InWorld = FirstLevelEditor->GetWorld();
		}

		if (!IsValid(InWorld))
		{
			return nullptr;
		}
	}

	if (const FDMLevelEditorIntegrationInstance* Integration = FDMLevelEditorIntegrationInstance::GetIntegrationForWorld(InWorld))
	{
		return Integration->InvokeTab();
	}

	return nullptr;
}
