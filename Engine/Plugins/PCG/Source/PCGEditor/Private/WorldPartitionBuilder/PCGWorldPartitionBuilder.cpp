// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionBuilder/PCGWorldPartitionBuilder.h"

#include "PCGBuilderSettings.h"
#include "PCGComponent.h"
#include "PCGEditorSettings.h"
#include "PCGGraph.h"
#include "SPCGBuilderDialog.h"

#include "AssetCompilingManager.h"
#include "Editor.h"
#include "FileHelpers.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/OutputDevice.h"
#include "Modules/ModuleManager.h"
#include "UObject/Linker.h"
#include "WorldPartition/IWorldPartitionEditorModule.h"
#include "WorldPartition/WorldPartitionHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPCGWorldPartitionBuilder, All, All);

#define LOCTEXT_NAMESPACE "PCGWorldPartitionBulder"

namespace PCGWorldPartitionBuilder
{
	/** Grab all original PCG components in world, applying given filter. */
	void CollectComponentsToGenerate(UWorld* InWorld, TFunctionRef<bool(const UPCGComponent*)> ComponentFilter, TArray<TWeakObjectPtr<UPCGComponent>>& OutComponents);
	
	/** Generate the given components. Optionally generate only one component at a time with a wait on async processes after each. Optionally apply given filter to select components. */
	bool GenerateComponents(TArray<TWeakObjectPtr<UPCGComponent>>& Components, UWorld* InWorld, bool bOneComponentAtATime, TArray<TObjectPtr<UPackage>>& InOutDeletedActorPackages, bool& bOutGenerationErrors);
	bool GenerateComponents(
		TArray<TWeakObjectPtr<UPCGComponent>>& Components,
		UWorld* InWorld,
		bool bOneComponentAtATime,
		TFunctionRef<bool(const UPCGComponent*)> ComponentFilter,
		TArray<TObjectPtr<UPackage>>& InOutDeletedActorPackages,
		bool& bOutGenerationErrors);

	/** Generate a component. Applies correct editing mode if necessary. */
	void GenerateComponent(UPCGComponent* InComponent, UWorld* InWorld);

	/** Waits for background activity to be quiet. */
	void WaitForAllAsyncEditorProcesses(UWorld* InWorld);

	/** Runs builder on current editor world. */
	bool Build(UWorld* InWorld, const TArray<FString>& Args);

	static FAutoConsoleCommand CommandBuildComponents(
		TEXT("pcg.BuildComponents"),
		TEXT("Runs PCG world builder on PCG components in current world. Arguments (multiple values separated with ';'), Flags can optionally have a specified '=true or false', true if none provided:\n"
			"\t[-PCGBuilderSettings=/Game/Path/BuilderSettingsAssetName]\n"
			"\t[-IncludeGraphNames=PCG_GraphA;PCG_GraphB] (default: all graphs)\n"
			"\t[-GenerateComponentEditingModeLoadAsPreview] (default: true)\n"
			"\t[-GenerateComponentEditingModeNormal] (default: false)\n"
			"\t[-GenerateComponentEditingModePreview] (default: false)\n"
			"\t[-IgnoreGenerationErrors] (default: false)\n"
			"\t[-IncludeActorIDs=MyActor1_UAID1234678;MyActor2_UAID1234678] (default: all actors)\n"
			"\t[-OneComponentAtATime] (default: false)\n"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { PCGWorldPartitionBuilder::Build(GEditor ? GEditor->GetEditorWorldContext().World() : nullptr, Args); }));
};

/** Output device to capture presence of errors during generation. */
struct FPCGDetectErrorsInScope : public FOutputDevice
{
	FPCGDetectErrorsInScope()
	{
		if (GLog)
		{
			GLog->AddOutputDevice(this);
		}
	}

	virtual ~FPCGDetectErrorsInScope()
	{
		if (GLog)
		{
			GLog->RemoveOutputDevice(this);
		}
	}

	//~Begin FOutputDevice interface
	virtual bool IsMemoryOnly() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
	{
		if (Verbosity <= ELogVerbosity::Error)
		{
			bErrorOccurred = true;
		}
	}
	//~End FOutputDevice interface

