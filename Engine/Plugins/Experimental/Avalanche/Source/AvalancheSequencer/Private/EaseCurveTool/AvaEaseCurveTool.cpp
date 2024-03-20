// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEaseCurveTool.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AvaSequencer.h"
#include "CurveEditor.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "EaseCurveTool/AvaEaseCurveToolCommands.h"
#include "EaseCurveTool/AvaEaseCurveToolSettings.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurveTool.h"
#include "EngineAnalytics.h"
#include "Factories/CurveFactory.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IKeyArea.h"
#include "ISequencer.h"
#include "ISettingsModule.h"
#include "Math/UnrealMathUtility.h"
#include "MVVM/CurveEditorExtension.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Settings/AvaSequencerSettings.h"
#include "Widgets/Notifications/SNotificationList.h"

using namespace UE::Sequencer;

#define LOCTEXT_NAMESPACE "AvaEaseCurveTool"

FAvaEaseCurveTool::FAvaEaseCurveTool(const TSharedRef<FAvaSequencer>& InSequencer)
	: AvaSequencerWeak(InSequencer)
{
	EaseCurve = NewObject<UAvaEaseCurve>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional);

	const TSharedRef<ISequencer> Sequencer = InSequencer->GetSequencer();

	if (const TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = Sequencer->GetViewModel())
	{
		SequencerSelectionWeak = SequencerViewModel->GetSelection();
	}

	UpdateEaseCurveFromSequencerKeySelections();
}

void FAvaEaseCurveTool::CacheSelectionData()
{
	if (!SequencerSelectionWeak.IsValid())
	{
		return;
	}

	const TSharedPtr<FSequencerSelection> SequencerSelection = SequencerSelectionWeak.Pin();

	FKeyDataCache NewKeyCache;

	for (const FKeyHandle Key : SequencerSelection->KeySelection)
	{
		if (Key == FKeyHandle::Invalid())
		{
			continue;
		}

		TViewModelPtr<FChannelModel> ChannelModel = SequencerSelection->KeySelection.GetModelForKey(Key);
		if (!ChannelModel.IsValid())
		{
			continue;
		}

		TSharedPtr<IKeyArea> KeyArea = ChannelModel->GetKeyArea();
		if (!KeyArea.IsValid())
		{
			continue;
		}

		FMovieSceneDoubleChannel* const DoubleChannel = KeyArea->GetChannel().Cast<FMovieSceneDoubleChannel>().Get();
		if (!DoubleChannel)
		{
			continue;
		}

		FKeyDataCache::FChannelData& Entry = NewKeyCache.ChannelKeyData.FindOrAdd(ChannelModel->GetChannelName());
		Entry.ChannelModel = ChannelModel;
		Entry.DoubleChannel = DoubleChannel;
		Entry.Section = ChannelModel->GetSection();
		Entry.KeyHandles.Add(Key);

		NewKeyCache.TotalSelectedKeys++;

		const TArrayView<const FFrameNumber> ChannelTimes = DoubleChannel->GetTimes();
		const int32 AllKeyCount = ChannelTimes.Num();
		const int32 SelectedKeyCount = Entry.KeyHandles.Num();

		if (SelectedKeyCount == 1 && NewKeyCache.TotalSelectedKeys == 1)
		{
			const int32 KeyIndex = DoubleChannel->GetIndex(Entry.KeyHandles[0]);
			if (KeyIndex == AllKeyCount - 1)
			{
				NewKeyCache.bIsLastOnlySelectedKey = true;
			}
		}
		else if (SelectedKeyCount > 1)
		{
			NewKeyCache.bAllChannelSingleKeySelections = false;
		}
	}

	KeyCache = NewKeyCache;
}

void FAvaEaseCurveTool::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(EaseCurve);
}

FString FAvaEaseCurveTool::GetReferencerName() const
{
	return TEXT("AvaEaseCurveTool");
}

