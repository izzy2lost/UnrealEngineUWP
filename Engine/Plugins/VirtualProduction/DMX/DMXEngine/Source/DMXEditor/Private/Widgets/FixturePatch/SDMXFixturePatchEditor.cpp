// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/FixturePatch/SDMXFixturePatchEditor.h"

#include "Customizations/DMXEntityFixturePatchDetails.h"
#include "DMXEditor.h"
#include "DMXEditorSettings.h"
#include "DMXFixturePatchSharedData.h"
#include "DMXSubsystem.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXOutputPortReference.h"
#include "IO/DMXPortManager.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SDMXFixturePatcher.h"
#include "Widgets/FixturePatch/SDMXFixturePatchList.h"
#include "Widgets/Layout/SSplitter.h"

#define LOCTEXT_NAMESPACE "SDMXFixturePatcher"

SDMXFixturePatchEditor::~SDMXFixturePatchEditor()
{
	const float LeftSideWidth = LhsRhsSplitter->SlotAt(0).GetSizeValue();

	UDMXEditorSettings* const DMXEditorSettings = GetMutableDefault<UDMXEditorSettings>();
	DMXEditorSettings->MVRFixtureListSettings.ListWidth = LeftSideWidth;
	DMXEditorSettings->SaveConfig();
}

void SDMXFixturePatchEditor::Construct(const FArguments& InArgs)
{
	SDMXEntityEditor::Construct(SDMXEntityEditor::FArguments());

	DMXEditorPtr = InArgs._DMXEditor;
	if (!DMXEditorPtr.IsValid())
	{
		return;
	}
	FixturePatchSharedData = DMXEditorPtr.Pin()->GetFixturePatchSharedData();

	SetCanTick(false);

	FixturePatchDetailsView = GenerateFixturePatchDetailsView();

	const UDMXEditorSettings* const DMXEditorSettings = GetDefault<UDMXEditorSettings>();
	const float LeftSideWidth = FMath::Clamp(DMXEditorSettings->MVRFixtureListSettings.ListWidth, 0.1f, 0.9f);
	const float RightSideWidth = FMath::Max(1.f - DMXEditorSettings->MVRFixtureListSettings.ListWidth, .1f);

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SAssignNew(LhsRhsSplitter, SSplitter)
		.Orientation(EOrientation::Orient_Horizontal)
		.ResizeMode(ESplitterResizeMode::FixedPosition)
		
		// Left, MVR Fixture List
		+ SSplitter::Slot()	
		.Value(LeftSideWidth)
		[
			SAssignNew(FixturePatchList, SDMXFixturePatchList, DMXEditorPtr)
		]

		// Right, Fixture Patcher and Details
		+ SSplitter::Slot()	
		.Value(RightSideWidth)
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Vertical)
			.ResizeMode(ESplitterResizeMode::FixedPosition)

			+SSplitter::Slot()			
			.Value(.618f)
			[
				SAssignNew(FixturePatcher, SDMXFixturePatcher)
				.DMXEditor(DMXEditorPtr)
			]
	
			+SSplitter::Slot()
			.Value(.382f)
			[
				FixturePatchDetailsView.ToSharedRef()
			]
		]
	];

	// Adopt the selection
	OnFixturePatchesSelected();

	// Bind to selection changes
	FixturePatchSharedData->OnFixturePatchSelectionChanged.AddSP(this, &SDMXFixturePatchEditor::OnFixturePatchesSelected);
}

FReply SDMXFixturePatchEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return FixturePatchList->ProcessCommandBindings(InKeyEvent);
}

void SDMXFixturePatchEditor::RequestRenameOnNewEntity(const UDMXEntity* InEntity, ESelectInfo::Type SelectionType)
{
	if (FixturePatchList.IsValid())
	{
		FixturePatchList->EnterFixturePatchNameEditingMode();
	}
}

void SDMXFixturePatchEditor::SelectEntity(UDMXEntity* InEntity, ESelectInfo::Type InSelectionType /*= ESelectInfo::Type::Direct*/)
{
	if (UDMXEntityFixturePatch* FixturePatch = Cast<UDMXEntityFixturePatch>(InEntity))
	{
		FixturePatchSharedData->SelectFixturePatch(FixturePatch);
	}
}

void SDMXFixturePatchEditor::SelectEntities(const TArray<UDMXEntity*>& InEntities, ESelectInfo::Type InSelectionType /*= ESelectInfo::Type::Direct*/)
{
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> FixturePatches;
	for (UDMXEntity* Entity : InEntities)
	{
		if (UDMXEntityFixturePatch* FixturePatch = Cast<UDMXEntityFixturePatch>(Entity))
		{
			FixturePatches.Add(FixturePatch);
		}
	}
	FixturePatchSharedData->SelectFixturePatches(FixturePatches);
}

TArray<UDMXEntity*> SDMXFixturePatchEditor::GetSelectedEntities() const
{
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = FixturePatchSharedData->GetSelectedFixturePatches();
	TArray<UDMXEntity*> SelectedEntities;
	for (TWeakObjectPtr<UDMXEntityFixturePatch> WeakSelectedFixturePatch : SelectedFixturePatches)
	{
		if (UDMXEntity* Entity = WeakSelectedFixturePatch.Get())
		{
			SelectedEntities.Add(Entity);
		}
	}

	return SelectedEntities;
}

void SDMXFixturePatchEditor::SelectUniverse(int32 UniverseID)
{
	if (!ensureMsgf(UniverseID >= 0 && UniverseID <= DMX_MAX_UNIVERSE, TEXT("Invalid Universe when trying to select Universe %i."), UniverseID))
	{
		return;
	}

	FixturePatchSharedData->SelectUniverse(UniverseID);
}

void SDMXFixturePatchEditor::OnFixturePatchesSelected()
{
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = FixturePatchSharedData->GetSelectedFixturePatches();
	TArray<UObject*> SelectedObjects;
	for (TWeakObjectPtr<UDMXEntityFixturePatch> WeakSelectedFixturePatch : SelectedFixturePatches)
	{
		if (UDMXEntity* SelectedObject = WeakSelectedFixturePatch.Get())
		{
			SelectedObjects.Add(SelectedObject);
		}
	}
	FixturePatchDetailsView->SetObjects(SelectedObjects);
}

TSharedRef<IDetailsView> SDMXFixturePatchEditor::GenerateFixturePatchDetailsView() const
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;

	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->RegisterInstancedCustomPropertyLayout(UDMXEntityFixturePatch::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FDMXEntityFixturePatchDetails::MakeInstance, DMXEditorPtr));

	return DetailsView;
}

#undef LOCTEXT_NAMESPACE