	bool GetErrorOccurred() const { return bErrorOccurred; }

private:
	std::atomic<bool> bErrorOccurred = false;
};

FPCGWorldPartitionBuilderArgs FPCGWorldPartitionBuilderArgs::InitializeFrom(const FPCGWorldPartitionCommandlineArgs& CommandlineArgs)
{
	FPCGWorldPartitionBuilderArgs BuilderArgs;
	TSoftObjectPtr<UPCGBuilderSettings> SettingsPath = CommandlineArgs.SettingAsset.IsSet() ? CommandlineArgs.SettingAsset.GetValue() : GetDefault<UPCGEditorProjectSettings>()->DefaultBuilderSetting;
	
	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Initialize Builder Args"));

	bool bUsingBuilderSettings = false;
	// Load Settings Asset if any exists
	if (SettingsPath.IsValid() || SettingsPath.IsPending())
	{
		if (UPCGBuilderSettings* BuilderSettings = Cast<UPCGBuilderSettings>(SettingsPath.LoadSynchronous()))
		{
			BuilderArgs.bGenerateEditingModeLoadAsPreviewComponents = BuilderSettings->EditingModes.Contains(EPCGEditorDirtyMode::LoadAsPreview);
			BuilderArgs.bGenerateEditingModeNormalComponents = BuilderSettings->EditingModes.Contains(EPCGEditorDirtyMode::Normal);
			BuilderArgs.bGenerateEditingModePreviewComponents = BuilderSettings->EditingModes.Contains(EPCGEditorDirtyMode::Preview);
			BuilderArgs.bIgnoreGenerationErrors = BuilderSettings->bIgnoreGenerationErrors;
			BuilderArgs.bOneComponentAtATime = BuilderSettings->bOneComponentAtATime;
			BuilderArgs.IncludeActorIDs = BuilderSettings->FilterByActorNames;
			BuilderArgs.IncludeGraphs = BuilderSettings->Graphs;

			UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Loaded PCGBuilderSettings '%s'"), *SettingsPath.ToString());
			bUsingBuilderSettings = true;
		}
		else
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("Failed to load PCGBuilderSettings '%s'"), *SettingsPath.ToString());
		}
	}
	
	if (!bUsingBuilderSettings)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Not using any PCGBuilderSettings"));
	}

	// Then apply Commandline since it has priority
	const TCHAR* FromCommandline = TEXT("Commandline");
	const TCHAR* FromDefault = bUsingBuilderSettings ? TEXT("PCGBuilderSettings") : TEXT("Default");
		
	// Init 'bGenerateEditingModeLoadAsPreviewComponents'
	bool bFromCommandline = CommandlineArgs.bGenerateEditingModeLoadAsPreviewComponents.IsSet();
	if (bFromCommandline)
	{
		BuilderArgs.bGenerateEditingModeLoadAsPreviewComponents = CommandlineArgs.bGenerateEditingModeLoadAsPreviewComponents.GetValue();
	}

	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("bGenerateEditingModeLoadAsPreviewComponents : '%d' (%s)"), 
		BuilderArgs.bGenerateEditingModeLoadAsPreviewComponents ? 1 : 0,
		bFromCommandline ? FromCommandline : FromDefault);
	
	// Init 'bGenerateEditingModeNormalComponents'
	bFromCommandline = CommandlineArgs.bGenerateEditingModeNormalComponents.IsSet();
	if (bFromCommandline)
	{
		BuilderArgs.bGenerateEditingModeNormalComponents = CommandlineArgs.bGenerateEditingModeNormalComponents.GetValue();
	}

	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("bGenerateEditingModeNormalComponents : '%d' (%s)"),
		BuilderArgs.bGenerateEditingModeNormalComponents ? 1 : 0,
		bFromCommandline ? FromCommandline : FromDefault);
	
	// Init 'bGenerateEditingModePreviewComponents'
	bFromCommandline = CommandlineArgs.bGenerateEditingModePreviewComponents.IsSet();
	if (bFromCommandline)
	{
		BuilderArgs.bGenerateEditingModePreviewComponents = CommandlineArgs.bGenerateEditingModePreviewComponents.GetValue();
	}

	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("bGenerateEditingModePreviewComponents : '%d' (%s)"),
		BuilderArgs.bGenerateEditingModePreviewComponents ? 1 : 0,
		bFromCommandline ? FromCommandline : FromDefault);


	// Init 'bOneComponentAtATime'
	bFromCommandline = CommandlineArgs.bOneComponentAtATime.IsSet();
	if (bFromCommandline)
	{
		BuilderArgs.bOneComponentAtATime = CommandlineArgs.bOneComponentAtATime.GetValue();
	}

	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("bOneComponentAtATime : '%d' (%s)"),
		BuilderArgs.bOneComponentAtATime ? 1 : 0,
		bFromCommandline ? FromCommandline : FromDefault);
	
	// Init 'bIgnoreGenerationErrors'
	bFromCommandline = CommandlineArgs.bIgnoreGenerationErrors.IsSet();
	if (bFromCommandline)
	{
		BuilderArgs.bIgnoreGenerationErrors = CommandlineArgs.bIgnoreGenerationErrors.GetValue();
	}

	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("bIgnoreGenerationErrors : '%d' (%s)"),
		BuilderArgs.bIgnoreGenerationErrors ? 1 : 0,
		bFromCommandline ? FromCommandline : FromDefault);
		
	// Init 'IncludeActorIDs'
	bFromCommandline = CommandlineArgs.IncludeActorIDs.IsSet();
	if (bFromCommandline)
	{
		BuilderArgs.IncludeActorIDs = CommandlineArgs.IncludeActorIDs.GetValue();
	}

	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("IncludeActorIDs : '%s' (%s)"),
		BuilderArgs.IncludeActorIDs.IsEmpty() ? TEXT("None") : *FString::Join(BuilderArgs.IncludeActorIDs, TEXT(",")),
		bFromCommandline ? FromCommandline : FromDefault);
		
	// Init 'IncludeGraphNames'
	bFromCommandline = CommandlineArgs.IncludeGraphNames.IsSet();
	if (bFromCommandline)
	{
		// Commandline overrides using graph names so we empty whatever graph specified in the settings (it one or the other)
		BuilderArgs.IncludeGraphs.Empty();
		BuilderArgs.IncludeGraphNames = CommandlineArgs.IncludeGraphNames.GetValue();
	}

	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("IncludeGraphNames : '%s' (%s)"),
		BuilderArgs.IncludeGraphNames.IsEmpty() ? TEXT("None") : *FString::Join(BuilderArgs.IncludeGraphNames, TEXT(",")),
		bFromCommandline ? FromCommandline : TEXT("Default"));

	TArray<FString> GraphsToString;
	Algo::Transform(BuilderArgs.IncludeGraphs, GraphsToString, [](const TSoftObjectPtr<UPCGGraphInterface>& Graph) { return Graph.ToString(); });
		
	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("IncludeGraphs '%s' (%s)"),
		GraphsToString.IsEmpty() ? TEXT("None") : *FString::Join(GraphsToString, TEXT(",")),
		FromDefault);

	return BuilderArgs;
}