TSharedRef<SWidget> FAvaEaseCurveTool::GenerateWidget()
{
	UpdateEaseCurveFromSequencerKeySelections();

	if (!ToolWidget.IsValid())
	{
		ToolWidget = SNew(SAvaEaseCurveTool, SharedThis(this))
			.InitialTangents(GetEaseCurveTangents())
			.Visibility(this, &FAvaEaseCurveTool::GetVisibility)
			.ToolOperation(this, &FAvaEaseCurveTool::GetToolOperation);
	}

	return ToolWidget.ToSharedRef();
}

EVisibility FAvaEaseCurveTool::GetVisibility() const
{
	return (KeyCache.TotalSelectedKeys > 0 && !KeyCache.bIsLastOnlySelectedKey) ? EVisibility::Visible : EVisibility::Collapsed;
}

TObjectPtr<UAvaEaseCurve> FAvaEaseCurveTool::GetToolCurve() const
{
	return EaseCurve;
}

FRichCurve* FAvaEaseCurveTool::GetToolRichCurve() const
{
	return &EaseCurve->FloatCurve;
}

FAvaEaseCurveTangents FAvaEaseCurveTool::GetEaseCurveTangents() const
{
	return EaseCurve->GetTangents();
}

void FAvaEaseCurveTool::SetEaseCurveTangents_Internal(const FAvaEaseCurveTangents& InTangents, const EOperation InOperation, const bool bInBroadcastUpdate)
{
	switch (InOperation)
	{
	case EOperation::InOut:
		EaseCurve->SetTangents(InTangents);
		break;
	case EOperation::In:
		EaseCurve->SetEndTangent(InTangents.End, InTangents.EndWeight);
		break;
	case EOperation::Out:
		EaseCurve->SetStartTangent(InTangents.Start, InTangents.StartWeight);
		break;
	}

	if (bInBroadcastUpdate)
	{
		EaseCurve->BroadcastUpdate();
	}
}

void FAvaEaseCurveTool::SetEaseCurveTangents(const FAvaEaseCurveTangents& InTangents, const EOperation InOperation
	, const bool bInBroadcastUpdate, const bool bInSetSequencerTangents)
{
	const FScopedTransaction Transaction(LOCTEXT("SetEaseCurveTangents", "Set Ease Curve Tangents"));
	EaseCurve->Modify();

	SetEaseCurveTangents_Internal(InTangents, InOperation, bInBroadcastUpdate);

	if (bInSetSequencerTangents)
	{
		SetSequencerKeySelectionTangents(InTangents, InOperation);
	}
}

void FAvaEaseCurveTool::ResetEaseCurveTangents(const EOperation InOperation)
{
	FText TransactionText;

	switch (InOperation)
	{
	case EOperation::InOut:
		TransactionText = LOCTEXT("ResetTangents", "Reset Tangents");
		break;
	case EOperation::In:
		TransactionText = LOCTEXT("ResetEndTangents", "Reset End Tangents");
		break;
	case EOperation::Out:
		TransactionText = LOCTEXT("ResetStartTangents", "Reset Start Tangents");
		break;
	}

	const FScopedTransaction Transaction(TransactionText);
	EaseCurve->ModifyOwner();

	const FAvaEaseCurveTangents ZeroTangents;
	SetEaseCurveTangents_Internal(ZeroTangents, InOperation, /*bInBroadcastUpdate=*/true);
	SetSequencerKeySelectionTangents(ZeroTangents, InOperation);
}

