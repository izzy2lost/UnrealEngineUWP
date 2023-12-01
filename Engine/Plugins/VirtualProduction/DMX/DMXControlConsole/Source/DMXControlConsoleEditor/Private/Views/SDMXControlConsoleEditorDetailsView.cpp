// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorDetailsView.h"

#include "Algo/Transform.h"
#include "Application/ThrottleManager.h"
#include "Controllers/DMXControlConsoleElementController.h"
#include "Customizations/DMXControlConsoleElementControllerDetails.h"
#include "Customizations/DMXControlConsoleFaderDetails.h"
#include "Customizations/DMXControlConsoleFaderGroupDetails.h"
#include "Delegates/IDelegateInstance.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/Filter/FilterModel.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "TimerManager.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorDetailsView"

namespace UE::DMX::Private
{
	SDMXControlConsoleEditorDetailsView::~SDMXControlConsoleEditorDetailsView()
	{
		FGlobalTabmanager::Get()->OnActiveTabChanged_Unsubscribe(OnActiveTabChangedDelegateHandle);
	}

	void SDMXControlConsoleEditorDetailsView::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		checkf(InEditorModel, TEXT("Invalid control console editor model, can't constuct details view correctly."));
		EditorModel = InEditorModel;

		OnActiveTabChangedDelegateHandle = FGlobalTabmanager::Get()->OnActiveTabChanged_Subscribe(FOnActiveTabChanged::FDelegate::CreateSP(this, &SDMXControlConsoleEditorDetailsView::OnActiveTabChanged));

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->GetOnSelectionChanged().AddSP(this, &SDMXControlConsoleEditorDetailsView::RequestUpdateDetailsViews);

		const TSharedRef<FFilterModel> FilterModel = EditorModel->GetFilterModel();
		FilterModel->OnFilterChanged.AddSP(this, &SDMXControlConsoleEditorDetailsView::RequestUpdateDetailsViews);

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;

		FPropertyEditorModule& PropertyEditor = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FaderGroupsDetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);
		ElementControllersDetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);
		FadersDetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);

		const FOnGetDetailCustomizationInstance FaderGroupsCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXControlConsoleFaderGroupDetails::MakeInstance, EditorModel);
		FaderGroupsDetailsView->RegisterInstancedCustomPropertyLayout(UDMXControlConsoleFaderGroup::StaticClass(), FaderGroupsCustomizationInstance);

		const FOnGetDetailCustomizationInstance ElementControllersCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXControlConsoleElementControllerDetails::MakeInstance, EditorModel);
		ElementControllersDetailsView->RegisterInstancedCustomPropertyLayout(UDMXControlConsoleElementController::StaticClass(), ElementControllersCustomizationInstance);

		const FOnGetDetailCustomizationInstance FadersCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXControlConsoleFaderDetails::MakeInstance, EditorModel);
		FadersDetailsView->RegisterInstancedCustomPropertyLayout(UDMXControlConsoleFaderBase::StaticClass(), FadersCustomizationInstance);

		ChildSlot
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)

				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						ElementControllersDetailsView.ToSharedRef()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SSeparator)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						FadersDetailsView.ToSharedRef()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SSeparator)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						FaderGroupsDetailsView.ToSharedRef()
					]
				]
			];

		ForceUpdateDetailsViews();
	}

	void SDMXControlConsoleEditorDetailsView::RequestUpdateDetailsViews()
	{
		if (!UpdateDetailsViewTimerHandle.IsValid())
		{
			UpdateDetailsViewTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXControlConsoleEditorDetailsView::ForceUpdateDetailsViews));
		}
	}

	void SDMXControlConsoleEditorDetailsView::ForceUpdateDetailsViews()
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, can't update details view correctly.")))
		{
			return;
		}

		UpdateDetailsViewTimerHandle.Invalidate();

		constexpr bool bForceRefresh = true;
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();

		// Fader Groups
		TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupObjects = SelectionHandler->GetSelectedFaderGroups();
		SelectedFaderGroupObjects.RemoveAll([](const TWeakObjectPtr<UObject>& SelectedFaderGroupObject)
			{
				const UDMXControlConsoleFaderGroup* SelectedFaderGroup = Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupObject);
				return SelectedFaderGroup && !SelectedFaderGroup->IsMatchingFilter();
			});
		FaderGroupsDetailsView->SetObjects(SelectedFaderGroupObjects, bForceRefresh);

		// Element Controllers
		TArray<TWeakObjectPtr<UObject>> SelectedElementControllerObjects = SelectionHandler->GetSelectedElementControllers();
		SelectedElementControllerObjects.RemoveAll([](const TWeakObjectPtr<UObject>& SelectedElementControllerObject)
			{
				const UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectedElementControllerObject);
				return SelectedElementController && !SelectedElementController->IsMatchingFilter();
			});
		ElementControllersDetailsView->SetObjects(SelectedElementControllerObjects, bForceRefresh);

		//Faders
		const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> SelectedElements = SelectionHandler->GetSelectedElements();
		TArray<TWeakObjectPtr<UObject>> SelectedElementObjects;
		Algo::TransformIf(SelectedElements, SelectedElementObjects,
			[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
			{
				return Element && IsValid(Cast<UDMXControlConsoleFaderBase>(Element.GetObject()));
			},
			[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
			{
				return Element.GetObject();
			});
		FadersDetailsView->SetObjects(SelectedElementObjects, bForceRefresh);
	}

	void SDMXControlConsoleEditorDetailsView::OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated)
	{
		if (!FadersDetailsView.IsValid())
		{
			return;
		}

		bool bDisableThrottle = false;
		if (IsWidgetInTab(PreviouslyActive, FadersDetailsView))
		{
			FSlateThrottleManager::Get().DisableThrottle(bDisableThrottle);
		}

		if (IsWidgetInTab(NewlyActivated, FadersDetailsView))
		{
			bDisableThrottle = true;
			FSlateThrottleManager::Get().DisableThrottle(bDisableThrottle);
		}
	}

	bool SDMXControlConsoleEditorDetailsView::IsWidgetInTab(TSharedPtr<SDockTab> InDockTab, TSharedPtr<SWidget> InWidget) const
	{
		if (InDockTab.IsValid())
		{
			// Tab content that should be a parent of this widget on some level
			TSharedPtr<SWidget> TabContent = InDockTab->GetContent();
			// Current parent being checked against
			TSharedPtr<SWidget> CurrentParent = InWidget;

			while (CurrentParent.IsValid())
			{
				if (CurrentParent == TabContent)
				{
					return true;
				}
				CurrentParent = CurrentParent->GetParentWidget();
			}

			// reached top widget (parent is invalid) and none was the tab
			return false;
		}

		return false;
	}
}

#undef LOCTEXT_NAMESPACE
