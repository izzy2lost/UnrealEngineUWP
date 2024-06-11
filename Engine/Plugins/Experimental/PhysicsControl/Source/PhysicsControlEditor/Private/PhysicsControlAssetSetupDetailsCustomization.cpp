// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlAssetSetupDetailsCustomization.h"

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

#define LOCTEXT_NAMESPACE "PhysicsControlAssetSetupDetailsCustomization"

//======================================================================================================================
TSharedRef<IDetailCustomization> FPhysicsControlAssetSetupDetailsCustomization::MakeInstance(
	TWeakPtr<FPhysicsControlAssetEditor> InPhysicsControlAssetEditor)
{
	return MakeShared<FPhysicsControlAssetSetupDetailsCustomization>(InPhysicsControlAssetEditor);
}

//======================================================================================================================
void FPhysicsControlAssetSetupDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	BindCommands();

	DetailLayout.HideCategory(TEXT("Profiles"));

}

//======================================================================================================================
void FPhysicsControlAssetSetupDetailsCustomization::BindCommands()
{
	const FPhysicsControlAssetEditorCommands& Commands = FPhysicsControlAssetEditorCommands::Get();

	TSharedPtr<FUICommandList> CommandList = PhysicsControlAssetEditor.Pin()->GetToolkitCommands();

}

#undef LOCTEXT_NAMESPACE