void FAvaEaseCurveTool::FlattenOrStraightenTangents(const EOperation InOperation, const bool bInFlattenTangents) const
{
	FText TransactionText;
	if (bInFlattenTangents)
	{
		switch (InOperation)
		{
		case EOperation::InOut:
			TransactionText = LOCTEXT("FlattenTangents", "Flatten Tangents");
			break;
		case EOperation::In:
			TransactionText = LOCTEXT("FlattenEndTangents", "Flatten End Tangents");
			break;
		case EOperation::Out:
			TransactionText = LOCTEXT("FlattenStartTangents", "Flatten Start Tangents");
			break;
		}
	}
	else
	{
		switch (InOperation)
		{
		case EOperation::InOut:
			TransactionText = LOCTEXT("StraightenTangents", "Straighten Tangents");
			break;
		case EOperation::In:
			TransactionText = LOCTEXT("StraightenEndTangents", "Straighten End Tangents");
			break;
		case EOperation::Out:
			TransactionText = LOCTEXT("StraightenStartTangents", "Straighten Start Tangents");
			break;
		}
	}
	const FScopedTransaction Transaction(TransactionText);
	EaseCurve->ModifyOwner();

	if (InOperation == EOperation::Out || InOperation == EOperation::InOut)
	{
		EaseCurve->FlattenOrStraightenTangents(EaseCurve->GetStartKeyHandle(), bInFlattenTangents);
	}
	if (InOperation == EOperation::In || InOperation == EOperation::InOut)
	{
		EaseCurve->FlattenOrStraightenTangents(EaseCurve->GetEndKeyHandle(), bInFlattenTangents);
	}

	EaseCurve->BroadcastUpdate();
}

void FAvaEaseCurveTool::ApplyQuickEaseToSequencerKeySelections(const EOperation InOperation)
{
	const UAvaEaseCurveToolSettings* const Settings = GetDefault<UAvaEaseCurveToolSettings>();

	FAvaEaseCurveTangents Tangents;
	if (!FAvaEaseCurveTangents::FromString(Settings->GetQuickEaseTangents(), Tangents))
	{
		UE_LOG(LogTemp, Warning, TEXT("Ease curve tool failed to apply quick ease tangents: "
			"Could not parse configured quick ease tangent string."));
		return;
	}

	SetEaseCurveTangents(Tangents, InOperation, /*bInBroadcastUpdate=*/true, /*bInSetSequencerTangents=*/true);

	// Update ease curve combobox widget and zoom graph editor
	if (ToolWidget.IsValid())
	{
		ToolWidget->SetTangents(Tangents, InOperation, /*bInSetEaseCurve=*/false, /*bInBroadcastUpdate=*/false, /*bInSetSequencerTangents=*/false);
	}

	if (FEngineAnalytics::IsAvailable())
	{
		FString ParamValue;
		switch (InOperation)
		{
		case EOperation::InOut:
			ParamValue = TEXT("InOut");
			break;
		case EOperation::In:
			ParamValue = TEXT("In");
			break;
		case EOperation::Out:
			ParamValue = TEXT("Out");
			break;
		}
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MotionDesign.EaseCurveTool"), TEXT("QuickEase"), ParamValue);
	}
}

