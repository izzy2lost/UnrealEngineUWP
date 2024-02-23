// Copyright Epic Games, Inc. All Rights Reserved.


#include "Layout/CategoryDrivenContentBuilder.h"
#include "ToolkitBuilder.h"
#include "Templates/SharedPointer.h"

FCategoryDrivenContentBuilder ::FCategoryDrivenContentBuilder(FCategoryDrivenContentBuilderArgs& Args):
	FCategoryDrivenContentBuilderBase( Args  )
{
}

FCategoryDrivenContentBuilder::~FCategoryDrivenContentBuilder()
{
	ProvideSelectedCategoryContentDelegate.Unbind();
}

void FCategoryDrivenContentBuilder::ProvideSelectedCategoryContent( FName InActiveCommandName )
{
	ActiveCommandName = InActiveCommandName;
	UpdateWidget();
}


void FCategoryDrivenContentBuilder::InitializeCategoryToolbar()
{
	for ( const TSharedPtr<FUICommandInfo>& Command : ContentLoaderCommands )
	{
		LoadToolPaletteCommandList->MapAction(
			Command,
			FExecuteAction::CreateSP(this, &FCategoryDrivenContentBuilder::ProvideSelectedCategoryContent, Command->GetCommandName()),
			FCanExecuteAction(),
			FGetActionCheckState::CreateSP(this, &FCategoryDrivenContentBuilder::IsActiveToolPalette,
			                               Command->GetCommandName())
		);
		LoadPaletteToolBarBuilder->AddToolBarButton(Command);
	}
}

void FCategoryDrivenContentBuilder::UpdateWidget()
{
	ToolkitWidgetVBox->ClearChildren();

	if ( ProvideSelectedCategoryContentDelegate.IsBound() )
	{
		TSharedRef<SWidget> Widget =  ProvideSelectedCategoryContentDelegate.Execute( ActiveCommandName );
		ToolkitWidgetVBox->AddSlot()
		.FillHeight(1.0f)
		.VAlign(VAlign_Fill)
		 [
		   Widget
		 ];
	}
}

void FCategoryDrivenContentBuilder::SetCommands(TArray<TSharedPtr<FUICommandInfo>> InContentLoaderCommands)
{
	ContentLoaderCommands = InContentLoaderCommands;
	const bool bForceSmallIcons = true; 
	
	Style = FToolkitStyle::Get().GetWidgetStyle<FToolkitWidgetStyle>("FToolkitWidgetStyle");
	LoadToolPaletteCommandList = MakeShared<FUICommandList>();
	LoadPaletteToolBarBuilder = MakeShared<FVerticalToolBarBuilder>(LoadToolPaletteCommandList, FMultiBoxCustomization::None, TSharedPtr<FExtender>(), bForceSmallIcons);
	LoadPaletteToolBarBuilder->SetLabelVisibility( CategoryButtonLabelVisibility );
	ToolkitWidgetVBox = SNew(SVerticalBox);
	InitializeCategoryToolbar();
}
