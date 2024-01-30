// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FAvaTransitionEditorViewModel;
class SWidget;
class UToolMenu;
struct FReadOnlyAssetEditorCustomization;

class FAvaTransitionToolbar : public TSharedFromThis<FAvaTransitionToolbar>
{
public:
	explicit FAvaTransitionToolbar(FAvaTransitionEditorViewModel& InOwner)
		: Owner(InOwner)
	{
	}

	static FName GetTreeToolbarName()
	{
		return TEXT("AvaTransitionTreeToolbar");
	}

	void ExtendEditorToolbar(UToolMenu* InToolbarMenu);

	void ExtendTreeToolbar(UToolMenu* InToolbarMenu);

	void SetupReadOnlyCustomization(FReadOnlyAssetEditorCustomization& InReadOnlyCustomization);

	TSharedRef<SWidget> GenerateTreeToolbarWidget();

private:
	FAvaTransitionEditorViewModel& Owner;
};