UPCGWorldPartitionBuilder::UPCGWorldPartitionBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		auto GetBoolParam = [this](const FString& ParamName, TOptional<bool>& OutOptionalParam)
		{
			const FString ParamNameAssignment = ParamName + "=";
			bool bParamValue;
			if (HasParam(ParamName))
			{
				OutOptionalParam = true;
			}
			else if (FParse::Bool(*GetBuilderArgs(), *ParamNameAssignment, bParamValue))
			{
				OutOptionalParam = bParamValue;
			}
		};

		GetBoolParam("GenerateComponentEditingModeLoadAsPreview", CommandlineArgs.bGenerateEditingModeLoadAsPreviewComponents);
		GetBoolParam("GenerateComponentEditingModeNormal", CommandlineArgs.bGenerateEditingModeNormalComponents);
		GetBoolParam("GenerateComponentEditingModePreview", CommandlineArgs.bGenerateEditingModePreviewComponents);
		GetBoolParam("OneComponentAtATime", CommandlineArgs.bOneComponentAtATime);
		GetBoolParam("IgnoreGenerationErrors", CommandlineArgs.bIgnoreGenerationErrors);
	
		FString IncludeGraphNamesValue;
		if (GetParamValue("IncludeGraphNames=", IncludeGraphNamesValue))
		{
			TArray<FString> IncludeGraphNameStrings;
			if (!IncludeGraphNamesValue.IsEmpty())
			{
				IncludeGraphNamesValue.ParseIntoArray(IncludeGraphNameStrings, TEXT(";"), true);
			}
			CommandlineArgs.IncludeGraphNames = IncludeGraphNameStrings;
		}

		FString IncludeActorIDsValue;
		if (GetParamValue("IncludeActorIDs=", IncludeActorIDsValue))
		{
			TArray<FString> IncludeActorIDStrings;
			if (!IncludeActorIDsValue.IsEmpty())
			{
				IncludeActorIDsValue.ParseIntoArray(IncludeActorIDStrings, TEXT(";"), true);
			}
			CommandlineArgs.IncludeActorIDs = IncludeActorIDStrings;
		}

		FString SettingAsset;
		if (GetParamValue("PCGBuilderSettings=", SettingAsset) && !SettingAsset.IsEmpty())
		{
			CommandlineArgs.SettingAsset = TSoftObjectPtr<UPCGBuilderSettings>(FSoftObjectPath(SettingAsset));
		}
	}
}

