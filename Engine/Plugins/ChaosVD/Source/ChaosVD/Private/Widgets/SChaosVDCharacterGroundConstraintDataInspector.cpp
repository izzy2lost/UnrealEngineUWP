// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDCharacterGroundConstraintDataInspector.h"

#include "ChaosVDScene.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Widgets/SChaosVDWarningMessageBox.h"
#include "Components/ChaosVDSolverCharacterGroundConstraintDataComponent.h"
#include "DataWrappers/ChaosVDCharacterGroundConstraintDataWrappers.h"
#include "Modules/ModuleManager.h"
#include "Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

SChaosVDCharacterGroundConstraintDataInspector::~SChaosVDCharacterGroundConstraintDataInspector()
{
	UnregisterSceneEvents();
}

void SChaosVDCharacterGroundConstraintDataInspector::Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDScene>& InScenePtr)
{
	SceneWeakPtr = InScenePtr;

	RegisterSceneEvents();

	ConstraintDataDetailsView = CreateDataDetailsView();

	constexpr float NoPadding = 0.0f;
	constexpr float OuterBoxPadding = 2.0f;
	constexpr float OuterInnerPadding = 5.0f;
	constexpr float TagTitleBoxHorizontalPadding = 10.0f;
	constexpr float TagTitleBoxVerticalPadding = 5.0f;
	constexpr float InnerDetailsPanelsHorizontalPadding = 15.0f;
	constexpr float InnerDetailsPanelsVerticalPadding = 15.0f;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(OuterInnerPadding)
		[
			SNew(SBox)
			.Visibility_Raw(this, &SChaosVDCharacterGroundConstraintDataInspector::GetOutOfDateWarningVisibility)
			.Padding(OuterBoxPadding, OuterBoxPadding,OuterBoxPadding,NoPadding)
			[
				SNew(SChaosVDWarningMessageBox)
				.WarningText(LOCTEXT("CharacterGroundConstraintDataOutOfDate", "Scene change detected!. Selected constraint data is out of date..."))
			]
		]
		+SVerticalBox::Slot()
		.Padding(TagTitleBoxHorizontalPadding, TagTitleBoxVerticalPadding, TagTitleBoxHorizontalPadding, NoPadding)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Visibility_Raw(this, &SChaosVDCharacterGroundConstraintDataInspector::GetNothingSelectedMessageVisibility)
			.Justification(ETextJustify::Center)
			.TextStyle(FAppStyle::Get(), "DetailsView.BPMessageTextStyle")
			.Text(LOCTEXT("CharacterGroundConstraintDataNoSelectedMessage", "Select a Character Ground Constraint in the viewport to see its details..."))
			.AutoWrapText(true)
		]
		+SVerticalBox::Slot()
		.Padding(OuterInnerPadding)
		[
			SNew(SScrollBox)
			.Visibility_Raw(this, &SChaosVDCharacterGroundConstraintDataInspector::GetDetailsSectionVisibility)
			+SScrollBox::Slot()
			.Padding(InnerDetailsPanelsHorizontalPadding,NoPadding,InnerDetailsPanelsHorizontalPadding,InnerDetailsPanelsVerticalPadding)
			[
				ConstraintDataDetailsView->GetWidget().ToSharedRef()
			]
		]
	];
}

void SChaosVDCharacterGroundConstraintDataInspector::RegisterSelectionEventsForSolver(AChaosVDSolverInfoActor* SolverInfo)
{
	if (UChaosVDSolverCharacterGroundConstraintDataComponent* ConstraintDataComponent = SolverInfo ? SolverInfo->GetCharacterGroundConstraintDataComponent() : nullptr)
	{
		ConstraintDataComponent->OnSelectionChanged().AddRaw(this, &SChaosVDCharacterGroundConstraintDataInspector::SetConstraintDataToInspect);
	}
}

void SChaosVDCharacterGroundConstraintDataInspector::UnregisterSelectionEventsForSolver(AChaosVDSolverInfoActor* SolverInfo)
{
	if (UChaosVDSolverCharacterGroundConstraintDataComponent* ConstraintDataComponent = SolverInfo ? SolverInfo->GetCharacterGroundConstraintDataComponent() : nullptr)
	{
		ConstraintDataComponent->OnSelectionChanged().RemoveAll(this);
	}
}

void SChaosVDCharacterGroundConstraintDataInspector::RegisterSceneEvents()
{
	if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		ScenePtr->OnSceneUpdated().AddRaw(this, &SChaosVDCharacterGroundConstraintDataInspector::HandleSceneUpdated);
		ScenePtr->OnSolverInfoActorCreated().AddRaw(this, &SChaosVDCharacterGroundConstraintDataInspector::RegisterSelectionEventsForSolver);

		for (const TPair<int32, AChaosVDSolverInfoActor*> SolverInfos : ScenePtr->GetSolverInfoActorsMap())
		{
			RegisterSelectionEventsForSolver(SolverInfos.Value);
		}
	}
}

