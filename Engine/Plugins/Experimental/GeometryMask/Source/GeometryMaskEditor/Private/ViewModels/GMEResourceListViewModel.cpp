// Copyright Epic Games, Inc. All Rights Reserved.

#include "GMEResourceListViewModel.h"

#include "Engine/Engine.h"
#include "GeometryMaskSubsystem.h"
#include "GMEResourceItemViewModel.h"

TSharedRef<FGMEResourceListViewModel> FGMEResourceListViewModel::Create()
{
	TSharedRef<FGMEResourceListViewModel> ViewModel = MakeShared<FGMEResourceListViewModel>(FPrivateToken{});
	ViewModel->Initialize();

	return ViewModel;
}

FGMEResourceListViewModel::~FGMEResourceListViewModel()
{
	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		Subsystem->OnGeometryMaskCanvasCreated().Remove(OnResourceCreatedHandle);
	}
	
	ResourceItems.Reset();
}

void FGMEResourceListViewModel::Initialize()
{
	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		// Add currently utilized canvas resources
		ResourceItems.Reserve(4);
		
		for (const TObjectPtr<UGeometryMaskCanvasResource>& Resource : Subsystem->GetCanvasResources())
		{
			ResourceItems.Add(FGMEResourceItemViewModel::Create(Resource));
		}

		// And listen for new ones
		{
			OnResourceCreatedHandle = Subsystem->OnGeometryMaskResourceCreated().AddRaw(this, &FGMEResourceListViewModel::OnResourceCreated);
		}

		OnChanged().Broadcast();
	}
}

void FGMEResourceListViewModel::OnResourceCreated(const UGeometryMaskCanvasResource* InGeometryMaskResource)
{
	// Don't add if already in list
	if (ResourceItems.ContainsByPredicate([Id = InGeometryMaskResource->GetUniqueID()](const TSharedPtr<FGMEResourceItemViewModel>& InItemViewModel)
	{
		return InItemViewModel->GetId() == Id;
	}))
	{
		return;
	}

	ResourceItems.Add(FGMEResourceItemViewModel::Create(InGeometryMaskResource));

	OnChanged().Broadcast();
}

bool FGMEResourceListViewModel::GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren)
{
	const TArray<TSharedPtr<FGMEResourceItemViewModel>> Children = ResourceItems;
	OutChildren.Append(Children);
	return Children.Num() > 0;
}
