// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EaseCurveTool/AvaEaseCurve.h"
#include "Curves/RichCurve.h"
#include "Curves/KeyHandle.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurvePreset.h"
#include "Editor/Transactor.h"
#include "EditorUndoClient.h"
#include "ScopedTransaction.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FAvaSequencer;
class FCurveEditor;
class SAvaEaseCurveTool;
class SCurveEditor;
class SCurveEditorPanel;
class SEditableTextBox;
class UCurveFloat;
struct FMovieSceneDoubleChannel;

namespace UE::Sequencer
{
	class FChannelModel;
	class FSequencerSelection;
}

class FAvaEaseCurveTool
	: public TSharedFromThis<FAvaEaseCurveTool>
	, public FGCObject
	, public FSelfRegisteringEditorUndoClient
{
public:
	static void ShowNotificationMessage(const FText& InMessageText);

	/** Returns true if the clipboard paste data contains tangent information. */
	static bool TangentsFromClipboardPaste(FAvaEaseCurveTangents& OutTangents);

	/**
	 * Current default and only implemented is DoubleKeyEdit.
	 */
	enum class EMode : uint8
	{
		/** Edits the selected key's leave tangent and the next key's arrive tangent in the curve editor graph. */
		DoubleKeyEdit,
		/** Edits only the selected key.
		 * The leave tangent in the curve editor graph will set the sequence key arrive tangent.
		 * The arrive tangent in the curve editor graph will set the sequence key leave tangent. */
		SingleKeyEdit
	};

	enum class EOperation : uint8
	{
		In,
		Out,
		InOut
	};

	FAvaEaseCurveTool(const TSharedRef<FAvaSequencer>& InSequencer);

	TSharedRef<SWidget> GenerateWidget();

	EVisibility GetVisibility() const;

	TObjectPtr<UAvaEaseCurve> GetToolCurve() const;
	FRichCurve* GetToolRichCurve() const;

	FAvaEaseCurveTangents GetEaseCurveTangents() const;
	void SetEaseCurveTangents(const FAvaEaseCurveTangents& InTangents, const bool bBroadcastUpdate, const bool bInSetSequencerTangents);
	void ResetEaseCurveTangents(const bool bInStartTangent, const bool bInEndTangent);

	void FlattenOrStraightenTangents(const bool bInStartTangent, const bool bInEndTangent, const bool bInFlattenTangents) const;

	FORCEINLINE EOperation GetOperation() const { return OperationMode; }
	FORCEINLINE void SetOperation(const EOperation InOperation) { OperationMode = InOperation; }

	/** Creates a new external float curve from the internet curve editor curve. */
	UCurveBase* CreateCurveAsset() const;
	
	void SetSequencerKeySelectionTangents(const FAvaEaseCurveTangents& InTangents);

	/** Updates the ease curve graph view based on the active sequencer key selection. */
	void UpdateEaseCurveFromSequencerKeySelections();

	void ApplyEaseCurveToSequencerKeySelections();

	EOperation GetToolOperation() const;
	void SetToolOperation(const EOperation InNewOperation);
	bool IsToolOperation(const EOperation InNewOperation) const;

	bool CanCopyTangentsToClipboard() const;
	void CopyTangentsToClipboard();
	bool CanPasteTangentsFromClipboard() const;
	void PasteTangentsFromClipboard();

	bool IsKeyInterpMode(const ERichCurveInterpMode InInterpMode, const ERichCurveTangentMode InTangentMode);
	void SetKeyInterpMode(const ERichCurveInterpMode InInterpMode, const ERichCurveTangentMode InTangentMode);

	void BeginTransaction(const FText& InDescription) const;
	void EndTransaction() const;

	void UndoAction();
	void RedoAction();

	void OpenToolSettings() const;

	FFrameRate GetTickResolution() const;
	FFrameRate GetDisplayRate() const;

	void SelectNextChannelKey();
	void SelectPreviousChannelKey();

	//~ Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject

	//~ Begin FEditorUndoClient
	virtual void PostUndo(bool bInSuccess);
	virtual void PostRedo(bool bInSuccess);
	//~ End FEditorUndoClient

protected:
	TWeakPtr<FAvaSequencer> AvaSequencerWeak;
	TWeakPtr<UE::Sequencer::FSequencerSelection> SequencerSelectionWeak;
	TWeakPtr<FCurveEditor> SequencerCurveEditorWeak;

	TObjectPtr<UAvaEaseCurve> EaseCurve;

	EMode ToolMode = EMode::DoubleKeyEdit;
	EOperation OperationMode = EOperation::InOut;

	TSharedPtr<SAvaEaseCurveTool> CurveEaseToolWidget;

	FSimpleMulticastDelegate::FDelegate LastDelegate;
	
	/** Cached data set when a new sequencer selection is made. */
	struct FCachedKeyData
	{
		TSharedPtr<UE::Sequencer::FChannelModel> ChannelModel;
		UMovieSceneSection* Section = nullptr;
		FMovieSceneDoubleChannel* DoubleChannel = nullptr;
		TArray<FKeyHandle> KeyHandles;
	};
	TMap<FName, FCachedKeyData> CachedChannelKeyData;
	int32 TotalSelectedKeys = 0;

	/** Indicates only one selected key on each of the selected channels. */
	bool bCachedAllChannelSingleKeySelections = false;

	/** Indicates only one key selected and it is the last key of the channel. */
	bool bCachedIsLastOnlySelectedKey = false;

private:
	void CacheSelectionData(const TSharedRef<UE::Sequencer::FSequencerSelection>& InSequencerSelection);
};