bool UPCGWorldPartitionBuilder::PreRun(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{
	if (!Super::PreRun(World, PackageHelper))
	{
		return false;
	}

	Args = FPCGWorldPartitionBuilderArgs::InitializeFrom(CommandlineArgs);
		
	return true;
}

bool UPCGWorldPartitionBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	if (!ensure(GEngine) || !ensure(World))
	{
		return false;
	}

	// Try to eliminate as much background noise as possible before getting started.
	PCGWorldPartitionBuilder::WaitForAllAsyncEditorProcesses(World);

	// 1. Collect components of interest once upfront.

	TArray<TWeakObjectPtr<UPCGComponent>> ComponentsToGenerate;
	{
		auto ComponentFilter = [this](const UPCGComponent* InComponent)
		{
			if (!InComponent || !InComponent->GetOwner())
			{
				return false;
			}

			// Check actor in inclusion list if provided.
			if (!Args.IncludeActorIDs.IsEmpty() && !Args.IncludeActorIDs.Contains(InComponent->GetOwner()->GetName()))
			{
				return false;
			}

			// Accept based on editing mode.
			return (Args.bGenerateEditingModeLoadAsPreviewComponents && InComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::LoadAsPreview)
				|| (Args.bGenerateEditingModeNormalComponents && InComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::Normal)
				|| (Args.bGenerateEditingModePreviewComponents && InComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::Preview);
		};

		PCGWorldPartitionBuilder::CollectComponentsToGenerate(World, ComponentFilter, ComponentsToGenerate);
	}

	// 2. Clear the dirty flag on any packages that dirtied themselves while loading. We're not interested in saving these.

	{
		TArray<UPackage*> DirtyPackages;
		FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);
		DirtyPackages.RemoveSwap(nullptr);
		for (UPackage* DirtyPackage : DirtyPackages)
		{
			// Make clean as it's not a change we have made.
			if (!PendingDirtyPackages.Contains(DirtyPackage->GetPersistentGuid()))
			{
				DirtyPackage->SetDirtyFlag(false);
			}
		}
		DirtyPackages.Reset();
	}

	const int32 NumPendingDirtyPackagesBefore = PendingDirtyPackages.Num();

	// 3. Generate components.

	bool bGeneratedAnyComponent = false;

	if (!Args.IncludeGraphNames.IsEmpty())
	{
		// Generate all components with graph of each name, in specified order.
		for (const FString& IncludeGraphName : Args.IncludeGraphNames)
		{
			auto FilterOnGraphName = [IncludeGraphName](const UPCGComponent* InComponent)
			{
				const UPCGGraph* Graph = InComponent ? InComponent->GetGraph() : nullptr;
				return Graph && Graph->GetName() == IncludeGraphName;
			};

			bool bErrorsOccurred = false;
			bGeneratedAnyComponent |= PCGWorldPartitionBuilder::GenerateComponents(ComponentsToGenerate, World, Args.bOneComponentAtATime, FilterOnGraphName, DeletedActorPackages, bErrorsOccurred);

			bErrorOccurredWhileGenerating |= bErrorsOccurred;
		}
	}
	else if (!Args.IncludeGraphs.IsEmpty())
	{
		// Generate all components with graph of each name, in specified order.
		for (const TSoftObjectPtr<UPCGGraphInterface>& IncludeGraphPtr : Args.IncludeGraphs)
		{
			auto FilterOnGraphPtr = [&IncludeGraphPtr](const UPCGComponent* InComponent)
				{
					TSoftObjectPtr<UPCGGraphInterface> ComponentGraphPtr(InComponent ? InComponent->GetGraph() : nullptr);
					return ComponentGraphPtr.IsValid() && ComponentGraphPtr == IncludeGraphPtr;
				};

			bool bErrorsOccurred = false;
			bGeneratedAnyComponent |= PCGWorldPartitionBuilder::GenerateComponents(ComponentsToGenerate, World, Args.bOneComponentAtATime, FilterOnGraphPtr, DeletedActorPackages, bErrorsOccurred);

			bErrorOccurredWhileGenerating |= bErrorsOccurred;
		}
	}
	else
	{
		bool bErrorsOccurred = false;
		bGeneratedAnyComponent |= PCGWorldPartitionBuilder::GenerateComponents(ComponentsToGenerate, World, Args.bOneComponentAtATime, DeletedActorPackages, bErrorsOccurred);

		bErrorOccurredWhileGenerating |= bErrorsOccurred;
	}

	// 4. Get packages that were dirtied during generation and record them for saving later.

	if (bGeneratedAnyComponent)
	{
		TArray<UPackage*> DirtyPackages;
		FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);
		
		DirtyPackages.RemoveSwap(nullptr);
		for (UPackage* DirtyPackage : DirtyPackages)
		{
			PendingDirtyPackages.Add(DirtyPackage->GetPersistentGuid(), DirtyPackage);
		}
	}
	
	const int NumPackagesDirtied = PendingDirtyPackages.Num() - NumPendingDirtyPackagesBefore;
	if (!bGeneratedAnyComponent)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("No components found to generate. No packages will be saved."));
	}
	else if (NumPackagesDirtied <= 0)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("At least one component was generated but no additional packages were dirtied."));
	}
	else
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Generation complete, %d packages dirtied."), NumPackagesDirtied);
	}

	if (!bGeneratedAnyComponent)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Dirty package detection and save skipped due to trivial run"));
		return !bErrorOccurredWhileGenerating;
	}

	// TODO: Review the save flow when we have iterative loading.
	return SaveDirtyPackages(World, PackageHelper);
}

