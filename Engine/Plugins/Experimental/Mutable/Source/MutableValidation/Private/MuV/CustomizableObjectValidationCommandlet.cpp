// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuV/CustomizableObjectValidationCommandlet.h"

#include "ValidationUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Array.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuR/Model.h"


int32 UCustomizableObjectValidationCommandlet::Main(const FString& Params)
{
	// Execution arguments for commandlet from IDE
	// -run=CustomizableObjectValidation -CustomizableObject=(PathToCO)
	
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
	UE_LOG(LogMutable, Log,TEXT("(int) instances_to_generate_count : %u "), InstancesToGenerate);
	
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
	PrepareAssetRegistry();
	
	// Compile the Customizable Object ------------------------------------------------------------------------------ //
	bool bWasCoCompilationSuccessful = false;
	{
		// Override some configurations that may have been changed by the user
		FCompilationOptions CompilationOptions = ToTestCustomizableObject->CompileOptions;
		CompilationOptions.bSilentCompilation = false;
		CompilationOptions.OptimizationLevel = 2;			// Set the optimization level to the max
		CompilationOptions.TextureCompression = ECustomizableObjectTextureCompression::Fast;

		bWasCoCompilationSuccessful = CompileCustomizableObject(ToTestCustomizableObject, &CompilationOptions);
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
		bool bInstanceFailedUpdate = false;
		const double InstancesUpdateStartSeconds = FPlatformTime::Seconds();
		{
			InstanceUpdater = NewObject<UCOIUpdater>();
			for (UCustomizableObjectInstance* InstanceToUpdate : InstancesToProcess)
			{
				if (InstanceUpdater && !InstanceUpdater->UpdateInstance(InstanceToUpdate))
				{
					bInstanceFailedUpdate = true;
				}
			}
		}
		const double InstancesUpdateEndSeconds = FPlatformTime::Seconds();
		
		// Notify and log time required by the instances to get updated
		const double CombinedInstanceUpdateSeconds = InstancesUpdateEndSeconds - InstancesUpdateStartSeconds;
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
		UE_LOG(LogMutable,Error,TEXT("The compilation of the Customizable object was not succesfull : No instances will be generated."));
	}
	
	
	// If something failed then fail the commandlet execution
	UE_LOG(LogMutable,Display,TEXT("Mutable commandlet finished."));
	return 0;
}
