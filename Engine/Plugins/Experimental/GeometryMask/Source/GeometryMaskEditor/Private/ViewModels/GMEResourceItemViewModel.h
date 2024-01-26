// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "EditorUndoClient.h"
#include "GMEViewModelShared.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class UGeometryMaskCanvasResource;
class UGeometryMaskCanvas;
class UTexture;
class UTextureRenderTarget2D;

class FGMEResourceItemViewModel
	: public TSharedFromThis<FGMEResourceItemViewModel>
	, public FEditorUndoClient
	, public IGMETreeNodeViewModel
{
public:
	/**  */
	static TSharedRef<FGMEResourceItemViewModel> Create(const TWeakObjectPtr<const UGeometryMaskCanvasResource>& InResource);
	virtual ~FGMEResourceItemViewModel() override = default;
	
	// ~Begin IGMETreeNodeViewModel
	virtual bool GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren) override;
	// ~End IGMETreeNodeViewModel

	uint32 GetId() const { return UniqueId; }

	const UTextureRenderTarget2D* GetResourceTexture() const { return ResourceTexture.Get(); }
	float GetMemoryUsage() const;
	FIntPoint GetDimensions() const;

private:
	// Private token only allows members or friends to call MakeShared
	struct FPrivateToken { explicit FPrivateToken() = default; };

public:
	FGMEResourceItemViewModel(FPrivateToken, const TWeakObjectPtr<const UGeometryMaskCanvasResource>& InResource);

private:
	uint32 UniqueId;
	TWeakObjectPtr<UTextureRenderTarget2D> ResourceTexture;
};
