// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementActorViewportProcessors.h"

#include "Components/PrimitiveComponent.h"
#include "Elements/Columns/TypedElementViewportColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "HAL/IConsoleManager.h"
#include "MassActorSubsystem.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

FAutoConsoleCommandWithArgsAndOutputDevice SetOutlineColorConsoleCommand(
	TEXT("TEDS.Debug.SetOutlineColor"),
	TEXT("Adds an outline color to selected objects."),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice& Output)
		{
			using namespace TypedElementQueryBuilder;
			using DSI = ITypedElementDataStorageInterface;

			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.AddOverlayColorToSelectionCommand);

			if (ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage())
			{
				static TypedElementQueryHandle OverlayQuery = TypedElementInvalidQueryHandle;
				if (OverlayQuery == TypedElementInvalidQueryHandle)
				{
					OverlayQuery = DataStorage->RegisterQuery(
						Select()
						.Where()
							.All<FTypedElementSelectionColumn>()
						.Compile());
				}
				
				if (OverlayQuery == TypedElementInvalidQueryHandle)
				{
					return;
				}

				if (Args.IsEmpty())
				{
					Output.Log(TEXT("Provide a color index (0-7) to use as outline"));
					return;
				}
				
				// Parse the color
				int32 ColorIndex;
				LexFromString(ColorIndex, *Args[0]);

				if (!(ColorIndex >= 0 && ColorIndex <= 7))
				{
					Output.Log(TEXT("Color index must be in range [0,7]"));
					return;
				}

				TArray<TypedElementRowHandle> RowHandles;
				
				DataStorage->RunQuery(OverlayQuery, [ColorIndex, &RowHandles](const DSI::FQueryDescription&, DSI::IDirectQueryContext& Context)
				{
					RowHandles = Context.GetRowHandles();
				});

				for (TypedElementRowHandle Row : RowHandles)
				{
					DataStorage->AddOrGetColumn<FTypedElementViewportOutlineColorColumn>(Row)->SelectionOutlineColorIndex = ColorIndex;
					DataStorage->AddColumn<FTypedElementSyncBackToWorldTag>(Row);
				}
			}
		}));

FAutoConsoleCommandWithArgsAndOutputDevice SetSelectionOverlayColorConsoleCommand(
	TEXT("TEDS.Debug.SetOverlayColor"),
	TEXT("Adds an overlay color to selected objects."),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice& Output)
		{
			using namespace TypedElementQueryBuilder;
			using DSI = ITypedElementDataStorageInterface;

			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.AddOverlayColorToSelectionCommand);

			if (ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage())
			{
				static TypedElementQueryHandle OverlayQuery = TypedElementInvalidQueryHandle;
				if (OverlayQuery == TypedElementInvalidQueryHandle)
				{
					OverlayQuery = DataStorage->RegisterQuery(
						Select()
						.Where()
							.All<FTypedElementSelectionColumn>()
						.Compile());
				}
				
				if (OverlayQuery == TypedElementInvalidQueryHandle)
				{
					return;
				}

				if (Args.IsEmpty())
				{
					Output.Log(TEXT("Provide a color in hexadecimal format (#RRGGBBAA) to overlay."));
					return;
				}
				
				// Parse the color
				FColor Color = FColor::FromHex(Args[0]);
				Color.A = FMath::Clamp(Color.A, 0, 128);

				TArray<TypedElementRowHandle> RowHandles;
				
				DataStorage->RunQuery(OverlayQuery, [Color, &RowHandles](const DSI::FQueryDescription&, DSI::IDirectQueryContext& Context)
				{
					RowHandles = Context.GetRowHandles();
				});

				for (TypedElementRowHandle Row : RowHandles)
				{
					DataStorage->AddOrGetColumn<FTypedElementViewportOverlayColorColumn>(Row)->OverlayColor = Color;
					DataStorage->AddColumn<FTypedElementSyncBackToWorldTag>(Row);
				}
			}
		}));

void UTypedElementActorViewportFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	RegisterOutlineColorColumnToActor(DataStorage);
	RegisterOverlayColorColumnToActor(DataStorage);
}

void UTypedElementActorViewportFactory::RegisterOutlineColorColumnToActor(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync viewport outline color column to actor"),
			FProcessor(DSI::EQueryTickPhase::DuringPhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncDataStorageToExternal))
				.ForceToGameThread(true),
			[](FMassActorFragment& Actor, const FTypedElementViewportOutlineColorColumn& ViewportColor)
			{
				if (AActor* ActorInstance = Actor.GetMutable(); ActorInstance != nullptr)
				{
					if (UPrimitiveComponent* PrimitiveComponent = ActorInstance->GetComponentByClass<UPrimitiveComponent>())
					{
						PrimitiveComponent->SetSelectionOutlineColorIndex(ViewportColor.SelectionOutlineColorIndex);
					}
				}
			}
		)
		.Where()
			.All<FTypedElementSyncBackToWorldTag>()
		.Compile()
	);
}

void UTypedElementActorViewportFactory::RegisterOverlayColorColumnToActor(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync viewport overlay color column to actor"),
			FProcessor(DSI::EQueryTickPhase::DuringPhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncDataStorageToExternal))
				.ForceToGameThread(true),
			[](FMassActorFragment& Actor, const FTypedElementViewportOverlayColorColumn& ViewportColor)
			{
				if (AActor* ActorInstance = Actor.GetMutable(); ActorInstance != nullptr)
				{
					if (UPrimitiveComponent* PrimitiveComponent = ActorInstance->GetComponentByClass<UPrimitiveComponent>())
					{
						PrimitiveComponent->SetOverlayColor(ViewportColor.OverlayColor);
					}
				}
			}
		)
		.Where()
			.All<FTypedElementSyncBackToWorldTag>()
		.Compile()
	);
}
