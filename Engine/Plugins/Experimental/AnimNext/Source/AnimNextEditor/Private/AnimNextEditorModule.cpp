// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEditorModule.h"

#include "AnimNextConfig.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Graph/AssetTypeActions.h"
#include "Graph/AnimNextGraphPanelNodeFactory.h"
#include "Param/ParamTypePropertyCustomization.h"
#include "Param/ParameterPickerArgs.h"
#include "Param/ParametersGraphPanelPinFactory.h"
#include "Param/ParamNamePropertyCustomization.h"
#include "Param/SParameterPicker.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "AnimNextEditorModule"

namespace UE::AnimNext::Editor
{

class FModule : public IModule
{
	virtual void StartupModule() override
	{
		// Register settings for user editing
		ISettingsModule& SettingsModule = FModuleManager::Get().LoadModuleChecked<ISettingsModule>("Settings");
		SettingsModule.RegisterSettings("Editor", "General", "AnimNext",
			LOCTEXT("SettingsName", "AnimNext"),
			LOCTEXT("SettingsDescription", "Customize AnimNext Settings."),
			GetMutableDefault<UAnimNextConfig>()
		);
		
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTypeActions_AnimNextGraph = MakeShared<FAssetTypeActions_AnimNextGraph>();
		AssetTools.RegisterAssetTypeActions(AssetTypeActions_AnimNextGraph.ToSharedRef());

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"AnimNextParamType",
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamTypePropertyTypeCustomization>(); }));

		Identifier = MakeShared<FParamNamePropertyTypeIdentifier>();
		PropertyModule.RegisterCustomPropertyTypeLayout(
			FNameProperty::StaticClass()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamNamePropertyTypeCustomization>(); }),
			Identifier);

		AnimNextGraphPanelNodeFactory = MakeShared<FAnimNextGraphPanelNodeFactory>();
		FEdGraphUtilities::RegisterVisualNodeFactory(AnimNextGraphPanelNodeFactory);

		ParametersGraphPanelPinFactory = MakeShared<FParametersGraphPanelPinFactory>();
		FEdGraphUtilities::RegisterVisualPinFactory(ParametersGraphPanelPinFactory);
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

		FEdGraphUtilities::UnregisterVisualNodeFactory(AnimNextGraphPanelNodeFactory);

		FEdGraphUtilities::UnregisterVisualPinFactory(ParametersGraphPanelPinFactory);
	}

	virtual TSharedRef<SWidget> CreateParameterPicker(const FParameterPickerArgs& InArgs) override
	{
		return SNew(SParameterPicker)
			.Args(InArgs);
	}

	TSharedPtr<FAssetTypeActions_AnimNextGraph> AssetTypeActions_AnimNextGraph;

	/** Node factory for the AnimNext graph */
	TSharedPtr<FAnimNextGraphPanelNodeFactory> AnimNextGraphPanelNodeFactory;
	
	/** Pin factory for parameters */
	TSharedPtr<FParametersGraphPanelPinFactory> ParametersGraphPanelPinFactory;

	/** Type identifier for parameter names */
	TSharedPtr<FParamNamePropertyTypeIdentifier> Identifier;
};

}

IMPLEMENT_MODULE(UE::AnimNext::Editor::FModule, AnimNextEditor);

#undef LOCTEXT_NAMESPACE