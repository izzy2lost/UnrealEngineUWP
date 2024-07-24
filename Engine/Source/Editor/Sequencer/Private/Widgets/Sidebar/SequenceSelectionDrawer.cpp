// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerSelectionDrawer.h"
#include "DetailsViewArgs.h"
#include "FrameNumberDetailsCustomization.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IKeyArea.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "IStructureDetailsView.h"
#include "Menus/CurveChannelSectionSidebarExtension.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneMarkedFrame.h"
#include "MovieSceneSequence.h"
#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/CategoryModel.h"
#include "MVVM/ViewModels/LayerBarModel.h"
#include "MVVM/ViewModels/PossessableModel.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "PropertyEditorModule.h"
#include "Sequencer.h"
#include "SequencerCommonHelpers.h"
#include "SKeyEditInterface.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "SequencerSelectionDrawer"

namespace UE::Sequencer::Private
{
	FKeyEditData GetKeyEditData(const UE::Sequencer::FKeySelection& InKeySelection)
	{
		if (InKeySelection.Num() == 1)
		{
			for (const FKeyHandle Key : InKeySelection)
			{
				if (const TSharedPtr<UE::Sequencer::FChannelModel> Channel = InKeySelection.GetModelForKey(Key))
				{
					FKeyEditData KeyEditData;
					KeyEditData.KeyStruct     = Channel->GetKeyArea()->GetKeyStruct(Key);
					KeyEditData.OwningSection = Channel->GetSection();
					return KeyEditData;
				}
			}
		}
		else
		{
			TArray<FKeyHandle> KeyHandles;
			UMovieSceneSection* CommonSection = nullptr;
			for (FKeyHandle Key : InKeySelection)
			{
				TSharedPtr<UE::Sequencer::FChannelModel> Channel = InKeySelection.GetModelForKey(Key);
				if (Channel.IsValid())
				{
					KeyHandles.Add(Key);
					if (!CommonSection)
					{
						CommonSection = Channel->GetSection();
					}
					else if (CommonSection != Channel->GetSection())
					{
						CommonSection = nullptr;
						break;
					}
				}
			}

			if (CommonSection)
			{
				FKeyEditData KeyEditData;
				KeyEditData.KeyStruct     = CommonSection->GetKeyStruct(KeyHandles);
				KeyEditData.OwningSection = CommonSection;
				return KeyEditData;
			}
		}

		return FKeyEditData();
	}

	TSharedPtr<UE::Sequencer::FSequencerSelection> GetSelection(const ISequencer& InSequencer)
	{
		const TSharedPtr<UE::Sequencer::FSequencerEditorViewModel> ViewModel = InSequencer.GetViewModel();
		if (!ViewModel.IsValid())
		{
			return nullptr;
		}

		return ViewModel->GetSelection();
	}
}

using namespace UE::Sequencer;

const FName FSequencerSelectionDrawer::UniqueId = TEXT("SequencerSelectionDrawer");

FSequencerSelectionDrawer::FSequencerSelectionDrawer(const TWeakPtr<FSequencer>& InSequencerWeak)
	: SequencerWeak(InSequencerWeak)
{
}

FName FSequencerSelectionDrawer::GetUniqueId() const
{
	return UniqueId;
}

FName FSequencerSelectionDrawer::GetSectionId() const
{
	return TEXT("Selection");
}

FText FSequencerSelectionDrawer::GetSectionDisplayText() const
{
	return LOCTEXT("SelectionDisplayText", "Selection");
}

TSharedRef<SWidget> FSequencerSelectionDrawer::CreateContentWidget()
{
	if (const TSharedPtr<ISequencer> Sequencer = SequencerWeak.Pin())
	{
		Sequencer->OnActorAddedToSequencer().AddLambda([this](AActor* InActor, const FGuid InGuid)
			{
				OnSequencerSelectionChanged();
			});

		if (const TSharedPtr<UE::Sequencer::FSequencerSelection> SequencerSelection = UE::Sequencer::Private::GetSelection(*Sequencer.Get()))
		{
			SequencerSelection->OnChanged.AddSP(this, &FSequencerSelectionDrawer::OnSequencerSelectionChanged);
			
			OnSequencerSelectionChanged();
		}
	}

	return SNew(SBorder)
		.HAlign(HAlign_Fill)
		.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
		[
			SAssignNew(ContentBox, SVerticalBox)
			+ SVerticalBox::Slot()
			[
				CreateNoSelectionHintText()
			]
		];
}

