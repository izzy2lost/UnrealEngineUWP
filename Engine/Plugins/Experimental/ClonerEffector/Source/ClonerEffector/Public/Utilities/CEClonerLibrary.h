// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "CEClonerLibrary.generated.h"

template <typename T> class TSubclassOf;
class UCEClonerExtensionBase;
class UCEClonerLayoutBase;

/** Blueprint operations for cloner */
UCLASS(MinimalAPI, DisplayName="Motion Design Cloner Library")
class UCEClonerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Retrieves all layout classes available for a cloner
	 * @param OutLayoutClasses [Out] Layout classes available
	 */
	UFUNCTION(BlueprintCallable, Category="Motion Design|Cloner|Utility")
	static CLONEREFFECTOR_API void GetClonerLayoutClasses(TSet<TSubclassOf<UCEClonerLayoutBase>>& OutLayoutClasses);

	/**
	 * Retrieves all extension classes available for a cloner
	 * @param OutExtensionClasses [Out] Extension classes available
	 */
	UFUNCTION(BlueprintCallable, Category="Motion Design|Cloner|Utility")
	static CLONEREFFECTOR_API void GetClonerExtensionClasses(TSet<TSubclassOf<UCEClonerExtensionBase>>& OutExtensionClasses);
};