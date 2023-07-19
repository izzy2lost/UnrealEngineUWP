// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEditorModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Graph/AssetTypeActions.h"
#include "Param/ParamTypePropertyCustomization.h"
#include "Param/ParameterPickerArgs.h"
#include "Param/SParameterPicker.h"
#include "Param/ParamType.h"

namespace UE::AnimNext::Editor
{

class FModule : public IModule
{

	virtual void StartupModule() override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTypeActions_AnimNextGraph = MakeShared<FAssetTypeActions_AnimNextGraph>();
		AssetTools.RegisterAssetTypeActions(AssetTypeActions_AnimNextGraph.ToSharedRef());

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"AnimNextParamType",
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamTypePropertyTypeCustomization>(); }));
	}

	virtual void ShutdownModule() override
	{
		if(FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.UnregisterAssetTypeActions(AssetTypeActions_AnimNextGraph.ToSharedRef());
		}
	
		if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomPropertyTypeLayout("AnimNextParamType");
		}
	}

	virtual TSharedRef<SWidget> CreateParameterPicker(const FParameterPickerArgs& InArgs) override
	{
		return SNew(SParameterPicker)
			.Args(InArgs);
	}

	TSharedPtr<FAssetTypeActions_AnimNextGraph> AssetTypeActions_AnimNextGraph;
};

}

IMPLEMENT_MODULE(UE::AnimNext::Editor::FModule, AnimNextEditor);