void FSequencerSelectionDrawer::OnSequencerSelectionChanged()
{
	if (!ContentBox.IsValid())
	{
		return;
	}

	ContentBox->ClearChildren();

	CurveChannelExtension.Reset();

	const TSharedPtr<FSequencer> Sequencer = SequencerWeak.Pin();
	if (!SequencerWeak.IsValid())
	{
		return;
	}

	const TSharedPtr<FSequencerSelection> SequencerSelection = Private::GetSelection(*Sequencer.Get());
	if (!SequencerSelection.IsValid())
	{
		return;
	}

	const TSharedRef<FSequencerSelection> SelectionRef = SequencerSelection.ToSharedRef();

	auto AddToContent = [this](const TSharedRef<SWidget>& InWidget)
		{
			ContentBox->AddSlot()
				.AutoHeight()
				[
					InWidget
				];
		};

	const ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
	const TSharedPtr<FExtensibilityManager> SidebarExtensibilityManager = SequencerModule.GetSidebarExtensibilityManager();
	const TSharedPtr<FExtender> Extender = SidebarExtensibilityManager->GetAllExtenders();

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/false
		, Sequencer->GetCommandBindings(), Extender
		, /*bInCloseSelfOnly=*/true, &FCoreStyle::Get(), /*bInSearchable=*/true, TEXT("Sequencer.Sidebar"));

	/**
	 * Selection details display order preference:
	 *  1) Key items
	 *  2) Binding properties
	 *  2) Track area items (if no key selected)
	 *  3) Outliner items (if no key or track area selected)
	 *  5) Marked frames
	 */

	// 1) Key items
	BuildKeySelectionDetails(SelectionRef, MenuBuilder);

	// 2) Binding properties
	BuildBindingPropertiesDetails(*Sequencer, SelectionRef, MenuBuilder);

	// Early out for key selections
	const bool bIsKeySelected = SequencerSelection->KeySelection.Num() > 0;
	if (bIsKeySelected)
	{
		AddToContent(MenuBuilder.MakeWidget());
		return;
	}

	// 3) Track area items
	BuildTrackAreaDetails(*Sequencer, SelectionRef, MenuBuilder);

	// 4) Outliner items
	const bool bIsTrackAreaSelected = SequencerSelection->TrackArea.Num() > 0;
	if (!bIsTrackAreaSelected)
	{
		BuildOutlinerDetails(*Sequencer, SelectionRef, MenuBuilder);
	}

	// 5) Marked frames
	BuildMarkedFrameDetails(SelectionRef, MenuBuilder);

	AddToContent(MenuBuilder.MakeWidget());
}

void FSequencerSelectionDrawer::BuildKeySelectionDetails(const TSharedRef<UE::Sequencer::FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder)
{
	if (InSelection->KeySelection.Num() == 0)
	{
		return;
	}

	MenuBuilder.BeginSection(TEXT("KeyEdit"), LOCTEXT("KeyEditMenuSection", "Key Edit"));

	MenuBuilder.AddWidget(CreateKeyFrameDetails(InSelection).ToSharedRef(), FText::GetEmpty(), /*bInNoIndent=*/true);

	MenuBuilder.EndSection();

	// Show the section for the keys if they are all part of the same section
	TArray<TViewModelPtr<FChannelModel>> Channels;
	for (const FKeyHandle KeyHandle : InSelection->KeySelection)
	{
		const TViewModelPtr<FChannelModel> Channel = InSelection->KeySelection.GetModelForKey(KeyHandle);
		Channels.Add(Channel);
	}
	if (Channels.Num() == 1)
	{
		// NOTE: Can't wrap with a section because the BuildSidebarMenu functions below add sections.
			
		if (const TSharedPtr<IKeyArea> KeyArea = Channels[0]->GetKeyArea())
		{
			if (const TViewModelPtr<IOutlinerExtension> LinkedOutlinerItem = Channels[0]->GetLinkedOutlinerItem())
			{
				if (const TViewModelPtr<FOutlinerItemModel> OutlinerItemModel = LinkedOutlinerItem.ImplicitCast())
				{
					OutlinerItemModel->BuildTrackOptionsMenu(MenuBuilder);
					OutlinerItemModel->BuildDisplayOptionsMenu(MenuBuilder);
				}
			}
		}
			
		if (const TViewModelPtr<FTrackModel>& TrackModel = Channels[0].AsModel()->FindAncestorOfType<FTrackModel>())
		{
			TrackModel->BuildSidebarMenu(MenuBuilder);
		}
	}
}

