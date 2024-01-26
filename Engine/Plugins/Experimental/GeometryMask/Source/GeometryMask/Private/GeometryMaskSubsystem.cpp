// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskSubsystem.h"

#include "Algo/RemoveIf.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GeometryMaskCanvas.h"
#include "GeometryMaskCanvasResource.h"
#include "SceneView.h"
#include "UnrealClient.h"

UGeometryMaskCanvas* UGeometryMaskSubsystem::GetNamedCanvas(FName InName)
{
	if (InName.IsNone())
	{
		static FName DefaultCanvasName = TEXT("Default");
		InName = DefaultCanvasName;
	}

	const FName ObjectName = MakeUniqueObjectName(this, UGeometryMaskCanvas::StaticClass(), FName(FString::Printf(TEXT("GeometryMaskCanvas_%s_"), *InName.ToString())));
	if (TObjectPtr<UGeometryMaskCanvas>* FoundCanvas = NamedCanvases.Find(InName))
	{
		return *FoundCanvas;
	}

	const TObjectPtr<UGeometryMaskCanvas>& NewCanvas = NamedCanvases.Emplace(InName, NewObject<UGeometryMaskCanvas>(this, ObjectName));
	NewCanvas->CanvasName = InName;
	AssignResourceToCanvas(NewCanvas);

	NewCanvas->OnActivated().BindUObject(this, &UGeometryMaskSubsystem::OnCanvasActivated, NewCanvas.Get());
	NewCanvas->OnDeactivated().BindUObject(this, &UGeometryMaskSubsystem::OnCanvasDeactivated, NewCanvas.Get());
	
	OnGeometryMaskCanvasCreatedDelegate.Broadcast(NewCanvas);

	return NewCanvas;
}

TArray<FName> UGeometryMaskSubsystem::GetCanvasNames()
{
	if (const UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		TArray<FName> CanvasNames;
		Subsystem->NamedCanvases.GenerateKeyArray(CanvasNames);
		return CanvasNames;
	}
	
	return {};
}

const TArray<TObjectPtr<UGeometryMaskCanvasResource>>& UGeometryMaskSubsystem::GetCanvasResources() const
{
	return CanvasResources;
}

void UGeometryMaskSubsystem::Update(
	UWorld* InWorld,
	FSceneViewFamily& InViewFamily)
{
	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		for (const FSceneView*& View : InViewFamily.Views)
		{
			FSceneView* MutableView = const_cast<FSceneView*>(View);
			for (const TPair<FName, TObjectPtr<UGeometryMaskCanvas>>& NamedCanvas : Subsystem->NamedCanvases)
			{
				NamedCanvas.Value->Update(InWorld, *MutableView);			
			}

			for (const TObjectPtr<UGeometryMaskCanvasResource>& Resource : CanvasResources)
			{
				Resource->Update(InWorld, *MutableView);
			}
		}
	}
}

int32 UGeometryMaskSubsystem::RemoveWithoutWriters()
{
	int32 NumRemoved = 0;
	
	TMap<FName, TObjectPtr<UGeometryMaskCanvas>> UsedCanvases;
	UsedCanvases.Reserve(NamedCanvases.Num());
	
	for (const TPair<FName, TObjectPtr<UGeometryMaskCanvas>>& NamedCanvas : NamedCanvases)
	{
		if (NamedCanvas.Key == NAME_None
			|| !NamedCanvas.Value->GetWriters().IsEmpty())
		{
			UsedCanvases.Emplace(NamedCanvas.Key, NamedCanvas.Value);
		}
		else
		{
			NamedCanvas.Value->FreeResource();
			++NumRemoved;
		}
	}
	
	NamedCanvases = UsedCanvases;

	return NumRemoved;
}

void UGeometryMaskSubsystem::AssignResourceToCanvas(UGeometryMaskCanvas* InCanvas)
{
	UGeometryMaskCanvasResource* AvailableResource = nullptr;
	EGeometryMaskColorChannel AvailableChannel = EGeometryMaskColorChannel::None;
	for (UGeometryMaskCanvasResource* CanvasResource : CanvasResources)
	{
		if (AvailableChannel = CanvasResource->GetNextAvailableColorChannel();
			AvailableChannel != EGeometryMaskColorChannel::None)
		{
			AvailableResource = CanvasResource;
		}
	}
	
	// Nothing available, create new resource
	if (AvailableChannel == EGeometryMaskColorChannel::None)
	{
		AvailableResource = CanvasResources.Emplace_GetRef(NewObject<UGeometryMaskCanvasResource>(this));
		AvailableChannel = AvailableResource->GetNextAvailableColorChannel();
	}

	AvailableResource->Checkout(AvailableChannel, InCanvas->GetCanvasName());
	InCanvas->AssignResource(AvailableResource, AvailableChannel);
}

void UGeometryMaskSubsystem::OnCanvasActivated(UGeometryMaskCanvas* InCanvas)
{
	if (InCanvas->IsDefaultCanvas())
	{
		return;
	}

	// Already has a resource
	if (InCanvas->GetResource())
	{
		return;
	}

	// Provide a new resource for the canvas to write to
	AssignResourceToCanvas(InCanvas);
}

void UGeometryMaskSubsystem::OnCanvasDeactivated(UGeometryMaskCanvas* InCanvas)
{
	if (InCanvas->IsDefaultCanvas())
	{
		return;
	}

	if (const UGeometryMaskCanvasResource* CanvasResource = InCanvas->GetResource())
	{
		// Resource assigned, so free it up
		InCanvas->FreeResource();
	}
}
