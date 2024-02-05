// Copyright Epic Games, Inc. All Rights Reserved.

#include "ValidationUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Commandlets/Commandlet.h"
#include "Components/SkeletalMeshComponent.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"

void PrepareAssetRegistry()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
	if (AssetRegistryModule.Get().IsSearchAllAssets())
	{
		UE_LOG(LogMutable,Display,TEXT("Asset search already in progress. Waitting for it to complete."));
		AssetRegistryModule.Get().WaitForCompletion();
	}
	else
	{
		UE_LOG(LogMutable,Display,TEXT("Searching all assets (this will take some time)..."));
		AssetRegistryModule.Get().SearchAllAssets(true /* bSynchronousSearch */);
	}

	UE_LOG(LogMutable,Display,TEXT("Asset searching completed!"));
}


bool CompileCustomizableObject(UCustomizableObject* InCustomizableObject, const FCompilationOptions* InCompilationOptionsOverride  /* nullptr */)
{
	if (!InCustomizableObject)
	{
		UE_LOG(LogMutable, Error, TEXT("Unable to compile CO : CO pointer is null!"));
		return false;
	}
	
	// Override the compilation options if an override has been provided by the user
	FCompilationOptions CompilationOptions;
	const bool bOverrideCompilationOptions = InCompilationOptionsOverride != nullptr;
	if (bOverrideCompilationOptions)
	{
		CompilationOptions = *InCompilationOptionsOverride;
		UE_LOG(LogMutable,Display,TEXT("CO Compilation options overriden by the user defined ones."));
	}
	else
	{
		CompilationOptions = InCustomizableObject->CompileOptions;
		UE_LOG(LogMutable,Display,TEXT("Compiling CO using it's own compilation options."));
	}

	UE_LOG(LogMutable, Log, TEXT("(string) model_compile_options_overriden : %s "), bOverrideCompilationOptions ? TEXT("true") : TEXT("false"));
	
	// TODO: Add logs for the other relevant configs of the model being compiled
	// Print MTU parseable logs
	UE_LOG(LogMutable, Log, TEXT("(int) model_optimization_level : %d "), CompilationOptions.OptimizationLevel);
	UE_LOG(LogMutable, Log, TEXT("(string) model_texture_compression : %s "), *UEnum::GetValueAsString(CompilationOptions.TextureCompression));
	UE_LOG(LogMutable, Log, TEXT("(string) model_disk_compilation : %s "), CompilationOptions.bUseDiskCompilation ? TEXT("true") : TEXT("false"));

	// Request a compiler to be able to locate the root and to compile it
	const TUniquePtr<FCustomizableObjectCompilerBase> Compiler =
		TUniquePtr<FCustomizableObjectCompilerBase>(UCustomizableObjectSystem::GetInstanceChecked()->GetNewCompiler());
	
    // Compile the CO with the selected compilation options
    // Run Sync compilation -> Warning : Potentially long operation -------------
	UE_LOG(LogMutable,Display,TEXT("Compiling Customizable Object..."));
	const double CompilationStartSeconds = FPlatformTime::Seconds();
	{
		Compiler->Compile(*InCustomizableObject, CompilationOptions, false);
	}
	const double CompilationEndSeconds = FPlatformTime::Seconds() - CompilationStartSeconds;
    // --------------------------------------------------------------------------
	UE_LOG(LogMutable,Display,TEXT("Compilation of CO completed!"));

	UE_LOG(LogMutable, Log, TEXT("(double) model_compile_time_ms : %f "), CompilationEndSeconds * 1000);
	UE_LOG(LogMutable, Display, TEXT("The compilation of the %s model took %f seconds."), *InCustomizableObject->GetName(), CompilationEndSeconds);

	
    // Get the compilation result
    const ECustomizableObjectCompilationState CompilationEndResult = Compiler->GetCompilationState();
	UE_LOG(LogMutable, Log, TEXT("(string) model_compile_end_state : %s "), *UEnum::GetValueAsString(CompilationEndResult));
	
    const bool bWasCoCompilationSuccessful = CompilationEndResult == ECustomizableObjectCompilationState::Completed;
    if (bWasCoCompilationSuccessful)
    {
    	UE_LOG(LogMutable, Display, TEXT("The compilation of the %s model was succesfull."), *InCustomizableObject->GetName());
    }
    else
    {
    	UE_LOG(LogMutable, Error, TEXT("The compilation of the %s model failed."), *InCustomizableObject->GetName());
    }
    
	return bWasCoCompilationSuccessful;
}



