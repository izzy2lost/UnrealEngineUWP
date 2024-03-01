// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneSettingsCustomization.h"
#include "AvaSceneSettings.h"
#include "DetailLayoutBuilder.h"
#include "IAvaAttributeEditorModule.h"
#include "IAvaSceneRigEditorModule.h"
#include "PropertyHandle.h"
#include "Templates/SharedPointer.h"

TSharedRef<IDetailCustomization> FAvaSceneSettingsCustomization::MakeInstance()
{
	return MakeShared<FAvaSceneSettingsCustomization>();
}

void FAvaSceneSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (TSharedPtr<IPropertyHandle> SceneRigHandle = DetailBuilder.GetProperty(UAvaSceneSettings::GetSceneRigPropertyName()))
	{
		IAvaSceneRigEditorModule::Get().CustomizeSceneRig(SceneRigHandle.ToSharedRef(), DetailBuilder);
	}

	if (TSharedPtr<IPropertyHandle> AttributesHandle = DetailBuilder.GetProperty(UAvaSceneSettings::GetSceneAttributesPropertyName()))
	{
		IAvaAttributeEditorModule::Get().CustomizeAttributes(AttributesHandle.ToSharedRef(), DetailBuilder);
	}
}
