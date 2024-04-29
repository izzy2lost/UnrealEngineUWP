// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWorkspaceTabWrapper.h"
#include "Widgets/Layout/SSeparator.h"

void SWorkspaceTabWrapper::Construct( const FArguments& InArgs, TSharedPtr<class FTabInfo> InTabInfo, TSharedPtr<UE::Workspace::FWorkspaceEditor> InWorkspaceEditor, UObject* InDocumentID)
{
	Content = InArgs._Content.Widget;
	WeakWorkspaceEditor = InWorkspaceEditor;
	WeakDocumentObject = InDocumentID;

	// Set-up shared breadcrumb defaults JDB TODO figure out correct padding to align fake title with breadcrumbs
    const FMargin BreadcrumbTrailPadding = FMargin(4.f, 2.f);
    const FSlateBrush* BreadcrumbButtonImage = FAppStyle::GetBrush("BreadcrumbTrail.Delimiter");
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				InTabInfo->CreateHistoryNavigationWidget()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
			]
			// Title text/icon
			+SHorizontalBox::Slot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					//.Padding( 10.0f,5.0f )
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image( this, &SWorkspaceTabWrapper::GetTabIcon )
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SAssignNew(BreadcrumbTrailScrollBox, SScrollBox)
						.Orientation(Orient_Horizontal)
						.ScrollBarVisibility(EVisibility::Collapsed)

						+SScrollBox::Slot()
						.Padding(0.f)
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)
							// show fake 'root' breadcrumb for the title
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(BreadcrumbTrailPadding)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.FillHeight(1.f)
								[
									SNew(STextBlock)
									.Text(this, &SWorkspaceTabWrapper::GetWorkspaceName)
									.TextStyle( FAppStyle::Get(), TEXT("GraphBreadcrumbButtonText") )
									.Visibility( this, &SWorkspaceTabWrapper::IsWorkspaceNameVisible )
								]
								
							]
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(BreadcrumbTrailPadding)
							[
								SNew(SImage)
								.Image( BreadcrumbButtonImage )
								.Visibility( this, &SWorkspaceTabWrapper::IsWorkspaceNameVisible )
								.ColorAndOpacity(FSlateColor::UseForeground())
							]

							// New style breadcrumb
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SAssignNew(BreadcrumbTrail, SBreadcrumbTrail<TSharedPtr<UE::Workspace::FWorkspaceBreadcrumb>>)
								.ButtonStyle(FAppStyle::Get(), "GraphBreadcrumbButton")
								.TextStyle(FAppStyle::Get(), "GraphBreadcrumbButtonText")
								.ButtonContentPadding(BreadcrumbTrailPadding)
								.DelimiterImage(BreadcrumbButtonImage)
								.PersistentBreadcrumbs(false)
								.OnCrumbClicked_Lambda([](const TSharedPtr<UE::Workspace::FWorkspaceBreadcrumb> InBreadcrumb){ InBreadcrumb->OnClicked.ExecuteIfBound(); })
							]
						]
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Orientation(Orient_Horizontal)
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			GetContent()
		]
	];

	RebuildBreadcrumbTrail();
} 

void SWorkspaceTabWrapper::RebuildBreadcrumbTrail() const
{
	BreadcrumbTrail->ClearCrumbs(false);

	TArray<TSharedPtr<UE::Workspace::FWorkspaceBreadcrumb>> Breadcrumbs;
	if (UObject* DocumentID = WeakDocumentObject.Get())
	{
		if (const TSharedPtr<UE::Workspace::FWorkspaceEditor> SharedWorkspaceEditor = WeakWorkspaceEditor.Pin())
		{
			const UE::Workspace::FWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::LoadModuleChecked<UE::Workspace::FWorkspaceEditorModule>("WorkspaceEditor");
			if(const UE::Workspace::FObjectDocumentArgs* DocumentArgs = WorkspaceEditorModule.FindObjectDocumentType(DocumentID->GetClass()->GetClassPathName()))
			{
				if(DocumentArgs->OnGetDocumentBreadcrumbTrail.IsBound())
				{
					DocumentArgs->OnGetDocumentBreadcrumbTrail.Execute(UE::Workspace::FWorkspaceEditorContext(SharedWorkspaceEditor.ToSharedRef(), DocumentID), Breadcrumbs);
				}
			}
		}			
	}

	// Widgets have to be added in reverse order
	for (int32 Index = Breadcrumbs.Num() - 1; Index >= 0; --Index)
	{
		TSharedPtr<UE::Workspace::FWorkspaceBreadcrumb> Breadcrumb = Breadcrumbs[Index];
		BreadcrumbTrail->PushCrumb(	TAttribute<FText>::CreateLambda([Breadcrumb]() { return Breadcrumb->OnGetLabel.IsBound() ? Breadcrumb->OnGetLabel.Execute().Get() : FText::GetEmpty(); }), Breadcrumb);
	}
}

const FSlateBrush* SWorkspaceTabWrapper::GetTabIcon() const
{
	if (UObject* DocumentID = WeakDocumentObject.Get())
	{
		if (const TSharedPtr<UE::Workspace::FWorkspaceEditor> SharedWorkspaceEditor = WeakWorkspaceEditor.Pin())
		{
			const UE::Workspace::FWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::LoadModuleChecked<UE::Workspace::FWorkspaceEditorModule>("WorkspaceEditor");
			if(const UE::Workspace::FObjectDocumentArgs* DocumentArgs = WorkspaceEditorModule.FindObjectDocumentType(DocumentID->GetClass()->GetClassPathName()))
			{
				if(DocumentArgs->OnGetTabIcon.IsBound())
				{
					return DocumentArgs->OnGetTabIcon.Execute(UE::Workspace::FWorkspaceEditorContext(SharedWorkspaceEditor.ToSharedRef(), DocumentID));
				}
			}
		}			
	}
	
	return nullptr;
}

EVisibility SWorkspaceTabWrapper::IsWorkspaceNameVisible() const
{
	if (const TSharedPtr<UE::Workspace::FWorkspaceEditor> SharedWorkspaceEditor = WeakWorkspaceEditor.Pin())
	{
		if (SharedWorkspaceEditor->Workspace)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FText  SWorkspaceTabWrapper::GetWorkspaceName() const
{
	if (const TSharedPtr<UE::Workspace::FWorkspaceEditor> SharedWorkspaceEditor = WeakWorkspaceEditor.Pin())
	{
		if (SharedWorkspaceEditor->Workspace)
		{
			return FText::FromName(SharedWorkspaceEditor->Workspace->GetFName());
		}
	}

	return FText::GetEmpty();
}

	