void FAvaEaseCurveTool::SetSequencerKeySelectionTangents(const FAvaEaseCurveTangents& InTangents, const EOperation InOperation)
{
	CacheSelectionData();

	if (KeyCache.TotalSelectedKeys == 0)
	{
		return;
	}

	const FFrameRate DisplayRate = GetDisplayRate();
	const FFrameRate TickResolution = GetTickResolution();
	const UAvaEaseCurveToolSettings* const EaseCurveToolSettings = GetDefault<UAvaEaseCurveToolSettings>();
	check(EaseCurveToolSettings);
	const bool bAutoFlipTangents = EaseCurveToolSettings->GetAutoFlipTangents();

	KeyCache.ForEachEaseableKey(/*bInIncludeEqualValueKeys=*/true, [&DisplayRate, &TickResolution, &InTangents, InOperation, bAutoFlipTangents](const FKeyHandle& InKeyHandle
		, const FKeyHandle& InNextKeyHandle, const FKeyDataCache::FChannelData& InChannelData)
		{
			TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = InChannelData.DoubleChannel->GetData();

			TArrayView<FMovieSceneDoubleValue> ChannelValues = ChannelData.GetValues();
			const TArrayView<FFrameNumber> ChannelTimes = ChannelData.GetTimes();
			const int32 KeyCount = ChannelValues.Num();

			const int32 KeyIndex = ChannelData.GetIndex(InKeyHandle);
			const int32 NextKeyIndex = ChannelData.GetIndex(InNextKeyHandle);

			FAvaEaseCurveTangents ScaledTangents = InTangents;

			// If there is a key frame after this key frame that we are editing, we check if the that key frame value is less
			// than or greater than this key frame value. If less, flip the tangent (if option is set).
			const bool bIncreasingValue = ChannelValues[NextKeyIndex].Value >= ChannelValues[KeyIndex].Value;

			if (bAutoFlipTangents && !bIncreasingValue)
			{
				ScaledTangents.Start *= -1.f;
				ScaledTangents.End *= -1.f;
			}

			// Scale normalized tangents to time/value range
			ScaledTangents.ScaleUp(ChannelTimes[KeyIndex], ChannelValues[KeyIndex].Value
				, ChannelTimes[NextKeyIndex], ChannelValues[NextKeyIndex].Value
				, DisplayRate, TickResolution);

			const FScopedTransaction Transaction(LOCTEXT("SetSequencerCurveTangents", "Set Sequencer Curve Tangents"));
			InChannelData.Section->Modify();
			InChannelData.Section->MarkAsChanged();

			// Set this keys leave tangent
			if (InOperation == EOperation::Out || InOperation == EOperation::InOut)
			{
				ChannelValues[KeyIndex].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
				ChannelValues[KeyIndex].Tangent.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedBoth;
				ChannelValues[KeyIndex].TangentMode = ERichCurveTangentMode::RCTM_Break;
				ChannelValues[KeyIndex].Tangent.LeaveTangent = ScaledTangents.Start;
				ChannelValues[KeyIndex].Tangent.LeaveTangentWeight = ScaledTangents.StartWeight;
			}

			// Set the next keys arrive tangent
			if (NextKeyIndex != INDEX_NONE && (InOperation == EOperation::In || InOperation == EOperation::InOut))
			{
				ChannelValues[NextKeyIndex].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
				ChannelValues[NextKeyIndex].Tangent.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedBoth;
				ChannelValues[NextKeyIndex].TangentMode = ERichCurveTangentMode::RCTM_Break;
				ChannelValues[NextKeyIndex].Tangent.ArriveTangent = ScaledTangents.End;
				ChannelValues[NextKeyIndex].Tangent.ArriveTangentWeight = ScaledTangents.EndWeight;
			}

			return true;
		});
}

