// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "EditorUndoClient.h"
#include "GeometryMaskCanvas.h"
#include "GMEViewModelShared.h"

class FGMECanvasItemViewModel;

class FGMECanvasListViewModel
	: public TSharedFromThis<FGMECanvasListViewModel>
	, public FEditorUndoClient
	, public IGMETreeNodeViewModel
{
public:
	/**  */
	static TSharedRef<FGMECanvasListViewModel> Create();
	virtual ~FGMECanvasListViewModel() override;

	// ~Begin IGMETreeNodeViewModel
	virtual bool GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren) override;
	// ~End IGMETreeNodeViewModel

public:
	using FOnChanged = TMulticastDelegate<void()>;;

	/** Something has changed within the ViewModel */
	FOnChanged& OnChanged() { return OnChangedDelegate; }

private:
	// Private token only allows members or friends to call MakeShared
	struct FPrivateToken { explicit FPrivateToken() = default; };
	
public:
	explicit FGMECanvasListViewModel(FPrivateToken);

private:
	void Initialize();

	void RefreshCanvases();

	void OnCanvasCreated(const UGeometryMaskCanvas* InGeometryMaskCanvas);

	bool Tick(const float InDeltaSeconds);

private:
	/** In seconds. */
	static constexpr float UpdateCheckInterval = 1.0f;
	FTSTicker::FDelegateHandle UpdateCheckHandle;
	
	FOnChanged OnChangedDelegate;
	FDelegateHandle OnCanvasCreatedHandle;

	/** Cached canvas names for comparison/refresh. */
	TArray<FName> LastCanvasNames;

	/** Canvas ViewModels */
	TArray<TSharedPtr<FGMECanvasItemViewModel>> CanvasItems;
};
