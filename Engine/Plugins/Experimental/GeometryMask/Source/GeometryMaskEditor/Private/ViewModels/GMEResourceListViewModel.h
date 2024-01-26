// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "GeometryMaskCanvas.h"
#include "GMEViewModelShared.h"

class FGMEResourceItemViewModel;

class FGMEResourceListViewModel
	: public TSharedFromThis<FGMEResourceListViewModel>
	, public FEditorUndoClient
	, public IGMETreeNodeViewModel
{
public:
	/**  */
	static TSharedRef<FGMEResourceListViewModel> Create();
	virtual ~FGMEResourceListViewModel() override;

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
	explicit FGMEResourceListViewModel(FPrivateToken) { }

protected:
	void Initialize();

	void OnResourceCreated(const UGeometryMaskCanvasResource* InGeometryMaskResource);

private:
	FOnChanged OnChangedDelegate;
	FDelegateHandle OnResourceCreatedHandle;

	/** Canvas ViewModels */
	TArray<TSharedPtr<FGMEResourceItemViewModel>> ResourceItems;
};