bool UPCGWorldPartitionBuilder::SaveDirtyPackages(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{
	// Check whether an error was thrown while generating components and fail the builder if the ignore argument is not provided.
	if (bErrorOccurredWhileGenerating)
	{
		if (!Args.bIgnoreGenerationErrors)
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Dirty package detection and save skipped due to errors during generation."));

			return !bErrorOccurredWhileGenerating;
		}
		else
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Generation errors ignored, dirty packages will be saved."));
		}
	}

	// Save/delete pending packages.

	TArray<TObjectPtr<UPackage>> DirtyPackages;
	PendingDirtyPackages.GenerateValueArray(DirtyPackages);

	// Empty packages should be deleted - mirrors logic in InternalPromptForCheckoutAndSave()
	TArray<UPackage*> PackagesToDelete;
	for (int i = DirtyPackages.Num() - 1; i >= 0; --i)
	{
		if (UPackage::IsEmptyPackage(ToRawPtr(DirtyPackages[i])))
		{
			PackagesToDelete.Add(DirtyPackages[i]);
			DirtyPackages.RemoveAtSwap(i);
		}
	}

	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("PostRun: %d packages modified, %d actor packages deleted, %d empty packages marked for delete."),
		DirtyPackages.Num(),
		DeletedActorPackages.Num(),
		PackagesToDelete.Num());

	// Combine the empty-deleted packages with the actor-deleted packages.
	for (UPackage* DeletedPackage : DeletedActorPackages)
	{
		PackagesToDelete.AddUnique(DeletedPackage);
	}

	// Log final changes after combining packages (there may have been duplicates in the two deleted package lists).
	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("PostRun: Final package changes: %d modified, %d deleted."), DirtyPackages.Num(), PackagesToDelete.Num());

	if (!SavePackages(DirtyPackages, PackageHelper))
	{
		return false;
	}
	
	if (!PackagesToDelete.IsEmpty())
	{
		// Release any file handles and prepare for delete.
		for (UPackage* PackageToDelete : PackagesToDelete)
		{
			ResetLoaders(PackageToDelete);
		}

		if (!DeletePackages(PackagesToDelete, PackageHelper))
		{
			return false;
		}
	}

	TArray<FString> FilesToSubmit;
	FilesToSubmit.Append(SourceControlHelpers::PackageFilenames(DirtyPackages));
	FilesToSubmit.Append(SourceControlHelpers::PackageFilenames(PackagesToDelete));

	const FString ChangeDescription = FString::Printf(TEXT("Generated PCG components for world '%s'"), *World->GetName());
	return OnFilesModified(FilesToSubmit, ChangeDescription);
}

