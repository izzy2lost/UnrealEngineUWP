// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerSelectionDrawer.h"
#include "DetailsViewArgs.h"
#include "FrameNumberDetailsCustomization.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IKeyArea.h"
#include "ISequencerSection.h"
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
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "PropertyEditorModule.h"
#include "Sequencer.h"
#include "SequencerCommonHelpers.h"
#include "SequencerContextMenus.h"
#include "SKeyEditInterface.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "SequencerSelectionDrawer"

namespace UE::Sequencer::Private
{
	FKeyEditData GetKeyEditData(const FKeySelection& InKeySelection)
	{
		if (InKeySelection.Num() == 1)
		{
			for (const FKeyHandle Key : InKeySelection)
			{
				if (const TSharedPtr<FChannelModel> Channel = InKeySelection.GetModelForKey(Key))
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
				TSharedPtr<FChannelModel> Channel = InKeySelection.GetModelForKey(Key);
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

	TSharedPtr<FSequencerSelection> GetSelection(const ISequencer& InSequencer)
	{
		const TSharedPtr<FSequencerEditorViewModel> ViewModel = InSequencer.GetViewModel();
		if (!ViewModel.IsValid())
		{
			return nullptr;
		}

		return ViewModel->GetSelection();
	}
}

using namespace UE::Sequencer;

const FName FSequencerSelectionDrawer::UniqueId = TEXT("SequencerSelectionDrawer");

FSequencerSelectionDrawer::FSequencerSelectionDrawer(const TWeakPtr<FSequencer>& InWeakSequencer)
	: WeakSequencer(InWeakSequencer)
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
	if (const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin())
	{
		Sequencer->OnActorAddedToSequencer().AddLambda([this](AActor* InActor, const FGuid InGuid)
			{
				OnSequencerSelectionChanged();
			});

		if (const TSharedPtr<FSequencerSelection> SequencerSelection = Private::GetSelection(*Sequencer.Get()))
		{
			SequencerSelection->OnChanged.AddSP(this, &FSequencerSelectionDrawer::OnSequencerSelectionChanged);
			
			OnSequencerSelectionChanged();
		}
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
		.Padding(0.f)
		[
			SAssignNew(ContentBox, SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
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

	const TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!WeakSequencer.IsValid())
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

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/false
		, Sequencer->GetCommandBindings()
		, SidebarExtensibilityManager->GetAllExtenders()
		, /*bInCloseSelfOnly=*/true, &FCoreStyle::Get(), /*bInSearchable=*/true, TEXT("Sequencer.Sidebar"));

	/**
	 * Selection details display order preference:
	 *  1) Key items
	 *  2) Track area items (if no key selected)
	 *  3) Outliner items (if no key or track area selected)
	 *  4) Marked frames
	 */

	// 1) Key items
	BuildKeySelectionDetails(SelectionRef, MenuBuilder);

	// Early out for key selections
	const bool bIsKeySelected = SequencerSelection->KeySelection.Num() > 0;
	if (bIsKeySelected)
	{
		AddToContent(MenuBuilder.MakeWidget());
		return;
	}

	// 2) Track area items
	BuildTrackAreaDetails(*Sequencer, SelectionRef, MenuBuilder);

	// 3) Outliner items
	const bool bIsTrackAreaSelected = SequencerSelection->TrackArea.Num() > 0;
	if (!bIsTrackAreaSelected)
	{
		BuildOutlinerDetails(*Sequencer, SelectionRef, MenuBuilder);
	}

	// 4) Marked frames
	BuildMarkedFrameDetails(SelectionRef, MenuBuilder);

	AddToContent(MenuBuilder.MakeWidget());
}

void FSequencerSelectionDrawer::BuildKeySelectionDetails(const TSharedRef<FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder)
{
	if (InSelection->KeySelection.Num() == 0)
	{
		return;
	}

	const TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	const ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
	const TSharedPtr<FExtensibilityManager> SidebarExtensibilityManager = SequencerModule.GetSidebarExtensibilityManager();

	MenuBuilder.BeginSection(TEXT("KeyEdit"), LOCTEXT("KeyEditMenuSection", "Key Edit"));
	{
		MenuBuilder.AddWidget(CreateKeyFrameDetails(InSelection).ToSharedRef(), FText::GetEmpty(), /*bInNoIndent=*/true);
	}
	MenuBuilder.EndSection();

	// Show the section for the keys if they are all part of the same section
	TArray<TViewModelPtr<FChannelModel>> Channels;
	for (const FKeyHandle KeyHandle : InSelection->KeySelection)
	{
		const TViewModelPtr<FChannelModel> Channel = InSelection->KeySelection.GetModelForKey(KeyHandle);
		Channels.AddUnique(Channel);
	}
	if (Channels.Num() == 1)
	{
		FSectionContextMenu::BuildKeyEditMenu(MenuBuilder, WeakSequencer, Sequencer->GetLastEvaluatedLocalTime());
	}
}

void FSequencerSelectionDrawer::BuildTrackAreaDetails(FSequencer& InSequencer, const TSharedRef<FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder)
{
	TArray<TWeakObjectPtr<>> AllSectionObjects;

	for (const FViewModelPtr TrackAreaItem : InSelection->TrackArea)
	{
		if (const TViewModelPtr<FLayerBarModel> LayerBarModel = TrackAreaItem.ImplicitCast())
		{
			const TViewModelPtr<IOutlinerExtension> LinkedOutlinerItem = LayerBarModel->GetLinkedOutlinerItem();

			if (const TViewModelPtr<FOutlinerItemModel> OutlinerItemModel = LinkedOutlinerItem.ImplicitCast())
			{
				OutlinerItemModel->BuildSidebarMenu(MenuBuilder);
			}
		}
		else if (const TViewModelPtr<FSectionModel> SectionModel = TrackAreaItem.ImplicitCast())
		{
			if (InSelection->TrackArea.Num() == 1)
			{
				const TViewModelPtr<IOutlinerExtension> LinkedOutlinerItem = SectionModel->GetLinkedOutlinerItem();

				if (const TSharedPtr<ISequencerSection> SectionInterface = SectionModel->GetSectionInterface())
				{
					const TSharedPtr<IObjectBindingExtension> ObjectBinding = SectionModel->FindAncestorOfType<IObjectBindingExtension>();
					SectionInterface->BuildSectionSidebarMenu(MenuBuilder, ObjectBinding.IsValid() ? ObjectBinding->GetObjectGuid() : FGuid());
				}
			}

			AllSectionObjects.Add(SectionModel->GetSection());
		}
	}

	if (!AllSectionObjects.IsEmpty())
	{
		SequencerHelpers::BuildEditSectionMenu(InSequencer, AllSectionObjects, MenuBuilder, false);
	}
}

void FSequencerSelectionDrawer::BuildOutlinerDetails(FSequencer& InSequencer, const TSharedRef<FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder)
{
	if (InSelection->Outliner.Num() == 0)
	{
		return;
	}

	TSet<TViewModelPtr<FChannelGroupOutlinerModel>> ChannelGroups;

	for (const FViewModelPtr OutlinerItem : InSelection->Outliner)
	{
		if (const TViewModelPtr<FOutlinerItemModel> OutlinerItemModel = OutlinerItem.ImplicitCast())
		{
			OutlinerItemModel->BuildSidebarMenu(MenuBuilder);
		}
		// Ex. "Location.X", "Rotation.Roll", "Color.R", etc.
        else if (const TViewModelPtr<FChannelGroupOutlinerModel> ChannelGroupOutlinerModel = OutlinerItem.ImplicitCast())
        {
        	ChannelGroupOutlinerModel->BuildSidebarMenu(MenuBuilder);
        	ChannelGroups.Add(ChannelGroupOutlinerModel);
        }
	}

	if (!ChannelGroups.IsEmpty())
	{
		const ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
		const TSharedPtr<FExtensibilityManager> SidebarExtensibilityManager = SequencerModule.GetSidebarExtensibilityManager();
		const TSharedPtr<FExtender> Extender = SidebarExtensibilityManager->GetAllExtenders();

		TArray<FName> ChannelTypeNames;
		TArray<ISequencerChannelInterface*> ChannelInterfaces;
		TArray<FMovieSceneChannelHandle> ChannelHandles;
		TArray<UMovieSceneSection*> SceneSections;

		for (const TViewModelPtr<FChannelGroupOutlinerModel>& ChannelModel : ChannelGroups)
		{
			for (const TSharedRef<IKeyArea>& KeyArea : ChannelModel->GetAllKeyAreas())
			{
				if (ISequencerChannelInterface* const SequencerChannelIterface = KeyArea->FindChannelEditorInterface())
				{
					const FMovieSceneChannelHandle& Channel = KeyArea->GetChannel();

					ChannelTypeNames.Add(Channel.GetChannelTypeName());
					ChannelInterfaces.Add(SequencerChannelIterface);
					ChannelHandles.Add(KeyArea->GetChannel());
					SceneSections.Add(KeyArea->GetOwningSection());
				}
			}
		}

		// Need to make sure all channels are the same type to allow editing of multiple channels as one
		bool bAllChannelNamesEqual = true;
		if (!ChannelTypeNames.IsEmpty())
		{
			for (int32 Index = 0; Index < ChannelTypeNames.Num(); ++Index)
			{
				if (Index > 0 && ChannelTypeNames[Index] != ChannelTypeNames[0])
				{
					bAllChannelNamesEqual = false;
					break;
				}
			}
		}

		// Channel Interface Extensions (Perlin Noise, Easing, Wave)
		if (ChannelInterfaces.Num() > 0)
		{
			if (bAllChannelNamesEqual)
			{
				ChannelInterfaces[0]->ExtendSidebarMenu_Raw(MenuBuilder, Extender, ChannelHandles, SceneSections, WeakSequencer);
			}
			else
			{
				// Display different channels separately and don't allow to edit "all-in-one"
				for (int32 Index = 0; Index < ChannelInterfaces.Num(); ++Index)
				{
					ChannelInterfaces[Index]->ExtendSidebarMenu_Raw(MenuBuilder, Extender, { ChannelHandles[Index] }, { SceneSections[Index] }, WeakSequencer);
				}
			}
		}

		// Curve Channel Options (Pre-Finity, Post-Finity, etc.)
		CurveChannelExtension = MakeShared<FCurveChannelSectionSidebarExtension>(WeakSequencer);
		CurveChannelExtension->AddSections(SceneSections);
		CurveChannelExtension->ExtendMenu(MenuBuilder);
	}
}

void FSequencerSelectionDrawer::BuildMarkedFrameDetails(const TSharedRef<FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder)
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
	const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return FKeyEditData();
	}

	const TSharedPtr<FSequencerSelection> SequencerSelection = Private::GetSelection(*Sequencer.Get());
	if (!SequencerSelection.IsValid())
	{
		return FKeyEditData();
	}

	return Private::GetKeyEditData(SequencerSelection->KeySelection);
}

TSharedPtr<SWidget> FSequencerSelectionDrawer::CreateKeyFrameDetails(const TSharedRef<FSequencerSelection>& InSequencerSelection)
{
	const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return nullptr;
	}

	const FKeyEditData KeyEditData = Private::GetKeyEditData(InSequencerSelection->KeySelection);
	if (KeyEditData.KeyStruct.IsValid())
	{
		return SNew(SKeyEditInterface, Sequencer.ToSharedRef())
			.EditData(this, &FSequencerSelectionDrawer::GetKeyEditData);
	}

	return CreateHintText(LOCTEXT("InvalidKeyCombination", "Selected keys must belong to the same section."));
}

TSharedPtr<SWidget> FSequencerSelectionDrawer::CreateMarkedFrameDetails(const int32 InMarkedFrameIndex)
{
	const TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
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

	const TSharedRef<SMarkedFramePropertyWidget> Widget = SNew(SMarkedFramePropertyWidget, FocusedMovieScene, InMarkedFrameIndex, WeakSequencer);
	Widget->SetEnabled(!AreMarkedFramesLocked());

	return Widget;
}

#undef LOCTEXT_NAMESPACE
