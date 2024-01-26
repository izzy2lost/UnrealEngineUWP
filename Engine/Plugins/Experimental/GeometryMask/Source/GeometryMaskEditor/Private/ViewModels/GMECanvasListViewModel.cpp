// Copyright Epic Games, Inc. All Rights Reserved.

#include "GMECanvasListViewModel.h"

#include "Editor.h"
#include "GeometryMaskSubsystem.h"
#include "GMECanvasItemViewModel.h"

TSharedRef<FGMECanvasListViewModel> FGMECanvasListViewModel::Create()
{
	TSharedRef<FGMECanvasListViewModel> ViewModel = MakeShared<FGMECanvasListViewModel>(FPrivateToken{});
	ViewModel->Initialize();

	return ViewModel;
}

FGMECanvasListViewModel::~FGMECanvasListViewModel()
{
	FTSTicker::GetCoreTicker().RemoveTicker(UpdateCheckHandle);
	
	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		Subsystem->OnGeometryMaskCanvasCreated().Remove(OnCanvasCreatedHandle);
	}
	
	CanvasItems.Reset();
}

FGMECanvasListViewModel::FGMECanvasListViewModel(FPrivateToken)
{
	UpdateCheckHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FGMECanvasListViewModel::Tick), UpdateCheckInterval);
}

void FGMECanvasListViewModel::Initialize()
{
	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		RefreshCanvases();

		// And listen for new ones
		{
			OnCanvasCreatedHandle = Subsystem->OnGeometryMaskCanvasCreated().AddRaw(this, &FGMECanvasListViewModel::OnCanvasCreated);
		}

		OnChanged().Broadcast();
	}
}

void FGMECanvasListViewModel::RefreshCanvases()
{
	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		// Add currently registered canvases
		{
			TArray<FName> CanvasNames = Subsystem->GetCanvasNames();
			CanvasItems.Reset();
			CanvasItems.Reserve(CanvasNames.Num());
		
			for (const FName CanvasName : CanvasNames)
			{
				UGeometryMaskCanvas* Canvas = Subsystem->GetNamedCanvas(CanvasName);
				CanvasItems.Add(FGMECanvasItemViewModel::Create(Canvas));
			}
		}
	}
}

void FGMECanvasListViewModel::OnCanvasCreated(const UGeometryMaskCanvas* InGeometryMaskCanvas)
{
	// Don't add if already in list
	if (CanvasItems.ContainsByPredicate([CanvasName = InGeometryMaskCanvas->GetCanvasName()](const TSharedPtr<FGMECanvasItemViewModel>& InItemViewModel)
	{
		return InItemViewModel->GetCanvasName() == CanvasName;
	}))
	{
		return;
	}

	CanvasItems.Add(FGMECanvasItemViewModel::Create(InGeometryMaskCanvas));

	OnChanged().Broadcast();
}

bool FGMECanvasListViewModel::Tick(const float InDeltaSeconds)
{
	if (const UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		const TArray<FName> CanvasNames = Subsystem->GetCanvasNames();
		if (LastCanvasNames != CanvasNames)
		{
			// Canvas Names changed
			LastCanvasNames = CanvasNames;
			RefreshCanvases();
			OnChanged().Broadcast();
		}
	}

	// Always loop
	return true;
}

bool FGMECanvasListViewModel::GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren)
{
	const TArray<TSharedPtr<FGMECanvasItemViewModel>> Children = CanvasItems;
	OutChildren.Append(Children);
	return Children.Num() > 0;
}
