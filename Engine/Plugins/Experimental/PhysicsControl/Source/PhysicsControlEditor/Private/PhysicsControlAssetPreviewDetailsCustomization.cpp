// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlAssetPreviewDetailsCustomization.h"

#include "PhysicsControlAssetActions.h"
#include "PhysicsControlAssetEditor.h"
#include "PhysicsControlAssetEditorData.h"
#include "PhysicsControlAssetEditorCommands.h"
#include "PhysicsControlAsset.h"
#include "PhysicsControlComponent.h"

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

#define LOCTEXT_NAMESPACE "PhysicsControlAssetPreviewDetailsCustomization"

//======================================================================================================================
TSharedRef<IDetailCustomization> FPhysicsControlAssetPreviewDetailsCustomization::MakeInstance(
	TWeakPtr<FPhysicsControlAssetEditor> InPhysicsControlAssetEditor)
{
	return MakeShared<FPhysicsControlAssetPreviewDetailsCustomization>(InPhysicsControlAssetEditor);
}

//======================================================================================================================
void FPhysicsControlAssetPreviewDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	BindCommands();

	DetailLayoutBuilder.HideCategory(TEXT("PreviewMesh"));
	DetailLayoutBuilder.HideCategory(TEXT("Actions"));
	DetailLayoutBuilder.HideCategory(TEXT("Inheritance"));
	DetailLayoutBuilder.HideCategory(TEXT("Setup"));
	DetailLayoutBuilder.HideCategory(TEXT("Profiles"));

	TSharedPtr<FPhysicsControlAssetEditorData> EditorData = PhysicsControlAssetEditor.Pin()->GetEditorData();
	UPhysicsControlAsset* PCA = EditorData->PhysicsControlAsset.Get();
	if (PCA)
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailLayoutBuilder.EditCategory(TEXT("Preview Profiles"));

		for (const TPair<FName, FPhysicsControlControlAndModifierUpdates>& ProfilePair : PCA->Profiles)
		{
			const FName ProfileName = ProfilePair.Key;
			DetailCategoryBuilder.AddCustomRow(
				FText::FromName(ProfileName))
				.NameContent()
				[
					SNew(STextBlock)
						.Font(DetailLayoutBuilder.GetDetailFont())
						.Text(FText::FromName(ProfileName))
				]
				.ValueContent()
				[
					SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.ContentPadding(FMargin(6, 2))
						.Text(LOCTEXT("Invoke", "Invoke"))
						.OnClicked(this, &FPhysicsControlAssetPreviewDetailsCustomization::InvokeControlProfile, ProfileName)
				];
		}
	}

}

//======================================================================================================================
// TODO Is this needed?
void FPhysicsControlAssetPreviewDetailsCustomization::BindCommands()
{
}

//======================================================================================================================
FReply FPhysicsControlAssetPreviewDetailsCustomization::InvokeControlProfile(FName ProfileName)
{
	TSharedPtr<FPhysicsControlAssetEditorData> EditorData = PhysicsControlAssetEditor.Pin()->GetEditorData();
	UPhysicsControlComponent* PCC = EditorData->PhysicsControlComponent.Get();
	if (PCC)
	{
		PCC->InvokeControlProfile(ProfileName);
	}
	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
