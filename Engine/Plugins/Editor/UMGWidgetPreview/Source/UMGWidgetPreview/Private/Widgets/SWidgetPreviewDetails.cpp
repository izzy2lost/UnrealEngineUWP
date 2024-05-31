// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWidgetPreviewDetails.h"

#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "WidgetPreview.h"

namespace UE::UMGWidgetPreview::Private
{
	void SWidgetPreviewDetails::Construct(const FArguments& Args, UWidgetPreview* InPreview)
	{
		WeakPreview = InPreview;

		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.NotifyHook = this;

		DetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);

		HandleSelectedObjectChanged();

		ChildSlot
		[
			DetailsView.ToSharedRef()
		];
	}

	void SWidgetPreviewDetails::HandleSelectedObjectChanged()
	{
		if (UWidgetPreview* Preview = WeakPreview.Get())
		{
			DetailsView->SetObject(Preview);
		}
	}
}
