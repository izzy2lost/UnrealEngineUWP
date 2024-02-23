// Copyright Epic Games, Inc. All Rights Reserved.

#include "Layout/CategoryDrivenContentBuilderBase.h"

#include "ToolbarRegistrationArgs.h"


#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "ToolElementRegistry.h"
#include "Framework/Commands/UICommandList.h"
#include "Layout/SeparatorBuilder.h"
#include "Layout/SeparatorTemplates.h"
#include "Layout/Visibility.h"

namespace UE::CategoryDrivenContentBuilderBase::Private
{
	const FName StyleName = TEXT("Name");
}

FToolElementRegistry FCategoryDrivenContentBuilderBase::ToolRegistry = FToolElementRegistry::Get();

FCategoryDrivenContentBuilderBase::FCategoryDrivenContentBuilderBase
	(
		FName InBuilderName
	) :
	FToolElementRegistrationArgs( UE::CategoryDrivenContentBuilderBase::Private::StyleName ),
        CategoryReclickBehavior( ECategoryReclickBehavior::NoEffect) ,
	BuilderName( InBuilderName )
{
}

FCategoryDrivenContentBuilderBase::FCategoryDrivenContentBuilderBase
	(
		FCategoryDrivenContentBuilderArgs& Args
	) :
	FToolElementRegistrationArgs(Args.BuilderName ),
	CategoryReclickBehavior( Args.CategoryReclickBehavior ),
	BuilderName(Args.BuilderName)
{
}

FCategoryDrivenContentBuilderBase::~FCategoryDrivenContentBuilderBase()
{
	if (VerticalToolbarElement.IsValid())
	{
		ToolRegistry.UnregisterElement(VerticalToolbarElement.ToSharedRef());
	}
}

TSharedPtr<FToolBarBuilder> FCategoryDrivenContentBuilderBase::GetLoadPaletteToolbar()
{
	return LoadPaletteToolBarBuilder;
}

void FCategoryDrivenContentBuilderBase::InitCategoryToolbarContainerWidget()
{
	if (!CategoryToolbarVBox.IsValid())
	{
		CategoryToolbarVBox = SNew(SVerticalBox)
			.Visibility(CategoryToolbarVisibility);
	}
	else
	{
		CategoryToolbarVBox->ClearChildren();
	}
	CategoryToolbarVBox->AddSlot()
	.Padding(0.f)
	[
		CreateToolbarWidget()
	];
}

void FCategoryDrivenContentBuilderBase::RefreshCategoryToolbarWidget()
{
	FToolElementRegistrationKey Key = FToolElementRegistrationKey(BuilderName, EToolElement::Toolbar);
	VerticalToolbarElement = ToolRegistry.GetToolElementSP(Key);
	const TSharedRef<FToolbarRegistrationArgs> VerticalToolbarRegistrationArgs = MakeShareable<FToolbarRegistrationArgs>(
		new FToolbarRegistrationArgs(LoadPaletteToolBarBuilder.ToSharedRef()));
	
	if (!VerticalToolbarElement.IsValid())
	{
		VerticalToolbarElement = MakeShareable(new FToolElement
			(BuilderName,
			VerticalToolbarRegistrationArgs));
		ToolRegistry.RegisterElement(VerticalToolbarElement.ToSharedRef());
	}

	VerticalToolbarElement->SetRegistrationArgs(VerticalToolbarRegistrationArgs);
	InitCategoryToolbarContainerWidget();
}

TSharedPtr<SWidget> FCategoryDrivenContentBuilderBase::GenerateWidget()
{
	if (!ToolkitWidgetContainerVBox)
	{
		CreateWidget();
	}
	return ToolkitWidgetContainerVBox.ToSharedRef();
}


void FCategoryDrivenContentBuilderBase::CreateWidget()
{
	ToolkitWidgetVBox = ToolkitWidgetVBox.IsValid() ? ToolkitWidgetVBox : SNew(SVerticalBox);
	ToolkitWidgetVBox->ClearChildren();
	RefreshCategoryToolbarWidget();
	ProvideSelectedCategoryContent();

	ToolkitWidgetContainerVBox = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight() [ *FSeparatorTemplates::SmallHorizontalPanelNoBorder()  ]
	+ SVerticalBox::Slot().AutoHeight() [ *FSeparatorTemplates::SmallHorizontalBackgroundNoBorder() ];

	TSharedPtr<SWidget> MainSplitter = 
		SNew(SSplitter)
		.PhysicalSplitterHandleSize(2.0f)
		+ SSplitter::Slot()
		.Resizable(false)
		.SizeRule(SSplitter::SizeToContent)
			[
				CategoryToolbarVBox.ToSharedRef()
			]

		+ SSplitter::Slot()
		.SizeRule(SSplitter::FractionOfParent)
			[
				ToolkitWidgetVBox->AsShared()
			];
	
		ToolkitWidgetContainerVBox->AddSlot()
    	.VAlign(VAlign_Fill)
	    .FillHeight(1)
		[
			MainSplitter->AsShared()
		];
}

FCategoryDrivenContentBuilderArgs::FCategoryDrivenContentBuilderArgs(FName InBuilderName):
                                                                                                      BuilderName( InBuilderName ),
                                                                                                      bShowCategoryButtonLabels(false),
                                                                                                      CategoryReclickBehavior(FCategoryDrivenContentBuilderBase::ECategoryReclickBehavior::NoEffect)
{
}

void FCategoryDrivenContentBuilderBase::SetCategoryButtonLabelVisibility(EVisibility Visibility)
{

	CategoryButtonLabelVisibility = Visibility;
	InitializeCategoryToolbar();
}

void FCategoryDrivenContentBuilderBase::SetCategoryButtonLabelVisibility(bool bIsCategoryButtonLabelVisible)
{
	SetCategoryButtonLabelVisibility(bIsCategoryButtonLabelVisible ? EVisibility::Visible : EVisibility::Collapsed);
}


TSharedRef<SWidget> FCategoryDrivenContentBuilderBase::CreateToolbarWidget() const
{
	return ToolRegistry.GenerateWidget(VerticalToolbarElement.ToSharedRef());
}