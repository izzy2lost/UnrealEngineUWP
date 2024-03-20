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
class IAvaSequencer;
class ISequencer;
class SAvaEaseCurveTool;
class SCurveEditor;
class SCurveEditorPanel;
class SEditableTextBox;
class UAvaEaseCurve;
class UCurveFloat;
class UMovieSceneSection;
struct FAvaEaseCurveTangents;
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
	/** Current default and only implemented is DualKeyEdit. */
	enum class EMode : uint8
	{
		/** Edits the selected key's leave tangent and the next key's arrive tangent in the curve editor graph. */
		DualKeyEdit,
		/** Edits only the selected key.
		 * The leave tangent in the curve editor graph will set the sequence key arrive tangent.
		 * The arrive tangent in the curve editor graph will set the sequence key leave tangent. */
		SingleKeyEdit
	};

	enum class EOperation : uint8
	{
		InOut,
		In,
		Out
	};

	static void ShowNotificationMessage(const FText& InMessageText);

	/** Returns true if the clipboard paste data contains tangent information. */
	static bool TangentsFromClipboardPaste(FAvaEaseCurveTangents& OutTangents);

	FAvaEaseCurveTool(const TSharedRef<FAvaSequencer>& InSequencer);

	TSharedRef<SWidget> GenerateWidget();

	EVisibility GetVisibility() const;

	TObjectPtr<UAvaEaseCurve> GetToolCurve() const;
	FRichCurve* GetToolRichCurve() const;

	FAvaEaseCurveTangents GetEaseCurveTangents() const;

	/**
	 * Sets the internal ease curve tangents and optionally broadcasts a change event for the curve object.
	 * This is different from SetEaseCurveTangents_Internal in that it performs undo/redo transactions and
	 * optionally sets the selected tangents in the actual sequence.
	 */
	void SetEaseCurveTangents(const FAvaEaseCurveTangents& InTangents, const EOperation InOperation, const bool bInBroadcastUpdate, const bool bInSetSequencerTangents);

	void ResetEaseCurveTangents(const EOperation InOperation);

	void FlattenOrStraightenTangents(const EOperation InOperation, const bool bInFlattenTangents) const;

	/** Creates a new external float curve from the internet curve editor curve. */
	UCurveBase* CreateCurveAsset() const;

	void SetSequencerKeySelectionTangents(const FAvaEaseCurveTangents& InTangents, const EOperation InOperation = EOperation::InOut);

	void ApplyQuickEaseToSequencerKeySelections(const EOperation InOperation = EOperation::InOut);

	/** Updates the ease curve graph view based on the active sequencer key selection. */
	void UpdateEaseCurveFromSequencerKeySelections();

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

	bool HasCachedKeysToEase();

	//~ Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject

	//~ Begin FEditorUndoClient
	virtual void PostUndo(bool bInSuccess);
	virtual void PostRedo(bool bInSuccess);
	//~ End FEditorUndoClient

protected:
	/** 
	 * Sets the internal ease curve tangents and optionally broadcasts a change event for the curve object.
	 * Changing the internal ease curve tangents will be directly reflected in the ease curve editor graph.
	 */
	void SetEaseCurveTangents_Internal(const FAvaEaseCurveTangents& InTangents, const EOperation InOperation, const bool bInBroadcastUpdate);

	TWeakPtr<FAvaSequencer> AvaSequencerWeak;
	TWeakPtr<UE::Sequencer::FSequencerSelection> SequencerSelectionWeak;

	TObjectPtr<UAvaEaseCurve> EaseCurve;

	EMode ToolMode = EMode::DualKeyEdit;
	EOperation OperationMode = EOperation::InOut;

	TSharedPtr<SAvaEaseCurveTool> ToolWidget;
	
	/** Cached data set when a new sequencer selection is made. */
	struct FKeyDataCache
	{
		struct FChannelData
		{
			TSharedPtr<UE::Sequencer::FChannelModel> ChannelModel;
			UMovieSceneSection* Section = nullptr;
			FMovieSceneDoubleChannel* DoubleChannel = nullptr;
			TArray<FKeyHandle> KeyHandles;
		};

		void ForEachEaseableKey(const bool bInIncludeEqualValueKeys
			, TFunctionRef<bool(const FKeyHandle& /*InKeyHandle*/, const FKeyHandle& /*InNextKeyHandle*/, const FChannelData&)> InCallable);

		TMap<FName, FChannelData> ChannelKeyData;

		int32 TotalSelectedKeys = 0;

		/** Indicates only one selected key on each of the selected channels. */
		bool bAllChannelSingleKeySelections = true;

		/** Indicates only one key selected and it is the last key of the channel. */
		bool bIsLastOnlySelectedKey = false;
	};
	FKeyDataCache KeyCache;

private:
	void CacheSelectionData();
};
