// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/SIKRetargetPoseEditor.h"

#include "RetargetEditor/IKRetargetCommands.h"
#include "RetargetEditor/IKRetargetEditor.h"
#include "RetargetEditor/IKRetargetEditorController.h"

#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "SIKRetargetPoseEditor"

void SIKRetargetPoseEditor::Construct(
	const FArguments& InArgs,
	TSharedRef<FIKRetargetEditorController> InEditorController)
{
	EditorController = InEditorController;

	// the editor controller
	FIKRetargetEditorController* Controller = EditorController.Pin().Get();

	// the commands for the menus
	TSharedPtr<FUICommandList> Commands = Controller->Editor.Pin()->GetToolkitCommands();
	
	ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.Padding(2.0f)
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			
			// pose selection label
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4,0)
			[
				SNew(STextBlock).Text(LOCTEXT("CurrentPose", "Current Retarget Pose:"))
			]
						
			// pose selection combobox
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(PoseListComboBox, SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&PoseNames)
				.OnComboBoxOpening(this, &SIKRetargetPoseEditor::Refresh)
				.OnGenerateWidget_Lambda([](TSharedPtr<FName> InItem)
				{
					return SNew(STextBlock).Text(FText::FromName(*InItem.Get()));
				})
				.OnSelectionChanged(Controller, &FIKRetargetEditorController::OnPoseSelected)
				.Content()
				[
					SNew(STextBlock).Text(Controller, &FIKRetargetEditorController::GetCurrentPoseName)
				]
			]

			// pose blending slider
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpinBox<float>)
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				.MinDesiredWidth(100)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.Value(Controller, &FIKRetargetEditorController::GetRetargetPoseAmount)
				.OnValueChanged(Controller, &FIKRetargetEditorController::SetRetargetPoseAmount)
			]
		]

		// pose editing toolbar
		+SVerticalBox::Slot()
		.Padding(2.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Center)
			[
				MakeToolbar(Commands)
			]
		]
	];
}

void SIKRetargetPoseEditor::Refresh()
{
	// get the retarget poses from the editor controller
	const FIKRetargetEditorController* Controller = EditorController.Pin().Get();
	const TMap<FName, FIKRetargetPose>& RetargetPoses = Controller->AssetController->GetRetargetPoses(Controller->GetSourceOrTarget());

	// fill list of pose names
	PoseNames.Reset();
	for (const TTuple<FName, FIKRetargetPose>& Pose : RetargetPoses)
	{
		PoseNames.Add(MakeShareable(new FName(Pose.Key)));
	}

	PoseListComboBox->RefreshOptions();
}

TSharedRef<SWidget>  SIKRetargetPoseEditor::MakeToolbar(TSharedPtr<FUICommandList> Commands)
{
	FToolBarBuilder ToolbarBuilder(Commands, FMultiBoxCustomization::None);
	
	ToolbarBuilder.BeginSection("Edit Current Pose");

	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SIKRetargetPoseEditor::GenerateResetMenuContent, Commands),
		LOCTEXT("ResetPose_Label", "Reset"),
		LOCTEXT("ResetPoseToolTip_Label", "Reset bones to reference pose."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"));

	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("Create Poses");

	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SIKRetargetPoseEditor::GenerateNewMenuContent, Commands),
		LOCTEXT("CreatePose_Label", "Create"),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));

	ToolbarBuilder.AddToolBarButton(
		FIKRetargetCommands::Get().DeleteRetargetPose,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::Get().GetStyleSetName(),"Icons.Delete"));

	ToolbarBuilder.AddToolBarButton(
		FIKRetargetCommands::Get().RenameRetargetPose,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::Get().GetStyleSetName(),"Icons.Settings"));

	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

TSharedRef<SWidget> SIKRetargetPoseEditor::GenerateResetMenuContent(TSharedPtr<FUICommandList> Commands)
{
	FMenuBuilder MenuBuilder(true, Commands);

	MenuBuilder.AddMenuEntry(
		FIKRetargetCommands::Get().ResetSelectedBones,
		TEXT("Reset Selected"),
		TAttribute<FText>(),
		TAttribute<FText>());

	MenuBuilder.AddMenuEntry(
		FIKRetargetCommands::Get().ResetSelectedAndChildrenBones,
		TEXT("Reset Selected And Children"),
		TAttribute<FText>(),
		TAttribute<FText>());

	MenuBuilder.AddMenuEntry(
		FIKRetargetCommands::Get().ResetAllBones,
		TEXT("Reset All"),
		TAttribute<FText>(),
		TAttribute<FText>());

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SIKRetargetPoseEditor::GenerateNewMenuContent(TSharedPtr<FUICommandList> Commands)
{
	const FName ParentEditorName = EditorController.Pin()->Editor.Pin()->GetToolMenuName();
	const FName MenuName = FName(ParentEditorName.ToString() + TEXT(".CreateMenu"));
	UToolMenu* ToolMenu = UToolMenus::Get()->ExtendMenu(MenuName);

	FToolMenuSection& CreateSection = ToolMenu->AddSection("Create", LOCTEXT("CreatePoseOperations", "Create New Retarget Pose"));
	CreateSection.AddMenuEntry(
		FIKRetargetCommands::Get().NewRetargetPose,
		TAttribute<FText>(),
        TAttribute<FText>());
	CreateSection.AddMenuEntry(
		FIKRetargetCommands::Get().DuplicateRetargetPose,
		TAttribute<FText>(),
		TAttribute<FText>());
	
	FToolMenuSection& ImportSection = ToolMenu->AddSection("Import",LOCTEXT("ImportPoseOperations", "Import Retarget Pose"));
	ImportSection.AddMenuEntry(
			FIKRetargetCommands::Get().ImportRetargetPose,
			TAttribute<FText>(),
			TAttribute<FText>());
	ImportSection.AddMenuEntry(
		FIKRetargetCommands::Get().ImportRetargetPoseFromAnim,
		TAttribute<FText>(),
		TAttribute<FText>());

	FToolMenuSection& ExportSection = ToolMenu->AddSection("Export",LOCTEXT("ExportPoseOperations", "Export Retarget Pose"));
	ExportSection.AddMenuEntry(
			FIKRetargetCommands::Get().ExportRetargetPose,
			TAttribute<FText>(),
			TAttribute<FText>());

	return UToolMenus::Get()->GenerateWidget(MenuName, FToolMenuContext(Commands));
}

#undef LOCTEXT_NAMESPACE
