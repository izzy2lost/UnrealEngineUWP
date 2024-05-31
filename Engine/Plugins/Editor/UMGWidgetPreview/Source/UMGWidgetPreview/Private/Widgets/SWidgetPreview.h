// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StrongObjectPtr.h"
#include "UObject/ObjectPtr.h"
#include "WidgetPreview.h"
#include "Widgets/SCompoundWidget.h"

class UWidget;
class UUserWidget;
class SBorder;
class SRetainerWidget;

namespace UE::UMGWidgetPreview::Private
{
	class FWidgetPreviewToolkit;

	class SWidgetPreview
		: public SCompoundWidget
	{
	public:
		using FSlotWidgetMap = TMap<FName, TObjectPtr<UWidget>>;

		SLATE_BEGIN_ARGS(SWidgetPreview) {}
		SLATE_END_ARGS()

		virtual ~SWidgetPreview() override;

		void Construct(const FArguments& Args, const TSharedRef<FWidgetPreviewToolkit>& InToolkit);

	private:
		void OnWidgetChanged(const EWidgetPreviewWidgetChangeType InChangeType);

		/** Convenience method to get world from the associated viewport. */
		UWorld* GetWorld() const;

	private:
		TWeakPtr<FWidgetPreviewToolkit> WeakToolkit;

		TSharedPtr<SRetainerWidget> RetainerWidget;
		TSharedPtr<SBorder> ContainerWidget;
		TSharedPtr<SWidget> CreatedSlateWidget;

		FDelegateHandle OnWidgetChangedHandle;
	};
}