void PCGWorldPartitionBuilder::CollectComponentsToGenerate(
	UWorld* InWorld,
	TFunctionRef<bool(const UPCGComponent*)> ComponentFilter,
	TArray<TWeakObjectPtr<UPCGComponent>>& OutComponents)
{
	check(InWorld);

	TArray<UObject*> AllComponents;
	GetObjectsOfClass(UPCGComponent::StaticClass(), AllComponents, /*bIncludeDerivedClasses=*/true);
	for (UObject* ComponentObject : AllComponents)
	{
		if (!IsValid(ComponentObject))
		{
			continue;
		}

		UPCGComponent* Component = Cast<UPCGComponent>(ComponentObject);
		if (!Component || !Component->GetOwner() || Component->GetWorld() != InWorld)
		{
			continue;
		}

		if (Component->IsLocalComponent())
		{
			// Skip LCs, rely on original component to generate everything.
			continue;
		}

		if (!ComponentFilter(Component))
		{
			continue;
		}

		OutComponents.Add(Component);
	}
}

bool PCGWorldPartitionBuilder::GenerateComponents(TArray<TWeakObjectPtr<UPCGComponent>>& Components, UWorld* InWorld, bool bOneComponentAtATime, TArray<TObjectPtr<UPackage>>& InOutDeletedActorPackages, bool& bOutGenerationErrors)
{
	return PCGWorldPartitionBuilder::GenerateComponents(Components, InWorld, bOneComponentAtATime, [](const UPCGComponent*) { return true; }, InOutDeletedActorPackages, bOutGenerationErrors);
}

