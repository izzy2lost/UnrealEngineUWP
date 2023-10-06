// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenXRInputSettings.h"
#include "XRMotionControllerBase.h"
#include "PlayerMappableInputConfig.h"
#include "InputMappingContext.h"
#include "HAL/FileManager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Features/IModularFeatures.h"
#include "SourceControlHelpers.h"
#endif

UOpenXRInputSettings::UOpenXRInputSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void UOpenXRInputSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if (MappableInputConfig.IsValid())
	{
		UPlayerMappableInputConfig* InputConfig = Cast<UPlayerMappableInputConfig>(MappableInputConfig.TryLoad());
		if (InputConfig)
		{
			for (const auto& Context : InputConfig->GetMappingContexts())
			{
				TSoftObjectPtr<UInputMappingContext> Obj = Context.Key;
				InputMappingContexts.Add(Obj);
			}
			MappableInputConfig.Reset();

			// Check if the file is read only. If it is, then try and check it out with source control
			const FString ConfigFileName = GetDefaultConfigFilename();
			bool bCanWriteToFile = !IFileManager::Get().IsReadOnly(*ConfigFileName) || (USourceControlHelpers::IsEnabled() && USourceControlHelpers::CheckOutFile(ConfigFileName));

			if (bCanWriteToFile)
			{
				TryUpdateDefaultConfigFile();
			}			
		}
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
