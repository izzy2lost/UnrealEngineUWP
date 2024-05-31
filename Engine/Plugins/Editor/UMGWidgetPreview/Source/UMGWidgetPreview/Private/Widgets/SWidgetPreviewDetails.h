// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "Widgets/SCompoundWidget.h"

class FWidgetBlueprintEditor;
class FWidgetPreviewToolkit;
class IDetailsView;
class UWidgetPreview;

namespace UE::UMGWidgetPreview::Private
{
	class SWidgetPreviewDetails : public SCompoundWidget, public FNotifyHook
	{
		SLATE_BEGIN_ARGS(SWidgetPreviewDetails) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, UWidgetPreview* InPreview);

	private:
		void HandleSelectedObjectChanged();

	private:
		TWeakObjectPtr<UWidgetPreview> WeakPreview;
		TSharedPtr<IDetailsView> DetailsView;
	};
}