bool UCOIUpdater::UpdateInstance(UCustomizableObjectInstance* InInstance)
{
	check (InInstance);
	check (ComponentsBeingUpdated.IsEmpty());
	
	// Cache the instance being updated for reference once in the update end callback
	Instance = InInstance;
	
	// Schedule the update of the COI
	{
		UE_LOG(LogMutable,Display,TEXT("Invoking update for %s instance."),*InInstance->GetName());

		// Instance update delegate	
		FInstanceUpdateDelegate InstanceUpdateDelegate;
		InstanceUpdateDelegate.BindDynamic(this, &UCOIUpdater::OnInstanceUpdateResult);

		bIsInstanceBeingUpdated = true;
		InInstance->UpdateSkeletalMeshAsyncResult(InstanceUpdateDelegate,true, true);
	}
	
	
	// Wait until the update has been completed and the mips streamed
	while (bIsInstanceBeingUpdated)
	{
		// Tick the engine
		CommandletHelpers::TickEngine();

		// Stop if exit was requested
		if (IsEngineExitRequested())
		{
			break;
		}

		// Wait until all MIPs gets streamed 
		if (!ComponentsBeingUpdated.IsEmpty())
		{
			bool bFullyStreamed = true;
			for (auto It = ComponentsBeingUpdated.CreateIterator(); It && bFullyStreamed; ++It)
			{
				TObjectPtr<USkeletalMeshComponent>& ComponentBeingUpdated = *It;
            		
				FStreamingTextureLevelContext LevelContext(EMaterialQualityLevel::Num, ComponentBeingUpdated);
				TArray<FStreamingRenderAssetPrimitiveInfo> RenderAssetInfoArray;
				ComponentBeingUpdated->GetStreamingRenderAssetInfo(LevelContext, RenderAssetInfoArray);

				for (auto ItAsset = RenderAssetInfoArray.CreateIterator(); ItAsset && bFullyStreamed; ++ItAsset)
				{
					bFullyStreamed = ItAsset->RenderAsset->IsFullyStreamedIn();
				}
			}

			if (bFullyStreamed)
			{
				UE_LOG(LogMutable,Display,TEXT("Instance %s finished streaming all MIPs."), *InInstance->GetName());
				ComponentsBeingUpdated.Reset();
				
				bIsInstanceBeingUpdated = false;		// Exit the while loop
			}
		}
	}
	
	// Return true if the update was successful and false if not
	return !bInstanceFailedUpdate;
}


void UCOIUpdater::OnInstanceUpdateResult(const FUpdateContext& Result)
{
	const FString InstanceName = Instance->GetName();
	
	if (UCustomizableObjectSystem::IsUpdateResultValid(Result.UpdateResult))
	{
		UE_LOG(LogMutable,Display,TEXT("Instance %s finished update succesfully."),*InstanceName);
		bInstanceFailedUpdate = false;
		
		// Request load all MIPs
		UE_LOG(LogMutable,Display,TEXT("Instance %s rquesting streaming all MIPs."), *InstanceName);

		check(ComponentsBeingUpdated.IsEmpty());
		for (int32 Index = 0; Index < Instance->GetNumComponents(); ++Index)
		{
			USkeletalMeshComponent* SkeletalComponent = NewObject<USkeletalMeshComponent>();
			SkeletalComponent->SetSkeletalMesh(Instance->GetSkeletalMesh(Index));
			
			ComponentsBeingUpdated.Add(SkeletalComponent);            			
		}

		// Request the streaming in of all the components affected by the update
		for (TObjectPtr<USkeletalMeshComponent>& ComponentBeingUpdated : ComponentsBeingUpdated)
		{
			FStreamingTextureLevelContext LevelContext(EMaterialQualityLevel::Num, ComponentBeingUpdated);
			TArray<FStreamingRenderAssetPrimitiveInfo> RenderAssetInfoArray;
			ComponentBeingUpdated->GetStreamingRenderAssetInfo(LevelContext, RenderAssetInfoArray);

			for (const FStreamingRenderAssetPrimitiveInfo& Info : RenderAssetInfoArray)
			{
				Info.RenderAsset->StreamIn(MAX_int32, true);
			}
		}
	}
	else
	{
		const FString OutputStatus = UEnum::GetValueAsString(Result.UpdateResult);
		UE_LOG(LogMutable,Error,TEXT("Instance %s finished update with anomalous state : %s."), *InstanceName, *OutputStatus);
		bInstanceFailedUpdate = true;
		
		// Tell the system the instance finished it's update so we can continue the execution without waitting for the mips to stream in
		bIsInstanceBeingUpdated = false;
	}	
}