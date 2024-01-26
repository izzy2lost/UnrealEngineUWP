// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Containers/ContainersFwd.h"
#include "Templates/SharedPointerFwd.h"
#include "AdvancedRenamerBlueprintLibrary.generated.h"

class AActor;
class IAdvancedRenamerProvider;
class IToolkitHost;
class SWidget;
class UUserWidget;

UCLASS(BlueprintType, ClassGroup="Advanced Rename Panel")
class ADVANCEDRENAMER_API UAdvancedRenamerBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Advanced Renamer Panel", meta = (DisplayName = "Open Advanced Rename Panel"))
	static void BP_OpenAdvancedRenamer(UObject* RenameProvider, UUserWidget* UserWidget = nullptr);

	static void OpenAdvancedRenamer(TSharedRef<IAdvancedRenamerProvider> RenameProvider, TSharedPtr<IToolkitHost> ToolkitHost);

	static void OpenAdvancedRenamer(TSharedRef<IAdvancedRenamerProvider> RenameProvider, TSharedPtr<SWidget> ParentWidget);

	UFUNCTION(BlueprintCallable, Category = "Advanced Renamer Panel", meta = (DisplayName = "Open Advanced Rename Panel For Actors"))
	static void BP_OpenAdvancedRenamerForActors(const TArray<AActor*>& Actors, UUserWidget* UserWidget = nullptr);

	static void OpenAdvancedRenamerForActors(const TArray<AActor*>& Actors, TSharedPtr<IToolkitHost> ToolkitHost);

	static void OpenAdvancedRenamerForActors(const TArray<AActor*>& Actors, TSharedPtr<SWidget> ParentWidget);

	UFUNCTION(BlueprintCallable, Category = "Advanced Renamer Panel")
	static TArray<AActor*> GetActorsSharingClassesInWorld(const TArray<AActor*>& InActors);
};
