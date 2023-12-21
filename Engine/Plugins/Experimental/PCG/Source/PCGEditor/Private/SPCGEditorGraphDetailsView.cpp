// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphDetailsView.h"

#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGSettings.h"

#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Layout/Visibility.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphDetailsView"

void SPCGEditorGraphDetailsView::Construct(const FArguments& InArgs)
{
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bCustomFilterAreaLocation = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SPCGEditorGraphDetailsView::IsReadOnlyProperty));
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SPCGEditorGraphDetailsView::IsVisibleProperty));

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(0.f, 0.f, 0.f, 0.f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SPCGEditorGraphDetailsView::OnLockButtonClicked)
				.ContentPadding(FMargin(4, 2))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("LockSelectionButton_ToolTip", "Locks the current attribute list view to this selection"))
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(this, &SPCGEditorGraphDetailsView::GetLockIcon)
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Visibility(this, &SPCGEditorGraphDetailsView::GetNameVisibility)
				.Text(this, &SPCGEditorGraphDetailsView::GetName)
			]
			+SHorizontalBox::Slot()
			[
				DetailsView->GetFilterAreaWidget().ToSharedRef()
			]
		]
		+SVerticalBox::Slot()
		[
			DetailsView.ToSharedRef()
		]
	];
}

void SPCGEditorGraphDetailsView::SetObject(UObject* InObject, bool bForceRefresh)
{
	TArray<UObject*> InObjects;
	InObjects.Add(InObject);

	SetObjects(InObjects, bForceRefresh);
}

void SPCGEditorGraphDetailsView::SetObjects(const TArray<UObject*>& InObjects, bool bForceRefresh, bool bOverrideLock)
{
	if (bOverrideLock || !bIsLocked)
	{
		DetailsView->SetObjects(InObjects, bForceRefresh, bOverrideLock);

		// Unlock the view automatically if we are viewing nothing
		if (InObjects.IsEmpty())
		{
			bIsLocked = false;
		}
	}
}

void SPCGEditorGraphDetailsView::SetObjects(const TArray<TWeakObjectPtr<UObject>>& InObjects, bool bForceRefresh, bool bOverrideLock)
{
	TArray<UObject*> Objects;
	Objects.Reserve(InObjects.Num());
	for (const TWeakObjectPtr<UObject>& InObject : InObjects)
	{
		if (InObject.IsValid())
		{
			Objects.Add(InObject.Get());
		}
	}

	SetObjects(Objects, bForceRefresh, bOverrideLock);
}

const TArray<TWeakObjectPtr<UObject>>& SPCGEditorGraphDetailsView::GetSelectedObjects() const
{
	return DetailsView->GetSelectedObjects();
}

FReply SPCGEditorGraphDetailsView::OnLockButtonClicked()
{
	bIsLocked = !bIsLocked;
	return FReply::Handled();
}

const FSlateBrush* SPCGEditorGraphDetailsView::GetLockIcon() const
{
	return FAppStyle::GetBrush(bIsLocked ? TEXT("PropertyWindow.Locked") : TEXT("PropertyWindow.Unlocked"));
}

EVisibility SPCGEditorGraphDetailsView::GetNameVisibility() const
{
	return bIsLocked ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SPCGEditorGraphDetailsView::GetName() const
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = GetSelectedObjects();
	if (SelectedObjects.IsEmpty())
	{
		// Implementation note: this doesn't really happen since we reselect the graph settings when unselecting
		return LOCTEXT("NoObjectsSelected", "Empty selection");
	}
	else if (SelectedObjects.Num() == 1)
	{
		UObject* SelectedObject = SelectedObjects[0].Get();
		UPCGNode* OwnerNode = nullptr;

		if (Cast<UPCGSettings>(SelectedObject) != nullptr || Cast<UPCGSettingsInstance>(SelectedObject) != nullptr)
		{
			OwnerNode = Cast<UPCGNode>(SelectedObject->GetOuter());
		}

		if (OwnerNode)
		{
			return OwnerNode->GetNodeTitle(EPCGNodeTitleType::ListView);
		}
		else if (UPCGGraph* Graph = Cast<UPCGGraph>(SelectedObject))
		{
			return LOCTEXT("GraphSettingsSelected", "Graph Settings");
		}
		else
		{
			return FText::FromName(SelectedObject->GetFName());
		}
	}
	else
	{
		return FText::Format(LOCTEXT("MultipleObjectsSelectedFmt", "{0} nodes"), FText::AsNumber(SelectedObjects.Num()));
	}
}

bool SPCGEditorGraphDetailsView::IsReadOnlyProperty(const FPropertyAndParent& InPropertyAndParent) const
{
	// Everything is writable when not in an instance
	if (!DetailsView ||
		InPropertyAndParent.ParentProperties.IsEmpty() ||
		InPropertyAndParent.ParentProperties.Last()->GetFName() != GET_MEMBER_NAME_CHECKED(UPCGSettingsInstance, Settings))
	{
		return false;
	}

	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailsView->GetSelectedObjects();
	for (const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjects)
	{
		if (!SelectedObject.IsValid())
		{
			continue;
		}

		if (UPCGSettingsInstance* Instance = Cast<UPCGSettingsInstance>(SelectedObject.Get()))
		{
			return true;
		}
	}

	return false;
}

bool SPCGEditorGraphDetailsView::IsVisibleProperty(const FPropertyAndParent& InPropertyAndParent) const
{
	if (!DetailsView)
	{
		return true;
	}

	// Currently never hide anything from the graph settings
	if (InPropertyAndParent.Objects.Num() == 1 && Cast<UPCGGraph>(InPropertyAndParent.Objects[0]))
	{
		return true;
	}

	// Always hide asset info information
	if (InPropertyAndParent.Property.HasMetaData(TEXT("Category")) &&
		InPropertyAndParent.Property.GetMetaData(TEXT("Category")) == TEXT("AssetInfo"))
	{
		return false;
	}

	// Otherwise, everything is visible when not in an instance
	if (InPropertyAndParent.ParentProperties.IsEmpty() ||
		InPropertyAndParent.ParentProperties.Last()->GetFName() != GET_MEMBER_NAME_CHECKED(UPCGSettingsInstance, Settings))
	{
		return true;
	}

	// Hide debug settings from the setting when showing the instance settings.
	if (InPropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, bEnabled) ||
		InPropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, bDebug) ||
		(InPropertyAndParent.ParentProperties.Num() >= 2 &&
			InPropertyAndParent.ParentProperties[InPropertyAndParent.ParentProperties.Num() - 2]->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, DebugSettings)))
	{
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE