// Copyright Epic Games, Inc. All Rights Reserved.

#include "../../Public/MuV/COIBakingTestCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCOE/CustomizableObjectInstanceBakingUtils.h"
#include "HAL/FileManager.h"
#include "MuT/UnrealPixelFormatOverride.h"


/** Flag useful to know if we are currently updating an instance or not */
bool bIsInstanceBeingUpdated = false;

/** Did the instance update finish with a successful status? */
bool bWasInstanceUpdateSuccessful = false;


void OnInstanceUpdate(const FUpdateContext& Result)
{
	const EUpdateResult InstanceUpdateResult = Result.UpdateResult;

	UE_LOG(LogMutable,Display,TEXT("Instance finished update with state : %s."), *UEnum::GetValueAsString(Result.UpdateResult));
	bWasInstanceUpdateSuccessful = InstanceUpdateResult == EUpdateResult::Success;
	
	// Clear update flag so we can exit the update while loop
	bIsInstanceBeingUpdated = false;
}


int32 UCOIBakingTestCommandlet::Main(const FString& Params)
{
	// Ensure we do not show any OK dialog since we are not an user that can interact with them
	GIsRunningUnattendedScript = true;
	
	// Look for the COI to be baked and load it
	{
		FString CustomizableObjectInstanceAssetPath = "";
		if (!FParse::Value(*Params, TEXT("CustomizableObjectInstance="), CustomizableObjectInstanceAssetPath))
		{
			UE_LOG(LogMutable,Error,TEXT("Failed to parse Customizable Object Instance package name from provided argument : %s"),*Params)
			return 1;
		}
	
		// Load the resource
		UObject* FoundObject = FSoftObjectPath(CustomizableObjectInstanceAssetPath).TryLoad();
		if (!FoundObject)
		{
			UE_LOG(LogMutable,Error,TEXT("Failed to retrieve UObject from path %s"),*CustomizableObjectInstanceAssetPath);
			return 1;
		}
	
		// Get the CustomizableObjectInstance.
		TargetInstance = Cast<UCustomizableObjectInstance>(FoundObject);
		if (!TargetInstance)
		{
			UE_LOG(LogMutable,Error,TEXT("Failed to cast found UObject to UCustomizableObjectInstance."));
			return 1;
		}

		UE_LOG(LogMutable,Display,TEXT("Succesfully loaded %s Customizable Object Instance!"), *TargetInstance->GetName());
	}

	// Perform a blocking search to ensure all assets used by mutable are reachable using the AssetRegistry
	{
		UE_LOG(LogMutable,Display,TEXT("Searching all assets (this will take some time)..."));
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		AssetRegistryModule.Get().SearchAllAssets(true /* bSynchronousSearch */);
	}
	
	// Compile it's CO (using current config)
	UCustomizableObject* InstanceCustomizableObject = TargetInstance->GetCustomizableObject();
	UCustomizableObjectSystem* System =  UCustomizableObjectSystem::GetInstanceChecked();
	check (InstanceCustomizableObject);
	{
		UE_LOG(LogMutable,Display,TEXT("Compiling Customizable Object..."));
    	
		// Request a compiler to be able to locate the root and to compile it
		const TUniquePtr<FCustomizableObjectCompilerBase> Compiler =
			TUniquePtr<FCustomizableObjectCompilerBase>(System->GetNewCompiler());

		// Compile (sync)
		// Compile the CO with the provided compilation options
		// Run Sync compilation -> Warning : Potentially long operation -------------
		Compiler->Compile(*InstanceCustomizableObject, InstanceCustomizableObject->CompileOptions, false);
		// --------------------------------------------------------------------------
		
		// Get the compilation result
		const ECustomizableObjectCompilationState CompilationEndResult = Compiler->GetCompilationState();
		check(CompilationEndResult != ECustomizableObjectCompilationState::None);
		check(CompilationEndResult != ECustomizableObjectCompilationState::InProgress);

		UE_LOG(LogMutable, Display, TEXT("CO compilation completed with end status : %s "), *UEnum::GetValueAsString(CompilationEndResult));
		
		const bool bWasCompilationSuccessful = CompilationEndResult == ECustomizableObjectCompilationState::Completed;
		if (!bWasCompilationSuccessful)
		{
			UE_LOG(LogMutable,Error,TEXT("Failed to compile the target CO."));
			return 1;
		}
	}
	
	// Update the instance
	{
		// If this fail something is very wrong
		check(TargetInstance);

		// Instance update delegate
		FInstanceUpdateNativeDelegate InstanceUpdateDelegate;
		InstanceUpdateDelegate.AddStatic(&OnInstanceUpdate);
		
		bIsInstanceBeingUpdated = true;
		UE_LOG(LogMutable,Display,TEXT("Invoking instance update."));
		UpdateInstanceForBaking(*TargetInstance, InstanceUpdateDelegate);
		
		// Now tick the engine so the instance gets updated while running in the commandlet context
        while (bIsInstanceBeingUpdated)
        {
            // Tick the engine
            CommandletHelpers::TickEngine();

            // Stop if exit was requested
            if (IsEngineExitRequested())
            {
            	break;
            }
        }
		
		// Check the end status of the instance update
		if (!bWasInstanceUpdateSuccessful)
		{
			UE_LOG(LogMutable,Error,TEXT("Failed to succesfully update the target COI."));
			return 1;
		}
	}
	

	// Bake the instance
	{
		const FString BakedResourcesFileName = "MuBakedInstances";
		
		// If this fail something is very wrong
		check(TargetInstance);
		const FString InstanceName = TargetInstance.GetName();
		
		// Create the actual directory in the filesystem of the host machine
		const FString GlobalBakingDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine( FPaths::ProjectContentDir(), BakedResourcesFileName, InstanceName));

		// This will only happen if we did make a partial run before and therefore the directory was not cleansed.
		// Delete it then but notify the user since this may mean that we have duplicated COIs
		if (FPaths::DirectoryExists(GlobalBakingDirectory))
		{
			UE_LOG(LogMutable,Warning,TEXT("The directory with path  \" %s \" does already exist. This may be produced by an incompelte execution of a previous test. Clearing it out before continuing..."), *GlobalBakingDirectory);
			if (!IFileManager::Get().DeleteDirectory(*GlobalBakingDirectory,false,true))
			{
				UE_LOG(LogMutable,Error,TEXT("Failed to deleta baking directory at path \" %s \"."), *GlobalBakingDirectory);
				return 1;
			}
		}
		
		// Compute the local path to the generated directory where to save the baked data
		const FString LocalBakingDirectory = FPaths::Combine("/","Game", BakedResourcesFileName, InstanceName);
		
		// Create a new directory where to save the bake itself
		if (IFileManager::Get().MakeDirectory(*GlobalBakingDirectory, true))
		{
			UE_LOG(LogMutable,Display,TEXT("Starting Instance Baking operation."));
			{
				BakeCustomizableObjectInstance(
					*TargetInstance,
					FString::Printf(TEXT("%s_Bake"), *TargetInstance->GetName()),
					LocalBakingDirectory,
					true,
					true);
			}
			UE_LOG(LogMutable,Display,TEXT("Instance Baking operation has been completed."));
			
			// Delete the target directory where we did save the baked instance
			if (!IFileManager::Get().DeleteDirectory(*GlobalBakingDirectory,true,true))
			{
				UE_LOG(LogMutable,Error,TEXT("Failed to deleta baking directory at path \" %s \"."), *GlobalBakingDirectory);
				return 1;
			}
		}
		else
		{
			UE_LOG(LogMutable,Error,TEXT("Failed to create baking directory at path \" %s \"."), *GlobalBakingDirectory);
			return 1;
		}
	}
	
	return 0;
}



