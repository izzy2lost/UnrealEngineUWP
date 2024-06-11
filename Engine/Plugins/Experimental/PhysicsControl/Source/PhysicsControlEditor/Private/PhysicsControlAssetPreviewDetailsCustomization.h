// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Types/SlateEnums.h"
#include "Input/Reply.h"

class FPhysicsControlAssetEditor;
class IDetailLayoutBuilder;
class FUICommandInfo;
class SWidget;
class IPropertyHandle;
class FUICommandList;
class SEditableTextBox;

class FPhysicsControlAssetPreviewDetailsCustomization : public IDetailCustomization
{
public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance(TWeakPtr<FPhysicsControlAssetEditor> InPhysicsControlAssetEditor);

	FPhysicsControlAssetPreviewDetailsCustomization(TWeakPtr<FPhysicsControlAssetEditor> InPhysicsControlAssetEditor)
		: PhysicsControlAssetEditor(InPhysicsControlAssetEditor)
	{}

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	void BindCommands();

	FReply InvokeControlProfile(FName ProfileName);

private:
	TWeakPtr<FPhysicsControlAssetEditor> PhysicsControlAssetEditor;
};