void FAvaEaseCurveTool::UpdateEaseCurveFromSequencerKeySelections()
{
	CacheSelectionData();
	
	const FFrameRate DisplayRate = GetDisplayRate();
	const FFrameRate TickResolution = GetTickResolution();
	const UAvaEaseCurveToolSettings* const EaseCurveToolSettings = GetDefault<UAvaEaseCurveToolSettings>();
	check(EaseCurveToolSettings);
	const bool bAutoFlipTangents = EaseCurveToolSettings->GetAutoFlipTangents();

	TArray<FAvaEaseCurveTangents> KeySetTangents;
	TArray<FAvaEaseCurveTangents> ChangingTangents;

	KeyCache.ForEachEaseableKey(/*bInIncludeEqualValueKeys=*/true, [&DisplayRate, &TickResolution, bAutoFlipTangents, &KeySetTangents, &ChangingTangents]
		(const FKeyHandle& InKeyHandle, const FKeyHandle& InNextKeyHandle, const FKeyDataCache::FChannelData& InChannelData)
		{
			TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = InChannelData.DoubleChannel->GetData();

			const TArrayView<FMovieSceneDoubleValue> ChannelValues = ChannelData.GetValues();
			const TArrayView<FFrameNumber> ChannelTimes = ChannelData.GetTimes();
			const int32 KeyCount = ChannelValues.Num();

			const int32 KeyIndex = ChannelData.GetIndex(InKeyHandle);
			const int32 NextKeyIndex = ChannelData.GetIndex(InNextKeyHandle);

			// If there is a key frame after this key frame that we are editing, we check if the that key frame value is less
			// than or greater than this key frame value. If less, flip the tangent (if option is set).
			const bool bIncreasingValue = ChannelValues[NextKeyIndex].Value >= ChannelValues[KeyIndex].Value;

			FAvaEaseCurveTangents Tangents = FAvaEaseCurveTangents(ChannelValues[KeyIndex], ChannelValues[NextKeyIndex]);

			if (bAutoFlipTangents && !bIncreasingValue)
			{
				Tangents.Start *= -1.f;
				Tangents.End *= -1.f;
			}

			// Scale time/value to normalized tangent range
			FAvaEaseCurveTangents ScaledTangents = Tangents;
			ScaledTangents.Normalize(ChannelTimes[KeyIndex], ChannelValues[KeyIndex].Value
				, ChannelTimes[NextKeyIndex], ChannelValues[NextKeyIndex].Value
				, DisplayRate, TickResolution);

			KeySetTangents.Add(ScaledTangents);
			if (ChannelValues[KeyIndex].Value != ChannelValues[NextKeyIndex].Value)
			{
				ChangingTangents.Add(ScaledTangents);
			}

			return true;
		});

	const FAvaEaseCurveTangents AverageTangents = FAvaEaseCurveTangents::Average(KeySetTangents);
	const FAvaEaseCurveTangents AverageChangingTangents = FAvaEaseCurveTangents::Average(ChangingTangents);

	SetEaseCurveTangents(AverageChangingTangents, EOperation::InOut, /*bInBroadcastUpdate=*/true, false);

	// Update the preset combobox widget
	if (ToolWidget.IsValid())
	{
		ToolWidget->SetTangents(AverageChangingTangents, EOperation::InOut
			, /*bInSetEaseCurve=*/false, /*bInBroadcastUpdate=*/false, /*bInSetSequencerTangents*/false);
	}
}

UCurveBase* FAvaEaseCurveTool::CreateCurveAsset() const
{
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	FString OutNewPackageName;
	FString OutNewAssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(TEXT("/Game/NewCurve"), TEXT(""), OutNewPackageName, OutNewAssetName);

	const TSharedRef<SDlgPickAssetPath> NewAssetDialog =
		SNew(SDlgPickAssetPath)
		.Title(LOCTEXT("CreateExternalCurve", "Create External Curve"))
		.DefaultAssetPath(FText::FromString(OutNewPackageName));

	if (NewAssetDialog->ShowModal() != EAppReturnType::Cancel)
	{
		const FString PackageName = NewAssetDialog->GetFullAssetPath().ToString();
		const FName AssetName = FName(*NewAssetDialog->GetAssetName().ToString());

		UPackage* const Package = CreatePackage(*PackageName);
		
		// Create curve object
		UObject* NewCurveObject = nullptr;
		
		if (UCurveFactory* const CurveFactory = Cast<UCurveFactory>(NewObject<UFactory>(GetTransientPackage(), UCurveFactory::StaticClass())))
		{
			CurveFactory->CurveClass = UCurveFloat::StaticClass();
			NewCurveObject = CurveFactory->FactoryCreateNew(CurveFactory->GetSupportedClass(), Package, AssetName, RF_Public | RF_Standalone, nullptr, GWarn);
		}

		if (NewCurveObject)
		{
			UCurveBase* AssetCurve = nullptr;
			
			// Copy curve data from current curve to newly create curve
			UCurveFloat* const DestCurve = CastChecked<UCurveFloat>(NewCurveObject);
			if (EaseCurve && DestCurve)
			{
				DestCurve->bIsEventCurve = false;

				AssetCurve = DestCurve;

				for (auto It(EaseCurve->FloatCurve.GetKeyIterator()); It; ++It)
				{
					const FRichCurveKey& Key = *It;
					const FKeyHandle KeyHandle = DestCurve->FloatCurve.AddKey(Key.Time, Key.Value);
					DestCurve->FloatCurve.GetKey(KeyHandle) = Key;
				}
			}

			FAssetRegistryModule::AssetCreated(NewCurveObject);

			Package->GetOutermost()->MarkPackageDirty();

			return AssetCurve;
		}
	}

	return nullptr;
}

