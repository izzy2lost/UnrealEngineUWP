// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEditorModule.h"

#include "AnimNextConfig.h"
#include "Graph/AnimNextGraphPanelNodeFactory.h"
#include "Param/ParamTypePropertyCustomization.h"
#include "Param/ParameterPickerArgs.h"
#include "Param/ParametersGraphPanelPinFactory.h"
#include "Param/ParamNamePropertyCustomization.h"
#include "Param/SParameterPicker.h"
#include "ISettingsModule.h"
#include "UncookedOnlyUtils.h"
#include "Graph/AnimNextGraph.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlockEntry.h"
#include "Param/AnimNextParameterLibrary.h"
#include "Param/IAnimNextParameterBlockGraphInterface.h"
#include "Param/SParameterBlockView.h"
#include "Param/SParameterLibraryView.h"
#include "Graph/SAnimNextGraphView.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Workspace/AnimNextWorkspaceEditor.h"
#include "Param/ParameterBlockParameterCustomization.h"
#include "Param/AnimNextParameterBlockParameter.h"

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

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"AnimNextParamType",
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamTypePropertyTypeCustomization>(); }));

		Identifier = MakeShared<FParamNamePropertyTypeIdentifier>();
		PropertyModule.RegisterCustomPropertyTypeLayout(
			FNameProperty::StaticClass()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamNamePropertyTypeCustomization>(); }),
			Identifier);

		PropertyModule.RegisterCustomClassLayout(UAnimNextParameterBlockParameter::StaticClass()->GetFName(), 
			FOnGetDetailCustomizationInstance::CreateLambda([] { return MakeShared<FParameterBlockParameterCustomization>(); }));

		AnimNextGraphPanelNodeFactory = MakeShared<FAnimNextGraphPanelNodeFactory>();
		FEdGraphUtilities::RegisterVisualNodeFactory(AnimNextGraphPanelNodeFactory);

		ParametersGraphPanelPinFactory = MakeShared<FParametersGraphPanelPinFactory>();
		FEdGraphUtilities::RegisterVisualPinFactory(ParametersGraphPanelPinFactory);

		FWorkspaceEditor::RegisterAssetDocumentWidget(UAnimNextParameterBlock::StaticClass()->GetFName(), [](TSharedRef<FWorkspaceEditor> InEditor, UObject* InAsset)
		{
			UAnimNextParameterBlock* ParameterBlock = CastChecked<UAnimNextParameterBlock>(InAsset);
			UAnimNextParameterBlock_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(ParameterBlock);
			return SNew(SParameterBlockView, EditorData)
				.OnSelectionChanged_Lambda([WeakEditor = TWeakPtr<FWorkspaceEditor>(InEditor)](const TArray<UObject*>& InObjects)
				{
					if(TSharedPtr<FWorkspaceEditor> Editor = WeakEditor.Pin())
					{
						Editor->SetSelectedObjects(InObjects);
					}
				})
				.OnOpenGraph_Lambda([WeakEditor = TWeakPtr<FWorkspaceEditor>(InEditor)](URigVMGraph* InGraph)
				{
					if(TSharedPtr<FWorkspaceEditor> Editor = WeakEditor.Pin())
					{
						if(IRigVMClientHost* RigVMClientHost = InGraph->GetImplementingOuter<IRigVMClientHost>())
						{
							if(UObject* EditorObject = RigVMClientHost->GetEditorObjectForRigVMGraph(InGraph))
							{
								Editor->OpenDocument(EditorObject, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
							}
						}
					}
				})
				.OnDeleteEntries_Lambda([WeakEditor = TWeakPtr<FWorkspaceEditor>(InEditor)](const TArray<UAnimNextParameterBlockEntry*>& InEntries)
				{
					if(InEntries.Num() > 0)
					{
						if(TSharedPtr<FWorkspaceEditor> Editor = WeakEditor.Pin())
						{
							if(IRigVMClientHost* RigVMClientHost = InEntries[0]->GetImplementingOuter<IRigVMClientHost>())
							{
								for(UAnimNextParameterBlockEntry* Entry : InEntries)
								{
									if(IAnimNextParameterBlockGraphInterface* GraphInterface = Cast<IAnimNextParameterBlockGraphInterface>(Entry))
									{
										if(URigVMGraph* RigVMGraph = GraphInterface->GetGraph())
										{
											if (UObject* EditorObject = RigVMClientHost->GetEditorObjectForRigVMGraph(RigVMGraph))
											{
												Editor->CloseDocumentTab(EditorObject);
											}
										}
									}
								}
							}
						}
					}
				});
		});

		FWorkspaceEditor::RegisterAssetDocumentWidget(UAnimNextParameterLibrary::StaticClass()->GetFName(), [](TSharedRef<FWorkspaceEditor> InEditor, UObject* InAsset)
		{
			UAnimNextParameterLibrary* ParameterLibrary = CastChecked<UAnimNextParameterLibrary>(InAsset);
			return SNew(SParameterLibraryView, ParameterLibrary)
				.OnSelectionChanged_Lambda([WeakEditor = TWeakPtr<FWorkspaceEditor>(InEditor)](const TArray<UObject*>& InObjects)
				{
					if(TSharedPtr<FWorkspaceEditor> Editor = WeakEditor.Pin())
					{
						Editor->SetSelectedObjects(InObjects);
					}
				});
		});

		FWorkspaceEditor::RegisterAssetDocumentWidget(UAnimNextSchedule::StaticClass()->GetFName(), [](TSharedRef<FWorkspaceEditor> InEditor, UObject* InAsset)
		{
			UAnimNextSchedule* Schedule = CastChecked<UAnimNextSchedule>(InAsset);
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
			FDetailsViewArgs DetailsViewArgs;
			DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
			TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
			DetailsView->SetObject(Schedule);
			return DetailsView;
		});

		FWorkspaceEditor::RegisterAssetDocumentWidget(UAnimNextGraph::StaticClass()->GetFName(), [](TSharedRef<FWorkspaceEditor> InEditor, UObject* InAsset)
		{
			UAnimNextGraph* Graph = CastChecked<UAnimNextGraph>(InAsset);
			UAnimNextGraph_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Graph);

			return SNew(SAnimNextGraphView, EditorData)
				.OnOpenGraph_Lambda([WeakEditor = TWeakPtr<FWorkspaceEditor>(InEditor)](URigVMGraph* InGraph)
				{
					if(TSharedPtr<FWorkspaceEditor> Editor = WeakEditor.Pin())
					{
						if(IRigVMClientHost* RigVMClientHost = InGraph->GetImplementingOuter<IRigVMClientHost>())
						{
							if(UObject* EditorObject = RigVMClientHost->GetEditorObjectForRigVMGraph(InGraph))
							{
								Editor->OpenDocument(EditorObject, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
							}
						}
					}
				});
		});
	}

	virtual void ShutdownModule() override
	{
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