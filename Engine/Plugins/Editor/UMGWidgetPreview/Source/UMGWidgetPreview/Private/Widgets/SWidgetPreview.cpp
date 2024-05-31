// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWidgetPreview.h"

#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "WidgetPreview.h"
#include "Slate/SRetainerWidget.h"
#include "WidgetPreviewToolkit.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "SWidgetPreview"

namespace UE::UMGWidgetPreview::Private
{
	void SWidgetPreview::Construct(const FArguments& Args, const TSharedRef<FWidgetPreviewToolkit>& InToolkit)
	{
		WeakToolkit = InToolkit;

		OnWidgetChangedHandle = InToolkit->GetPreview()->OnWidgetChanged().AddSP(this, &SWidgetPreview::OnWidgetChanged);

		CreatedSlateWidget = SNullWidget::NullWidget;

		ContainerWidget = SNew(SBorder)
		[
			CreatedSlateWidget.ToSharedRef()
		];

		OnWidgetChanged(EWidgetPreviewWidgetChangeType::Assignment);

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(RetainerWidget, SRetainerWidget)
				[
					ContainerWidget.ToSharedRef()
				]
			]
		];
	}

	SWidgetPreview::~SWidgetPreview()
    {
    	if (const TSharedPtr<FWidgetPreviewToolkit> Toolkit = WeakToolkit.Pin())
    	{
    		if (UWidgetPreview* Preview = Toolkit->GetPreview())
    		{
    			Preview->OnWidgetChanged().Remove(OnWidgetChangedHandle);
    		}
    	}
    }

	void SWidgetPreview::OnWidgetChanged(const EWidgetPreviewWidgetChangeType InChangeType)
	{
		if (const TSharedPtr<FWidgetPreviewToolkit> Toolkit = WeakToolkit.Pin())
		{
			if (UWidgetPreview* Preview = Toolkit->GetPreview())
			{
				UWorld* World = GetWorld();

				if (UUserWidget* PreviewWidget = Preview->GetOrCreateWidgetInstance(World))
				{
					CreatedSlateWidget = PreviewWidget->TakeWidget();
				}
				else
				{
					CreatedSlateWidget = SNullWidget::NullWidget;
				}

				ContainerWidget->SetContent(CreatedSlateWidget.ToSharedRef());
			}
		}
	}

	UWorld* SWidgetPreview::GetWorld() const
	{
		if (const TSharedPtr<FWidgetPreviewToolkit> Toolkit = WeakToolkit.Pin())
		{
			return Toolkit->GetPreviewWorld();
		}

		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
