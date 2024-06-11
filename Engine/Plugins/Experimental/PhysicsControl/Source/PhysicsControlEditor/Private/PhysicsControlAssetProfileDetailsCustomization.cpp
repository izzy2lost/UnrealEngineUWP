// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlAssetProfileDetailsCustomization.h"

#include "PhysicsControlAssetActions.h"
#include "PhysicsControlAssetEditor.h"
#include "PhysicsControlAssetEditorCommands.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EditorFontGlyphs.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "PhysicsControlAssetProfileDetailsCustomization"

//======================================================================================================================
TSharedRef<IDetailCustomization> FPhysicsControlAssetProfileDetailsCustomization::MakeInstance(
	TWeakPtr<FPhysicsControlAssetEditor> InPhysicsControlAssetEditor)
{
	return MakeShared<FPhysicsControlAssetProfileDetailsCustomization>(InPhysicsControlAssetEditor);
}

//======================================================================================================================
void FPhysicsControlAssetProfileDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	BindCommands();

	DetailLayout.HideCategory(TEXT("PreviewMesh"));
	DetailLayout.HideCategory(TEXT("Actions"));
	DetailLayout.HideCategory(TEXT("Inheritance"));
	DetailLayout.HideCategory(TEXT("Setup"));

}

//======================================================================================================================
void FPhysicsControlAssetProfileDetailsCustomization::BindCommands()
{
	const FPhysicsControlAssetEditorCommands& Commands = FPhysicsControlAssetEditorCommands::Get();

	TSharedPtr<FUICommandList> CommandList = PhysicsControlAssetEditor.Pin()->GetToolkitCommands();

}

#undef LOCTEXT_NAMESPACE
