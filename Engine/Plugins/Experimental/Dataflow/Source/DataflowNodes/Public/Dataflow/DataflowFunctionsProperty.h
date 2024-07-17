// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "DataflowFunctionsProperty.generated.h"

/**
 * Function property for all Dataflow nodes.
 * The structure is also used in DataFlow::FFunctionDetailCustomization to appear as text and/or image buttons.
 * This helps with the equivalent UCLASS UFUNCTION CallInEditor functionality that is missing from the USTRUCT implementation.
 */
USTRUCT()
struct FDataflowFunctionsProperty
{
	GENERATED_BODY()

public:

	struct FFunction
	{
		FSimpleDelegate Delegate;

#if WITH_EDITORONLY_DATA
		FName ImageStyle;  // Button icon, optional if a name is provided, but can have both
		FText Name;  // Display Name, optional if an icon is provided, but can have both
		FText ToolTip;
#endif

		FFunction(const FName& InImageStyle, const FText& InName, const FText& InToolTip, FSimpleDelegate&& Delegate)
			: Delegate(MoveTemp(Delegate))
#if WITH_EDITORONLY_DATA
			, ImageStyle(InImageStyle)
			, Name(InName)
			, ToolTip(InToolTip)
#endif
		{}
	};

	FDataflowFunctionsProperty() = default;

	explicit FDataflowFunctionsProperty(TArray<FFunction>&& InFunctions) { Functions = MoveTemp(InFunctions); }

	const TArray<FFunction>& GetFunctions() const { return Functions; }

private:
	TArray<FFunction> Functions;
};