bool PCGWorldPartitionBuilder::GenerateComponents(
	TArray<TWeakObjectPtr<UPCGComponent>>& Components,
	UWorld* InWorld,
	bool bOneComponentAtATime,
	TFunctionRef<bool(const UPCGComponent*)> ComponentFilter,
	TArray<TObjectPtr<UPackage>>& InOutDeletedActorPackages,
	bool& bOutGenerationErrors)
{
	if (!bOneComponentAtATime)
	{
		PCGWorldPartitionBuilder::WaitForAllAsyncEditorProcesses(InWorld);
	}

	const FPCGDetectErrorsInScope DetectErrors;

	auto WaitForComponentGeneration = [InWorld](const UPCGComponent* InComponent)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Completing generation on PCG component on actor '%s' label '%s', graph '%s'"),
			*InComponent->GetOwner()->GetName(),
			*InComponent->GetOwner()->GetActorNameOrLabel(),
			*InComponent->GetGraph()->GetName());

		while (InComponent->IsGenerating())
		{
			FWorldPartitionHelpers::FakeEngineTick(InWorld);
		}

		// Can be useful to let some things flush/update after generation.
		FWorldPartitionHelpers::FakeEngineTick(InWorld);
	};

	// Hook actor deleted events and track any corresponding deleted packages, as the packages can be GC'd if PCG triggers a GC
	// before generation.
	FDelegateHandle ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddLambda([&InOutDeletedActorPackages](AActor* InActor)
	{
		if (InActor && InActor->IsPackageExternal())
		{
			if (UPackage* ActorPackage = InActor->GetPackage())
			{
				UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Actor '%s' deleted, package '%s' added to delete list."),
					*InActor->GetName(),
					*ActorPackage->GetName());

				InOutDeletedActorPackages.AddUnique(ActorPackage);
			}
		}
	});

	ON_SCOPE_EXIT
	{
		GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
	};

	TSet<TObjectKey<UPCGComponent>> GeneratedComponents;

	for (TWeakObjectPtr<UPCGComponent> ComponentWeakPtr : Components)
	{
		UPCGComponent* Component = ComponentWeakPtr.Get();
		if (!Component)
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("Lost a PCG component weak pointer, component will not be generated."));
			continue;
		}

		// Validate this before running the filter as the filtering can check the graph name etc.
		const UPCGGraph* Graph = Component->GetGraph();
		if (!Graph)
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("PCG component on actor '%s' label '%s' has no graph assigned, skipping."),
				*Component->GetOwner()->GetName(),
				*Component->GetOwner()->GetActorNameOrLabel());
			continue;
		}

		if (!ComponentFilter(Component))
		{
			continue;
		}
		
		// Last minute validations, done here just prior to generation (after component has passed all previous filters) to minimize spam.
		if (!Component->bActivated)
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("'Activated' toggle was set false on PCG component on actor '%s' label '%s' graph '%s'. Component skipped."),
				*Component->GetOwner()->GetName(),
				*Component->GetOwner()->GetActorNameOrLabel(),
				*Component->GetGraph()->GetName());
			continue;
		}

		if (Component->IsManagedByRuntimeGenSystem())
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("PCG component generation trigger is set to run-time generation on actor '%s' label '%s' graph '%s'. Component skipped."),
				*Component->GetOwner()->GetName(),
				*Component->GetOwner()->GetActorNameOrLabel(),
				*Component->GetGraph()->GetName());
			continue;
		}

		if (bOneComponentAtATime)
		{
			PCGWorldPartitionBuilder::WaitForAllAsyncEditorProcesses(InWorld);
		}

		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Generate PCG component on actor '%s' label '%s', graph '%s'"),
			*Component->GetOwner()->GetName(),
			*Component->GetOwner()->GetActorNameOrLabel(),
			*Component->GetGraph()->GetName());

		PCGWorldPartitionBuilder::GenerateComponent(Component, InWorld);
		GeneratedComponents.Add(Component);

		if (bOneComponentAtATime)
		{
			WaitForComponentGeneration(Component);
		}
	}

	if (!bOneComponentAtATime)
	{
		for (TObjectKey<UPCGComponent>& GeneratedComponent : GeneratedComponents)
		{
			if (const UPCGComponent* Component = GeneratedComponent.ResolveObjectPtr())
			{
				WaitForComponentGeneration(Component);
			}
			else
			{
				UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("GeneratedComponent reference lost, abandoning."));
			}
		}
	}

	bOutGenerationErrors = DetectErrors.GetErrorOccurred();

	return !GeneratedComponents.IsEmpty();
}

void PCGWorldPartitionBuilder::GenerateComponent(UPCGComponent* InComponent, UWorld* InWorld)
{
	// Separate ensures for maximum debug information.
	if (!ensure(InComponent) || !ensure(InComponent->GetGraph()))
	{
		return;
	}

	ensure(InComponent->bActivated);
	ensure(!InComponent->IsManagedByRuntimeGenSystem());

	if (InComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::LoadAsPreview)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Setting PCG editing mode to Load As Preview on actor '%s' label '%s' graph '%s'."),
			*InComponent->GetOwner()->GetName(),
			*InComponent->GetOwner()->GetActorNameOrLabel(),
			*InComponent->GetGraph()->GetName());

		InComponent->SetEditingMode(EPCGEditorDirtyMode::LoadAsPreview, InComponent->GetSerializedEditingMode());
		InComponent->ChangeTransientState(EPCGEditorDirtyMode::LoadAsPreview);
	}

	// Force generate as components that are already generated may decline the request.
	const FPCGTaskId GenerateTask = InComponent->GenerateLocalGetTaskId(/*bForce=*/true);

	if (GenerateTask == InvalidPCGTaskId)
	{
		if (ensure(InComponent->GetOwner()))
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("Scheduling generate task failed for PCG Component on actor '%s' label '%s' graph '%s'."),
				*InComponent->GetOwner()->GetName(),
				*InComponent->GetOwner()->GetActorNameOrLabel(),
				*InComponent->GetGraph()->GetName());
		}
		else
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("Scheduling generate task failed for PCG Component with no owner. Component '%s' graph '%s'."),
				*InComponent->GetName(),
				*InComponent->GetGraph()->GetName());
		}
	}
}