void FSequencerSelectionDrawer::BuildBindingPropertiesDetails(FSequencer& InSequencer, const TSharedRef<UE::Sequencer::FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder)
{
	TArray<FGuid> ObjectBindings;
	InSequencer.GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	UMovieSceneSequence* const FocusedMovieSceneSequence = InSequencer.GetFocusedMovieSceneSequence();
	if (!IsValid(FocusedMovieSceneSequence) || !FocusedMovieSceneSequence->AllowsSpawnableObjects())
	{
		return;
	}

	if (InSelection->Outliner.Num() == 1)
	{
		for (const FViewModelPtr Possessable : InSelection->Outliner.Filter<FPossessableModel>())
		{
			MenuBuilder.BeginSection(TEXT("Possessable"));
			MenuBuilder.EndSection();

			// Adding this causes two sets of details for the property bindings to show since
			// ULevelSequenceEditorSubsystem::Initialize adds extension hooks "Possessable" and "CustomBinding"
			// that call the exact same function. Will add this if there is a reason for this or remove this
			// if not needed.
			//MenuBuilder.BeginSection(TEXT("CustomBinding"));
			//MenuBuilder.EndSection();
		}
	}
}

void FSequencerSelectionDrawer::BuildTrackAreaDetails(FSequencer& InSequencer, const TSharedRef<UE::Sequencer::FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder)
{
	if (InSelection->TrackArea.Num() == 0)
	{
		return;
	}

	TArray<TWeakObjectPtr<>> AllSectionObjects;
	TArray<TViewModelPtr<FSectionModel>> AllSectionModels;
	
	for (const FViewModelPtr TrackAreaItem : InSelection->TrackArea)
	{
		if (const TViewModelPtr<FLayerBarModel> LayerBarModel = TrackAreaItem.ImplicitCast())
		{
			const TViewModelPtr<IOutlinerExtension> LinkedOutlinerItem = LayerBarModel->GetLinkedOutlinerItem();

			if (InSelection->TrackArea.Num() == 1)
			{
				if (const TViewModelPtr<FOutlinerItemModel> OutlinerItemModel = LinkedOutlinerItem.ImplicitCast())
				{
					OutlinerItemModel->BuildTrackOptionsMenu(MenuBuilder);
					OutlinerItemModel->BuildDisplayOptionsMenu(MenuBuilder);
				}
			}
		}
		else if (const TViewModelPtr<FSectionModel> SectionModel = TrackAreaItem.ImplicitCast())
		{
			// ISequencerSection Details (Shot Takes, Etc.)
			if (const TSharedPtr<ISequencerSection> SectionInterface = SectionModel->GetSectionInterface())
			{
				AllSectionModels.Add(SectionModel);
			}
			
			// Gather track section to use to build a single details for all selected sections
			AllSectionObjects.Add(SectionModel->GetSection());
		}
	}

	if (AllSectionModels.Num() == 1)
	{
		if (const TSharedPtr<ISequencerSection> SectionInterface = AllSectionModels[0]->GetSectionInterface())
		{
			const TViewModelPtr<IObjectBindingExtension> ObjectBindingModel = AllSectionModels[0]->FindAncestorOfType<IObjectBindingExtension>();
			const FGuid ObjectGuid = ObjectBindingModel ? ObjectBindingModel->GetObjectGuid() : FGuid();
			SectionInterface->BuildSectionSidebarMenu(MenuBuilder, ObjectGuid);
		}
	}
	
	SequencerHelpers::AddPropertiesMenu(InSequencer, MenuBuilder, AllSectionObjects);
}

