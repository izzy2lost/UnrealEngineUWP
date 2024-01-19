// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuV/CustomizableObjectValidationCommandlet.h"

#include "Components/SkeletalMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Array.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuR/Model.h"


int32 UCustomizableObjectValidationCommandlet::Main(const FString& Params)
{
	// Execution arguments for commandlet from IDE
	// $(LocalDebuggerCommandArguments) -run=CustomizableObjectValidation -CustomizableObject=(PathToCO)
	
	// Get the package name of the CO to test
	FString CustomizableObjectAssetPath = "";
	if (!FParse::Value(*Params, TEXT("CustomizableObject="), CustomizableObjectAssetPath))
	{
		UE_LOG(LogMutable,Error,TEXT("Failed to parse Customizable Object package name from provided argument : %s"),*Params)
		return 1;
	}
	
	// Get the amount of instances to generate if parameter was provided
	uint32 InstancesToGenerate = 16;
	if (!FParse::Value(*Params, TEXT("InstanceGenerationCount="),InstancesToGenerate))
	{
		UE_LOG(LogMutable,Display,TEXT("Instance generation count not specified. Using default value : %u"),InstancesToGenerate);
	}
	
	// TODO: Detect target compilation platform based on argument value (string)
	
	// Load the resource
	UObject* FoundObject = FSoftObjectPath(CustomizableObjectAssetPath).TryLoad();
	if (!FoundObject)
	{
		UE_LOG(LogMutable,Error,TEXT("Failed to retrieve UObject from path %s"),*CustomizableObjectAssetPath);
		return 1;
	}
	
	// Get the CustomizableObject.
	ToTestCustomizableObject = Cast<UCustomizableObject>(FoundObject);
	if (!ToTestCustomizableObject)
	{
		UE_LOG(LogMutable,Error,TEXT("Failed to cast found UObject to UCustomizableObject."));
		return 1;
	}
	
	// Perform a blocking search to ensure all assets used by mutable are reachable using the AssetRegistry
	{
		UE_LOG(LogMutable,Display,TEXT("Searching all assets (this will take some time)..."));
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		AssetRegistryModule.Get().SearchAllAssets(true /* bSynchronousSearch */);
	}
	
	// Compile the Customizable Object ------------------------------------------------------------------------------ //
	bool bWasCoCompilationSuccessful = false;
	{
		UE_LOG(LogMutable,Display,TEXT("Compiling Customizable Object..."));
    	
    	// Request a compiler to be able to locate the root and to compile it
    	const TUniquePtr<FCustomizableObjectCompilerBase> Compiler =
    		TUniquePtr<FCustomizableObjectCompilerBase>(UCustomizableObjectSystem::GetInstanceChecked()->GetNewCompiler());
		
		// Override some configurations that may have been changed by the user
		FCompilationOptions CompilationOptions = ToTestCustomizableObject->CompileOptions;
		CompilationOptions.bSilentCompilation = false;
		CompilationOptions.OptimizationLevel = 2;			// Set the optimization level to the max
		CompilationOptions.TextureCompression = ECustomizableObjectTextureCompression::Fast;
		
		// TODO: Add logs for the other relevant configs of the model being compiled
		// Print MTU parseable logs
		UE_LOG(LogMutable, Log, TEXT("(int) model_optimization_level : %d "), CompilationOptions.OptimizationLevel);
		UE_LOG(LogMutable, Log, TEXT("(string) model_texture_compression : %s "), *UEnum::GetValueAsString(CompilationOptions.TextureCompression));
		UE_LOG(LogMutable, Log, TEXT("(string) model_disk_compilation : %s "), CompilationOptions.bUseDiskCompilation ? TEXT("true") : TEXT("false"));

    	// Compile the CO with the provided compilation options
    	// Run Sync compilation -> Warning : Potentially long operation -------------
		const double CompilationStartSeconds = FPlatformTime::Seconds();
		{
			Compiler->Compile(*ToTestCustomizableObject, CompilationOptions, false);
		}
		const double CompilationEndSeconds = FPlatformTime::Seconds() - CompilationStartSeconds;
		UE_LOG(LogMutable, Log, TEXT("(double) model_compile_time_ms : %f "), CompilationEndSeconds * 1000);
		UE_LOG(LogMutable, Display, TEXT("The compilation of the %s model took %f seconds."), *ToTestCustomizableObject->GetName(), CompilationEndSeconds);

    	// --------------------------------------------------------------------------
		
    	// Get the compilation result
    	const ECustomizableObjectCompilationState CompilationEndResult = Compiler->GetCompilationState();
    	check(CompilationEndResult != ECustomizableObjectCompilationState::None);
    	check(CompilationEndResult != ECustomizableObjectCompilationState::InProgress);
    	
    	bWasCoCompilationSuccessful = CompilationEndResult == ECustomizableObjectCompilationState::Completed;
	}
	// -------------------------------------------------------------------------------------------------------------- //
	
	if (bWasCoCompilationSuccessful)
	{
		UE_LOG(LogMutable,Display,TEXT("Customizable Object was compiled succesfully."));
		
		// GHet the total size of the streaming data of the model ---------------------------------------------- //
		const TSharedPtr<const mu::Model> MutableModel = ToTestCustomizableObject->GetModel();
		check (MutableModel);

		// Roms ---------------------- //
		{
			const int32 RomCount =  MutableModel->GetRomCount();
			int64 TotalRomSizeBytes = 0;
			for (int32 RomIndex = 0; RomIndex < RomCount; RomIndex++)
			{
				const uint32 RomByteSize = MutableModel->GetRomSize(RomIndex);
				TotalRomSizeBytes += RomByteSize;
			}

			// Print MTU parseable logs
			UE_LOG(LogMutable, Log,TEXT("(int) model_rom_count : %d "), RomCount);
			UE_LOG(LogMutable, Log,TEXT("(int) model_roms_size : %lld "), TotalRomSizeBytes);
		}

		// CO embedded data size ------ //
		{
			TArray<uint8> EmbeddedDataBytes{};
			FMemoryWriter SerializationTarget{EmbeddedDataBytes, false};
			
			ToTestCustomizableObject->SaveEmbeddedData(SerializationTarget);
			const int64 COEmbeddedDataSizeBytes = EmbeddedDataBytes.Num();
			
			UE_LOG(LogMutable, Log,TEXT("(int) co_embedded_data_bytes : %lld "), COEmbeddedDataSizeBytes);
		}

		// Generate target random instances to be tested ------------------------------------------------------------ //
		bool bWasInstancesCreationSuccessful = true;
		{
			UE_LOG(LogMutable,Display,TEXT("Generating %i random instances..."), InstancesToGenerate);	

			// Create randomization stream for the parameters of the instance
			FRandomStream RandomizationStream = FRandomStream(0);
			
			// Generate a series of instances to later update
			for (uint32 CustomizableObjectInstanceIndex = 0; CustomizableObjectInstanceIndex < InstancesToGenerate; CustomizableObjectInstanceIndex++)
			{
				UCustomizableObjectInstance* GeneratedInstance = ToTestCustomizableObject->CreateInstance();
				if (GeneratedInstance)
				{
					// Force generation of all LODS
					TArray<uint16> RequestedLodLevels{};
					RequestedLodLevels.Init(MAX_uint8, GeneratedInstance->GetNumComponents());
					GeneratedInstance->GetDescriptor().SetRequestedLODLevels(RequestedLodLevels);
					
					// Randomize instance values
					GeneratedInstance->SetRandomValuesFromStream(RandomizationStream);
					InstancesToProcess.Push(GeneratedInstance);
				}
				else
				{
					UE_LOG(LogMutable,Error,TEXT("Failed to generate COI for the %s CO."),*ToTestCustomizableObject->GetName());
					bWasInstancesCreationSuccessful = false;
				}
			}
		}
		// ---------------------------------------------------------------------------------------------------------- //
		
		// Update the instances generated --------------------------------------------------------------------------- //
		UE_LOG(LogMutable,Display,TEXT("Updating generated instances..."));
		const double InstanceUpdateStartSeconds = FPlatformTime::Seconds();
		{
            // Now update the instances one by one
            while (!InstancesToProcess.IsEmpty() || InstanceBeingUpdated)
            {
            	// Tick the engine
            	CommandletHelpers::TickEngine();
    
            	// Stop if exit was requested
            	if (IsEngineExitRequested())
            	{
            		break;
            	}
            
            	// Wait until current instance turns invalid
            	if (InstanceBeingUpdated)
            	{
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
            				UE_LOG(LogMutable,Display,TEXT("Instance %s finished streaming all MIPs."), *InstanceBeingUpdated->GetName());
            				ComponentsBeingUpdated.Reset();
            				InstanceBeingUpdated = nullptr;
            			}
            		}
            		
            		continue;
            	}
           
            	// We may have already updated the instance so ensure we have more instances to work with
            	if (InstancesToProcess.IsEmpty())
            	{
            		continue;
            	}
            	
            	InstanceBeingUpdated = InstancesToProcess[0];
            	InstancesToProcess.RemoveAt(0);
            	if (InstanceBeingUpdated)
            	{
            		UE_LOG(LogMutable,Display,TEXT("Invoking update for %s instance."),*InstanceBeingUpdated->GetName());
            		// Instance update delegate
            		FInstanceUpdateDelegate InstanceUpdateDelegate;
            		InstanceUpdateDelegate.BindDynamic(this, &UCustomizableObjectValidationCommandlet::OnInstanceUpdate);
            		InstanceBeingUpdated->UpdateSkeletalMeshAsyncResult(InstanceUpdateDelegate);
            	}
            }
		}	
		
		// Notify and log time required by the instances to get updated
		const double CombinedInstanceUpdateSeconds = FPlatformTime::Seconds() - InstanceUpdateStartSeconds;
		UE_LOG(LogMutable, Log,TEXT("(double) combined_update_time_ms : %f "), CombinedInstanceUpdateSeconds * 1000);

		check(InstancesToGenerate > 0);
		const double AverageInstanceUpdateSeconds = CombinedInstanceUpdateSeconds / InstancesToGenerate;
		UE_LOG(LogMutable, Log,TEXT("(double) avg_update_time_ms : %f "), AverageInstanceUpdateSeconds * 1000);

		UE_LOG(LogMutable,Display,TEXT("Generation of Customizable object instances took %f seconds (%f seconds avg)."), CombinedInstanceUpdateSeconds, AverageInstanceUpdateSeconds);
		// ---------------------------------------------------------------------------------------------------------- //

		// Compute instance update result
		const bool bInstancesTestedSuccessfully = !bInstanceFailedUpdate && bWasInstancesCreationSuccessful;
		if (bInstancesTestedSuccessfully)
        {
        	UE_LOG(LogMutable,Display,TEXT("Generation of Customizable object instances was succesfull."));
        }
        else
        {
        	UE_LOG(LogMutable,Error,TEXT("The generation of Customizable object instances  was not succesfull."));
        }
	}
	else
	{
		UE_LOG(LogMutable,Error,TEXT("The compilation of the Customizable object was not succesfull."));
	}
	
	
	// If something failed then fail the commandlet execution
	UE_LOG(LogMutable,Display,TEXT("Mutable commandlet finished."));
	return 0;
}


void UCustomizableObjectValidationCommandlet::OnInstanceUpdate(const FUpdateContext& Result)
{
	const FString InstanceName = InstanceBeingUpdated->GetName();
	const EUpdateResult InstanceUpdateResult = Result.UpdateResult;
	if (InstanceUpdateResult == EUpdateResult::Success)
	{
		UE_LOG(LogMutable,Display,TEXT("Instance %s finished update succesfully."),*InstanceName);

		// Request load all MIPs
		UE_LOG(LogMutable,Display,TEXT("Instance %s rquesting streaming all MIPs."), *InstanceBeingUpdated->GetName());

		check(ComponentsBeingUpdated.IsEmpty());
		for (int32 Index = 0; Index < InstanceBeingUpdated->GetNumComponents(); ++Index)
		{
			USkeletalMeshComponent* SkeletalComponent = NewObject<USkeletalMeshComponent>();
			SkeletalComponent->SetSkeletalMesh(InstanceBeingUpdated->GetSkeletalMesh(Index));
            
			ComponentsBeingUpdated.Add(SkeletalComponent);            			
		}
		
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

		InstanceBeingUpdated = nullptr;
	}	
}

