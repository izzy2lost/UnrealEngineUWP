// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "EditorUndoClient.h"
#include "GeometryMaskCanvasResource.h"
#include "GMEViewModelShared.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class UGeometryMaskCanvas;
class UTexture;

class FGMECanvasItemViewModel
	: public TSharedFromThis<FGMECanvasItemViewModel>
	, public FEditorUndoClient
	, public IGMETreeNodeViewModel
{
public:
	/**  */
	static TSharedRef<FGMECanvasItemViewModel> Create(const TWeakObjectPtr<const UGeometryMaskCanvas>& InCanvas);
	virtual ~FGMECanvasItemViewModel() override = default;
	
	// ~Begin IGMETreeNodeViewModel
	virtual bool GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren) override;
	// ~End IGMETreeNodeViewModel

	const FName& GetCanvasName() const { return CanvasName; }
	const EGeometryMaskColorChannel GetColorChannel() const { return ColorChannel; }
	const UTexture* GetCanvasTexture() const;
	float GetMemoryUsage();

private:
	// Private token only allows members or friends to call MakeShared
	struct FPrivateToken { explicit FPrivateToken() = default; };

public:
	FGMECanvasItemViewModel(FPrivateToken, const TWeakObjectPtr<const UGeometryMaskCanvas>& InCanvas);

private:
	FName CanvasName;
	EGeometryMaskColorChannel ColorChannel;
	TWeakObjectPtr<UTexture> CanvasTexture;
	int32 KnownReaderCount;
	int32 KnownWriterCount;
};