FAvaEaseCurveTool::EOperation FAvaEaseCurveTool::GetToolOperation() const
{
	return OperationMode;
}

void FAvaEaseCurveTool::SetToolOperation(const FAvaEaseCurveTool::EOperation InNewOperation)
{
	OperationMode = InNewOperation;
}

bool FAvaEaseCurveTool::IsToolOperation(const FAvaEaseCurveTool::EOperation InNewOperation) const
{
	return OperationMode == InNewOperation;
}

bool FAvaEaseCurveTool::CanCopyTangentsToClipboard() const
{
	return true;
}

void FAvaEaseCurveTool::CopyTangentsToClipboard()
{
	FPlatformApplicationMisc::ClipboardCopy(*EaseCurve->GetTangents().ToJson());

	ShowNotificationMessage(LOCTEXT("EaseCurveToolTangentsCopied", "Ease Curve Tool Tangents Copied!"));
}

bool FAvaEaseCurveTool::CanPasteTangentsFromClipboard() const
{
	FAvaEaseCurveTangents Tangents;
	return TangentsFromClipboardPaste(Tangents);
}

void FAvaEaseCurveTool::PasteTangentsFromClipboard()
{
	FAvaEaseCurveTangents Tangents;
	if (TangentsFromClipboardPaste(Tangents))
	{
		EaseCurve->SetTangents(Tangents);
	}
}

bool FAvaEaseCurveTool::TangentsFromClipboardPaste(FAvaEaseCurveTangents& OutTangents)
{
	FString ClipboardString;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardString);

	// Expects four comma separated cubic bezier points that define the curve
	return FAvaEaseCurveTangents::FromString(ClipboardString, OutTangents);
}

bool FAvaEaseCurveTool::IsKeyInterpMode(const ERichCurveInterpMode InInterpMode, const ERichCurveTangentMode InTangentMode)
{
	const FKeyHandle StartKeyHandle = EaseCurve->GetStartKeyHandle();
	return (EaseCurve->FloatCurve.GetKeyInterpMode(StartKeyHandle) == InInterpMode
		&& EaseCurve->FloatCurve.GetKeyTangentMode(StartKeyHandle) == InTangentMode);
}

void FAvaEaseCurveTool::SetKeyInterpMode(const ERichCurveInterpMode InInterpMode, const ERichCurveTangentMode InTangentMode)
{
	const FKeyHandle StartKeyHandle = EaseCurve->GetStartKeyHandle();

	const FScopedTransaction Transaction(LOCTEXT("CurveEditor_SetInterpolationMode", "Select Interpolation Mode"));
	EaseCurve->ModifyOwner();

	EaseCurve->FloatCurve.SetKeyInterpMode(StartKeyHandle, InInterpMode);
	EaseCurve->FloatCurve.SetKeyTangentMode(StartKeyHandle, InTangentMode);

	if (InInterpMode != ERichCurveInterpMode::RCIM_Cubic)
	{
		FRichCurveKey& StartKey = EaseCurve->GetStartKey();
		StartKey.LeaveTangentWeight = 0.f;
		
		FRichCurveKey& EndKey = EaseCurve->GetEndKey();
		EndKey.ArriveTangentWeight = 0.f;
	}

	TArray<FRichCurveEditInfo> ChangedCurveEditInfos;
	ChangedCurveEditInfos.Add(FRichCurveEditInfo(&EaseCurve->FloatCurve));
	EaseCurve->OnCurveChanged(ChangedCurveEditInfos);
}

void FAvaEaseCurveTool::BeginTransaction(const FText& InDescription) const
{
	if (GEditor)
	{
		EaseCurve->ModifyOwnerChange();

		GEditor->BeginTransaction(InDescription);
	}
}

