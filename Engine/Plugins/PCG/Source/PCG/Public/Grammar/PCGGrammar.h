// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGGrammar.generated.h"

namespace PCGGrammar
{
	struct FTokenizedModule
	{
		TArray<FName> Symbols;
		TArray<bool> AreSymbolsScalable;
		TArray<double> SymbolSizes;
		int NumRepeat = 1;
		bool bScalable = false;
		double Size = 0.0;

		bool IsValid() const { return Size > 0 && !Symbols.IsEmpty() && Symbols.Num() == AreSymbolsScalable.Num() && Symbols.Num() == SymbolSizes.Num(); }
		double GetSize() const { return Size; }
		int GetNumRepeat() const { return NumRepeat; }
		bool IsScalable() const { return bScalable; }
		int32 GetSubmodulesCount() const { return Symbols.Num(); }
		TArrayView<const bool> AreSubmodulesScalable() const { return AreSymbolsScalable; }
		TArrayView<const double> SubmoduleSizes() const { return SymbolSizes; }
	};

	using FTokenizedGrammar = TArray<FTokenizedModule>;
}

USTRUCT(Blueprintable, BlueprintType)
struct FPCGGrammarSelection
{
	GENERATED_BODY()

	/** Read the grammar as an attribute rather than directly from the settings.
	* Grammar syntax:
	* - Each symbol can have multiple characters
	* - Modules are defined in '[]', multiple symbols in a module are separated with ','
	* - Modules can be repeated a fixed number of times, by adding a number after it (like [A,B]3 will produce ABABAB)
	* - Modules can be marked repeated an indefinite number of times, with '*'. (like [A,B]* will produce ABABABAB... while it fits the allowed size).
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bGrammarAsAttribute = false;

	/** An encoded string that represents how to apply a set of rules to a series of defined modules. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bGrammarAsAttribute", EditConditionHides, PCG_Overridable))
	FString GrammarString;

	/** Attribute to be taken from the input spline containing the grammar to use. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bGrammarAsAttribute", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyInputSelector GrammarAttribute;
};