void SChaosVDCharacterGroundConstraintDataInspector::UnregisterSceneEvents()
{
	if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		ScenePtr->OnSceneUpdated().RemoveAll(this);
		ScenePtr->OnSolverInfoActorCreated().RemoveAll(this);

		for (const TPair<int32, AChaosVDSolverInfoActor*> SolverInfos : ScenePtr->GetSolverInfoActorsMap())
		{
			UnregisterSelectionEventsForSolver(SolverInfos.Value);
		}
	}
}

void SChaosVDCharacterGroundConstraintDataInspector::SetConstraintDataToInspect(const FChaosVDCharacterGroundConstraintSelectionHandle& InDataSelectionHandle)
{
	ClearInspector();

	if (const TSharedPtr<FChaosVDCharacterGroundConstraint> QueryDataToInspect = InDataSelectionHandle.GetData().Pin())
	{
		CurrentDataSelectionHandle = InDataSelectionHandle;
		const TSharedPtr<FStructOnScope> QueryDataView = MakeShared<FStructOnScope>(FChaosVDCharacterGroundConstraint::StaticStruct(), reinterpret_cast<uint8*>(QueryDataToInspect.Get()));
		ConstraintDataDetailsView->SetStructureData(QueryDataView);
	}

	bIsUpToDate = true;
}

void SChaosVDCharacterGroundConstraintDataInspector::SetConstraintDataProviderObjectToInspect(IChaosVDCharacterGroundConstraintDataProviderInterface* ConstraintDataProvider)
{
	if (ConstraintDataProvider == nullptr)
	{
		return;
	}

	// Currently only support viewing first found data
	if (ConstraintDataProvider->HasCharacterGroundConstraintData())
	{
		TArray<TSharedPtr<FChaosVDCharacterGroundConstraint>> FoundConstraintData;
		ConstraintDataProvider->GetCharacterGroundConstraintData(FoundConstraintData);

		FChaosVDCharacterGroundConstraintSelectionHandle NewDataSelectionHandle = FChaosVDCharacterGroundConstraintSelectionHandle(FoundConstraintData[0]);
		if (NewDataSelectionHandle.GetData() != CurrentDataSelectionHandle.GetData())
		{
			ClearInspector();

			const TSharedPtr<FStructOnScope> QueryDataView = MakeShared<FStructOnScope>(FChaosVDCharacterGroundConstraint::StaticStruct(), reinterpret_cast<uint8*>(FoundConstraintData[0].Get()));
			ConstraintDataDetailsView->SetStructureData(QueryDataView);
			CurrentDataSelectionHandle = NewDataSelectionHandle;
		}
	}
	else
	{
		ClearInspector();
	}

	bIsUpToDate = true;
}

EVisibility SChaosVDCharacterGroundConstraintDataInspector::GetOutOfDateWarningVisibility() const
{
	return !bIsUpToDate && CurrentDataSelectionHandle.GetData().IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SChaosVDCharacterGroundConstraintDataInspector::GetDetailsSectionVisibility() const
{
	return CurrentDataSelectionHandle.GetData().IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SChaosVDCharacterGroundConstraintDataInspector::GetNothingSelectedMessageVisibility() const
{
	return !CurrentDataSelectionHandle.GetData().IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SChaosVDCharacterGroundConstraintDataInspector::HandleSceneUpdated()
{
	// TODO: Optimize this

	if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		if (USelection* Selection = ScenePtr->GetActorSelectionObject())
		{
			if (Selection->Num() > 0)
			{
				if (IChaosVDCharacterGroundConstraintDataProviderInterface* DataProvider = Cast<IChaosVDCharacterGroundConstraintDataProviderInterface>(Selection->GetSelectedObject(0)))
				{
					SetConstraintDataProviderObjectToInspect(DataProvider);
					return;
				}
			}
		}
	}

	ClearInspector();
}

void SChaosVDCharacterGroundConstraintDataInspector::ClearInspector()
{
	ConstraintDataDetailsView->SetStructureData(nullptr);
	CurrentDataSelectionHandle = FChaosVDCharacterGroundConstraintSelectionHandle();
}

TSharedPtr<IStructureDetailsView> SChaosVDCharacterGroundConstraintDataInspector::CreateDataDetailsView()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	const FStructureDetailsViewArgs StructDetailsViewArgs;
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowFavoriteSystem = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bShowScrollBar = false;

	return PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructDetailsViewArgs, nullptr);
}

#undef LOCTEXT_NAMESPACE