void FSequencerSelectionDrawer::BuildOutlinerDetails(FSequencer& InSequencer, const TSharedRef<UE::Sequencer::FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder)
{
	if (InSelection->Outliner.Num() == 0)
	{
		return;
	}

	TSet<TViewModelPtr<FChannelGroupOutlinerModel>> ChannelGroups;

	for (const FViewModelPtr OutlinerItem : InSelection->Outliner)
	{
		if (const TViewModelPtr<FTrackModel> TrackModel = OutlinerItem.ImplicitCast())
		{
			if (const TSharedPtr<ISequencerTrackEditor> TrackEditor = TrackModel->GetTrackEditor())
			{
				TrackEditor->BuildTrackSidebarMenu(MenuBuilder, TrackModel->GetTrack());
			}
		}

		if (InSelection->Outliner.Num() == 1)
		{
			if (const TViewModelPtr<FOutlinerItemModel> OutlinerItemModel = OutlinerItem.ImplicitCast())
			{
				OutlinerItemModel->BuildTrackOptionsMenu(MenuBuilder);
				OutlinerItemModel->BuildDisplayOptionsMenu(MenuBuilder);
			}
		}

		// Ex. "Location.X"
		if (const TViewModelPtr<FChannelGroupOutlinerModel> ChannelGroupOutlinerModel = OutlinerItem.ImplicitCast())
		{
			ChannelGroups.Add(ChannelGroupOutlinerModel);
		}
	}

	if (!ChannelGroups.IsEmpty())
	{
		const ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
		const TSharedPtr<FExtensibilityManager> SidebarExtensibilityManager = SequencerModule.GetSidebarExtensibilityManager();
		const TSharedPtr<FExtender> Extender = SidebarExtensibilityManager->GetAllExtenders();

		TArray<ISequencerChannelInterface*> ChannelInterfaces;
		TArray<FMovieSceneChannelHandle> ChannelHandles;
		TArray<UMovieSceneSection*> SceneSections;

		for (const TViewModelPtr<FChannelGroupOutlinerModel>& ChannelModel : ChannelGroups)
		{
			for (const TSharedRef<IKeyArea>& KeyArea : ChannelModel->GetAllKeyAreas())
			{
				if (ISequencerChannelInterface* const SequencerChannelIterface = KeyArea->FindChannelEditorInterface())
				{
					ChannelInterfaces.Add(SequencerChannelIterface);
					ChannelHandles.Add(KeyArea->GetChannel());
					SceneSections.Add(KeyArea->GetOwningSection());
				}
			}
		}

		// Channel Interface Extensions (Perlin Noise, Easing, Wave)
		for (const ISequencerChannelInterface* const ChannelInterface : ChannelInterfaces)
		{
			ChannelInterface->ExtendSidebarMenu_Raw(MenuBuilder, Extender, ChannelHandles, SceneSections, SequencerWeak);
		}

		// Curve Channel Options (Pre-Finity, Post-Finity, etc.)
		CurveChannelExtension = MakeShared<FCurveChannelSectionSidebarExtension>(SequencerWeak);
		CurveChannelExtension->AddSections(SceneSections);
		CurveChannelExtension->ExtendMenu(MenuBuilder);
	}
}

void FSequencerSelectionDrawer::BuildMarkedFrameDetails(const TSharedRef<UE::Sequencer::FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder)
{
	if (InSelection->MarkedFrames.Num() == 0)
	{
		return;
	}

	MenuBuilder.BeginSection(TEXT("MarkedFrames"), LOCTEXT("MarkedFramesMenuSection", "Marked Frames"));
	
	for (const int32 MarkIndex : InSelection->MarkedFrames)
	{
		MenuBuilder.AddWidget(CreateMarkedFrameDetails(MarkIndex).ToSharedRef(), FText::GetEmpty(), /*bInNoIndent=*/true);
	}

	MenuBuilder.EndSection();
}

TSharedRef<SWidget> FSequencerSelectionDrawer::CreateHintText(const FText& InMessage)
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.Padding(2.f, 12.f, 2.f, 12.f)
		[
			SNew(STextBlock)
			.Text(InMessage)
			.TextStyle(FAppStyle::Get(), "HintText")
		];
}

TSharedRef<SWidget> FSequencerSelectionDrawer::CreateNoSelectionHintText()
{
	return CreateHintText(LOCTEXT("NoSelection", "Select an object to view details."));
}

FKeyEditData FSequencerSelectionDrawer::GetKeyEditData() const
{
	const TSharedPtr<ISequencer> Sequencer = SequencerWeak.Pin();
	if (!Sequencer.IsValid())
	{
		return FKeyEditData();
	}

	const TSharedPtr<UE::Sequencer::FSequencerSelection> SequencerSelection = UE::Sequencer::Private::GetSelection(*Sequencer.Get());
	if (!SequencerSelection.IsValid())
	{
		return FKeyEditData();
	}

	return UE::Sequencer::Private::GetKeyEditData(SequencerSelection->KeySelection);
}

