// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "GeometryMaskCanvas.h"
#include "GeometryMaskCanvasResource.h"
#include "SceneView.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

#include "GeometryMaskSubsystem.generated.h"

using FOnGeometryMaskCanvasCreated = TMulticastDelegate<void(const UGeometryMaskCanvas*)>;
using FOnGeometryMaskResourceCreated = TMulticastDelegate<void(const UGeometryMaskCanvasResource*)>;

/** Maintains the registered named canvases. */
UCLASS(BlueprintType)
class GEOMETRYMASK_API UGeometryMaskSubsystem
	: public UEngineSubsystem
{
	GENERATED_BODY()

public:	
	/** Retrieves a Canvas, uniquely identified by it's name. */
	UFUNCTION(BlueprintCallable, Category = "Canvas")
	UGeometryMaskCanvas* GetNamedCanvas(FName InName);

	/** Returns all registered canvas names. */
	UFUNCTION(BlueprintCallable, Category = "Canvas")
	static TArray<FName> GetCanvasNames();

	const TArray<TObjectPtr<UGeometryMaskCanvasResource>>& GetCanvasResources() const;

	void Update(UWorld* InWorld, FSceneViewFamily& InViewFamily);

	/** Remove all canvases without any Readers or Writers. Return the number of canvases removed. */
	int32 RemoveWithoutWriters();

	/** Called when a new canvas is created due to a unique name being requested. */
	FOnGeometryMaskCanvasCreated& OnGeometryMaskCanvasCreated() { return OnGeometryMaskCanvasCreatedDelegate; }

	/** Called when a new canvas resource is created. */
	FOnGeometryMaskResourceCreated& OnGeometryMaskResourceCreated() { return OnGeometryMaskResourceCreatedDelegate; }

private:
	/** Find and assign the next available resource to the given canvas. */
	void AssignResourceToCanvas(UGeometryMaskCanvas* InCanvas);

	void OnCanvasActivated(UGeometryMaskCanvas* InCanvas);
	void OnCanvasDeactivated(UGeometryMaskCanvas* InCanvas);

private:
	friend class UGeometryMaskWorldSubsystem;

	FOnGeometryMaskCanvasCreated OnGeometryMaskCanvasCreatedDelegate;
	FOnGeometryMaskResourceCreated OnGeometryMaskResourceCreatedDelegate;

	UPROPERTY()
	TMap<FName, TObjectPtr<UGeometryMaskCanvas>> NamedCanvases;

	/** Pool of GPU/Texture resources used by the canvases. */
	UPROPERTY(Getter)
	TArray<TObjectPtr<UGeometryMaskCanvasResource>> CanvasResources;
};