void FAvaEaseCurveTool::EndTransaction() const
{
	if (GEditor)
	{
		GEditor->EndTransaction();
	}
}

void FAvaEaseCurveTool::UndoAction()
{
	if (GEditor && GEditor->UndoTransaction())
	{
		UpdateEaseCurveFromSequencerKeySelections();
	}
}

void FAvaEaseCurveTool::RedoAction()
{
	if (GEditor && GEditor->RedoTransaction())
	{
		UpdateEaseCurveFromSequencerKeySelections();
	}
}

void FAvaEaseCurveTool::PostUndo(bool bInSuccess)
{
	UndoAction();
}

void FAvaEaseCurveTool::PostRedo(bool bInSuccess)
{
	RedoAction();
}

void FAvaEaseCurveTool::OpenToolSettings() const
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer(TEXT("Editor"), TEXT("Motion Design"), TEXT("Ease Curve Tool"));
	}
}

FFrameRate FAvaEaseCurveTool::GetTickResolution() const
{
	if (TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin())
	{
		return AvaSequencer->GetSequencer()->GetFocusedTickResolution();
	}

	return FFrameRate();
}

FFrameRate FAvaEaseCurveTool::GetDisplayRate() const
{
	if (TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin())
	{
		return AvaSequencer->GetSequencer()->GetFocusedDisplayRate();
	}

	// Fallback to using config display rate if tool is being used outside sequencer
	return GetDefault<UAvaSequencerSettings>()->GetDisplayRate();
}