TSharedPtr<SWidget> FSequencerSelectionDrawer::CreateKeyFrameDetails(const TSharedRef<UE::Sequencer::FSequencerSelection>& InSequencerSelection)
{
	const TSharedPtr<ISequencer> Sequencer = SequencerWeak.Pin();
	if (!Sequencer.IsValid())
	{
		return nullptr;
	}

	const FKeyEditData KeyEditData = UE::Sequencer::Private::GetKeyEditData(InSequencerSelection->KeySelection);
	if (KeyEditData.KeyStruct.IsValid())
	{
		return SNew(SKeyEditInterface, Sequencer.ToSharedRef())
			.EditData(this, &FSequencerSelectionDrawer::GetKeyEditData);
	}

	return CreateHintText(LOCTEXT("InvalidKeyCombination", "Selected keys must belong to the same section."));
}

TSharedPtr<SWidget> FSequencerSelectionDrawer::CreateMarkedFrameDetails(const int32 InMarkedFrameIndex)
{
	const TSharedPtr<FSequencer> Sequencer = SequencerWeak.Pin();
	if (!Sequencer.IsValid())
	{
		return nullptr;
	}

	const UMovieSceneSequence* const FocusedMovieSceneSequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!IsValid(FocusedMovieSceneSequence))
	{
		return nullptr;
	}

	UMovieScene* const FocusedMovieScene = FocusedMovieSceneSequence->GetMovieScene();
	if (!IsValid(FocusedMovieScene))
	{
		return nullptr;
	}

	if (FocusedMovieScene->GetMarkedFrames().Num() == 0)
	{
		return nullptr;
	}

	class SMarkedFramePropertyWidget : public SCompoundWidget, public FNotifyHook
	{
	public:
		SLATE_BEGIN_ARGS(SMarkedFramePropertyWidget) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UMovieScene* const InMovieScene, const int32 InMarkedFrameIndex, const TWeakPtr<FSequencer>& InWeakSequencer)
		{
			MovieSceneToModify = InMovieScene;
			WeakSequencer = InWeakSequencer;

			FDetailsViewArgs DetailsViewArgs;
			DetailsViewArgs.bAllowSearch = false;
			DetailsViewArgs.bShowScrollBar = false;
			DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
			DetailsViewArgs.NotifyHook = this;

			FStructureDetailsViewArgs StructureDetailsViewArgs;
			StructureDetailsViewArgs.bShowObjects = true;
			StructureDetailsViewArgs.bShowAssets = true;
			StructureDetailsViewArgs.bShowClasses = true;
			StructureDetailsViewArgs.bShowInterfaces = true;
			
			const TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FMovieSceneMarkedFrame::StaticStruct(), (uint8*)&InMovieScene->GetMarkedFrames()[InMarkedFrameIndex]);

			FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

			DetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureDetailsViewArgs, nullptr);
			DetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout(TEXT("FrameNumber"), FOnGetPropertyTypeCustomizationInstance::CreateLambda([this]() {
				return MakeShared<FFrameNumberDetailsCustomization>(WeakSequencer.Pin()->GetNumericTypeInterface()); }));
			DetailsView->SetStructureData(StructOnScope);
			
			ChildSlot
			[
				DetailsView->GetWidget().ToSharedRef()
			];
		}

		virtual void NotifyPreChange(FProperty* InPropertyAboutToChange) override
		{
			MovieSceneToModify->Modify();
		}

		virtual void NotifyPreChange(FEditPropertyChain* InPropertyAboutToChange) override
		{
			MovieSceneToModify->Modify();
		}

	private:
		TObjectPtr<UMovieScene> MovieSceneToModify;
		TSharedPtr<IStructureDetailsView> DetailsView;
		TWeakPtr<FSequencer> WeakSequencer;
	};

	auto AreMarkedFramesLocked = [&Sequencer]() -> bool
	{
		if (Sequencer->IsReadOnly())
		{
			return true;
		}

		const UMovieSceneSequence* const FocusedMovieSceneSequence = Sequencer->GetFocusedMovieSceneSequence();
		if (FocusedMovieSceneSequence != nullptr)
		{
			const UMovieScene* const MovieScene = FocusedMovieSceneSequence->GetMovieScene();
			if (MovieScene->IsReadOnly())
			{
				return true;
			}
			return MovieScene->AreMarkedFramesLocked();
		}

		return false;
	};

	const TSharedRef<SMarkedFramePropertyWidget> Widget = SNew(SMarkedFramePropertyWidget, FocusedMovieScene, InMarkedFrameIndex, SequencerWeak);
	Widget->SetEnabled(!AreMarkedFramesLocked());

	return Widget;
}

#undef LOCTEXT_NAMESPACE