void PCGWorldPartitionBuilder::WaitForAllAsyncEditorProcesses(UWorld* InWorld)
{
	InWorld->BlockTillLevelStreamingCompleted();

	// Quite a lot of activity can happen here..
	FWorldPartitionHelpers::FakeEngineTick(InWorld);

	// Make sure all actor changes are out of the way, as we are sensitive to these.
	bool bActorsStable = false;
	FDelegateHandle ActorAddedHandle = GEngine->OnLevelActorAdded().AddLambda([&bActorsStable](AActor* InActor) { bActorsStable = false; });
	FDelegateHandle ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddLambda([&bActorsStable](AActor* InActor) { bActorsStable = false; });

	while (!bActorsStable)
	{
		bActorsStable = true;
		FWorldPartitionHelpers::FakeEngineTick(InWorld);
	}

	GEngine->OnLevelActorAdded().Remove(ActorAddedHandle);
	GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);

	// Finalize asset compilation before we potentially use them during generation. Example: static mesh collision depends on built SMs.
	// Done before every generation to ensure everything up until now is compiled/built.
	FAssetCompilingManager::Get().FinishAllCompilation();

	// This may execute pending construction scripts.
	FAssetCompilingManager::Get().ProcessAsyncTasks();
}

bool PCGWorldPartitionBuilder::Build(UWorld* InWorld, const TArray<FString>& Args)
{
	if (InWorld)
	{
		IWorldPartitionEditorModule::FRunBuilderParams Params;
		Params.BuilderClass = UPCGWorldPartitionBuilder::StaticClass();
		Params.World = InWorld;
		Params.OperationDescription = FText::FromString("Generating PCG Components...");
		
		Params.ExtraArgs = TEXT("-AllowCommandletRendering -AllowSoftwareRendering -AssetGatherAll=true");
		for (const FString& Arg : Args)
		{
			Params.ExtraArgs += " ";
			Params.ExtraArgs += Arg;
		}

		return IWorldPartitionEditorModule::Get().RunBuilder(Params);
	}

	return false;
}

bool UPCGWorldPartitionBuilder::CanBuild(const UWorld* InWorld, FName InBuildOption)
{
	return InWorld && InWorld->IsPartitionedWorld();
}

EEditorBuildResult UPCGWorldPartitionBuilder::Build(UWorld* InWorld, FName InBuildOption)
{
	TSharedPtr<SWindow> DlgWindow =
		SNew(SWindow)
		.Title(LOCTEXT("BuildPCGWindowTitle", "Build PCG"))
		.ClientSize(SPCGBuilderDialog::DEFAULT_WINDOW_SIZE)
		.SizingRule(ESizingRule::UserSized)
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.SizingRule(ESizingRule::FixedSize);

	TSharedRef<SPCGBuilderDialog> BuildDialog =
		SNew(SPCGBuilderDialog)
		.ParentWindow(DlgWindow);

	DlgWindow->SetContent(BuildDialog);

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	FSlateApplication::Get().AddModalWindow(DlgWindow.ToSharedRef(), MainFrameModule.GetParentWindow());

	if (BuildDialog->GetDialogResult() != SPCGBuilderDialog::DialogResult::Cancel)
	{
		TArray<FString> Args;
		FString BuilderSettingStr = BuildDialog->GetBuilderSetting().ToString();
		if(!BuilderSettingStr.IsEmpty())
		{
			Args.Add("-PCGBuilderSettings=" + BuilderSettingStr);
		}
		return PCGWorldPartitionBuilder::Build(InWorld, Args) ? EEditorBuildResult::Success : EEditorBuildResult::Skipped;
	}

	return EEditorBuildResult::Skipped;
}

#undef LOCTEXT_NAMESPACE