void FAvaEaseCurveTool::ShowNotificationMessage(const FText& InMessageText)
{
	FNotificationInfo Info(InMessageText);
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FAvaEaseCurveTool::SelectNextChannelKey()
{
	// This command is only available for single key selections
	if (KeyCache.TotalSelectedKeys != 1 || !SequencerSelectionWeak.IsValid())
	{
		return;
	}

	const TSharedPtr<FSequencerSelection> SequencerSelection = SequencerSelectionWeak.Pin();

	int32 SelectedKeyIndex = INDEX_NONE;
	FKeyHandle SelectedKeyHandle;
	FKeyHandle NextKeyHandle;
	TViewModelPtr<FChannelModel> ChannelModel;

	for (const TPair<FName, FKeyDataCache::FChannelData>& ChannelEntry : KeyCache.ChannelKeyData)
	{
		if (ChannelEntry.Value.KeyHandles.Num() == 1)
		{
			SelectedKeyHandle = ChannelEntry.Value.KeyHandles[0];

			TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = ChannelEntry.Value.DoubleChannel->GetData();
			const TArrayView<const FFrameNumber> KeyTimes = ChannelData.GetTimes();

			SelectedKeyIndex = ChannelData.GetIndex(SelectedKeyHandle);
			if (KeyTimes.IsValidIndex(SelectedKeyIndex + 1))
			{
				NextKeyHandle = ChannelData.GetHandle(SelectedKeyIndex + 1);

				ChannelModel = ChannelEntry.Value.ChannelModel;
			}
		}
		break;
	}

	if (SelectedKeyIndex == INDEX_NONE
		|| SelectedKeyHandle == FKeyHandle::Invalid()
		|| NextKeyHandle == FKeyHandle::Invalid()
		|| !ChannelModel.IsValid())
	{
		return;
	}
	
	SequencerSelection->KeySelection.Deselect(SelectedKeyHandle);
	SequencerSelection->KeySelection.Select(ChannelModel, NextKeyHandle);

	UpdateEaseCurveFromSequencerKeySelections();
}

void FAvaEaseCurveTool::SelectPreviousChannelKey()
{
	// This command is only available for single key selections
	if (KeyCache.TotalSelectedKeys != 1 || !SequencerSelectionWeak.IsValid())
	{
		return;
	}

	const TSharedPtr<FSequencerSelection> SequencerSelection = SequencerSelectionWeak.Pin();

	int32 SelectedKeyIndex = INDEX_NONE;
	FKeyHandle SelectedKeyHandle;
	FKeyHandle PreviousKeyHandle;
	TViewModelPtr<FChannelModel> ChannelModel;

	for (const TPair<FName, FKeyDataCache::FChannelData>& ChannelEntry : KeyCache.ChannelKeyData)
	{
		if (ChannelEntry.Value.KeyHandles.Num() == 1)
		{
			SelectedKeyHandle = ChannelEntry.Value.KeyHandles[0];

			TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = ChannelEntry.Value.DoubleChannel->GetData();
			const TArrayView<const FFrameNumber> KeyTimes = ChannelData.GetTimes();

			SelectedKeyIndex = ChannelData.GetIndex(SelectedKeyHandle);
			if (KeyTimes.IsValidIndex(SelectedKeyIndex - 1))
			{
				PreviousKeyHandle = ChannelData.GetHandle(SelectedKeyIndex - 1);

				ChannelModel = ChannelEntry.Value.ChannelModel;
			}
		}
		break;
	}

	if (SelectedKeyIndex == INDEX_NONE
		|| SelectedKeyHandle == FKeyHandle::Invalid()
		|| PreviousKeyHandle == FKeyHandle::Invalid()
		|| !ChannelModel.IsValid())
	{
		return;
	}

	SequencerSelection->KeySelection.Deselect(SelectedKeyHandle);
	SequencerSelection->KeySelection.Select(ChannelModel, PreviousKeyHandle);

	UpdateEaseCurveFromSequencerKeySelections();
}

bool FAvaEaseCurveTool::HasCachedKeysToEase()
{
	bool bEaseableKeys = false;

	KeyCache.ForEachEaseableKey(/*bInIncludeEqualValueKeys=*/false, [&bEaseableKeys](const FKeyHandle& InKeyHandle
		, const FKeyHandle& InNextKeyHandle, const FKeyDataCache::FChannelData& InChannelData)
		{
			bEaseableKeys = true;
			return false;
		});

	return bEaseableKeys;
}

void FAvaEaseCurveTool::FKeyDataCache::ForEachEaseableKey(const bool bInIncludeEqualValueKeys
	, TFunctionRef<bool(const FKeyHandle& /*InKeyHandle*/, const FKeyHandle& /*InNextKeyHandle*/, const FChannelData&)> InCallable)
{
	for (const TPair<FName, FKeyDataCache::FChannelData>& ChannelEntry : ChannelKeyData)
	{
		TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = ChannelEntry.Value.DoubleChannel->GetData();
		const TArrayView<const FMovieSceneDoubleValue> ChannelValues = ChannelData.GetValues();
		const int32 KeyCount = ChannelValues.Num();
		
		for (const FKeyHandle& KeyHandle : ChannelEntry.Value.KeyHandles)
		{
			if (KeyHandle == FKeyHandle::Invalid())
			{
				continue;
			}

			const int32 KeyIndex = ChannelData.GetIndex(KeyHandle);
			if (KeyIndex == INDEX_NONE)
			{
				continue;
			}
			
			// If there is no key after the selected key, we don't need to process.
			// The arrive tangents of this key will be set by the previous key's processing.
			int32 NextKeyIndex = KeyIndex + 1;
			NextKeyIndex = (NextKeyIndex < KeyCount) ? NextKeyIndex : INDEX_NONE;
			if (NextKeyIndex == INDEX_NONE)
			{
				continue;
			}

			// Need to check if the next key index is valid, otherwise GetHandle() will fail.
			const FKeyHandle NextKeyHandle = ChannelData.GetHandle(NextKeyIndex);
			if (NextKeyHandle == FKeyHandle::Invalid())
			{
				continue;
			}

			if (!bInIncludeEqualValueKeys && ChannelValues[KeyIndex].Value == ChannelValues[NextKeyIndex].Value)
			{
				continue;
			}
			
			if (!InCallable(KeyHandle, NextKeyHandle, ChannelEntry.Value))
			{
				return;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
