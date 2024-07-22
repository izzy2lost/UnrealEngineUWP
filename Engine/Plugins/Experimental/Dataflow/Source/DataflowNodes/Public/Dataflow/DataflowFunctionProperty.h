// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "DataflowFunctionProperty.generated.h"

/**
 * Function property for all Dataflow nodes.
 * The structure is also used in DataFlow::FFunctionPropertyCustomization to appear as text and/or image buttons.
 * This helps with the equivalent UCLASS UFUNCTION CallInEditor functionality that is missing from the USTRUCT implementation.
 *
 * By default the text of the button is the name of the structure property.
 * The tooltip is the property source documentation.
 * Further (but optional) customizations can be achieved by using the following Meta tags where declaring the property:
 *   ButtonText
 *   ButtonImage
 *
 * Specifying an empty ButtonText string will only display the icon and no text.
 *
 * For example:
 *   UPROPERTY(EditAnywhere, Category = "Functions")
 *   FDataflowFunctionProperty ReimportAssetTextOnly;
 *
 *   UPROPERTY(EditAnywhere, Category = "Functions", Meta = (ButtonImage = "Persona.ReimportAsset"))
 *   FDataflowFunctionProperty ReimportAssetTextAndIcon;
 * 
 *   UPROPERTY(EditAnywhere, Category = "Functions", Meta = (ButtonText = "", ButtonImage = "Persona.ReimportAsset"))
 *   FDataflowFunctionProperty ReimportAssetIconOnly;
 *
 *   UPROPERTY(EditAnywhere, Category = "Functions", Meta = (ButtonText = "Reimport Asset"))
 *   FDataflowFunctionProperty ReimportAssetOverriddenText;
 *
 *   UPROPERTY(EditAnywhere, Category = "Functions", Meta = (ButtonText = "Reimport Asset", ButtonImage = "Persona.ReimportAsset"))
 *   FDataflowFunctionProperty ReimportAssetOverriddenTextAndIcon;
 *
 */
USTRUCT()
struct FDataflowFunctionProperty
{
	GENERATED_BODY()

public:

	FDataflowFunctionProperty() = default;

	explicit FDataflowFunctionProperty(FSimpleDelegate&& InDelegate) { Delegate = MoveTemp(InDelegate); }

	void Execute() const { Delegate.ExecuteIfBound(); }

private:
	FSimpleDelegate Delegate;
};
