// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Grammar/PCGGrammar.h"
#include "Utils/PCGLogErrors.h"

#include "PCGSlicingBase.generated.h"

class UPCGData;
class UPCGPointData;

USTRUCT(BlueprintType)
struct FPCGSlicingSubmodule
{
	GENERATED_BODY()

	/** Symbol for the grammar. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "")
	FName Symbol = NAME_None;

	/** Size of the block, aligned on the segment direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "")
	double Size = 100.0;

	/** If the volume can be scaled to fit the remaining space or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "")
	bool bScalable = false;

	/** For easier debugging, using Point color in conjunction with PCG Debug Color Material. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug")
	FVector4 DebugColor = FVector4::One();
};

namespace PCGSlicingBaseConstants
{
	const FName ModulesInfoPinLabel = TEXT("ModulesInfo");
	const FName SymbolAttributeName = TEXT("Symbol");
	const FName SizeAttributeName = TEXT("Size");
	const FName ScalableAttributeName = TEXT("Scalable");
	const FName DebugColorAttributeName = TEXT("DebugColor");
}

USTRUCT(BlueprintType)
struct FPCGSlicingModuleAttributeNames
{
	GENERATED_BODY()

public:
	/** Mandatory. Expected type: FName. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "")
	FName SymbolAttributeName = PCGSlicingBaseConstants::SymbolAttributeName;

	/** Mandatory. Expected type: double. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "")
	FName SizeAttributeName = PCGSlicingBaseConstants::SizeAttributeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "", meta = (InlineEditConditionToggle))
	bool bProvideScalable = false;

	/** Optional. Expected type: bool. If disabled, default value will be false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "", meta = (EditCondition = "bProvideScalable"))
	FName ScalableAttributeName = PCGSlicingBaseConstants::ScalableAttributeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "", meta = (InlineEditConditionToggle))
	bool bProvideDebugColor = false;

	/** Optional. Expected type: Vector4. If disabled, default value will be (1.0, 1.0, 1.0, 1.0). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "", meta = (EditCondition = "bProvideDebugColor"))
	FName DebugColorAttributeName = PCGSlicingBaseConstants::DebugColorAttributeName;
};

UCLASS(MinimalAPI, Abstract, ClassGroup = (Procedural))
class UPCGSlicingBaseSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
	//~End UPCGSettings interface

	virtual void PostLoad() override;

public:
	/** Set it to true to pass the info as attribute set. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bModuleInfoAsInput = false;

	/** Fixed array of modules used for the slicing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bModuleInfoAsInput", EditConditionHides, DisplayAfter = bModuleInfoAsInput))
	TArray<FPCGSlicingSubmodule> ModulesInfo;

	/** Fixed array of modules used for the slicing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bModuleInfoAsInput", EditConditionHides, DisplayAfter = bModuleInfoAsInput, DisplayName = "Attribute Names for Module Info"))
	FPCGSlicingModuleAttributeNames ModulesInfoAttributeNames;

	/** An encoded string that represents how to apply a set of rules to a series of defined modules. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGGrammarSelection GrammarSelection;

	/** Attribute to be taken from the input spline containing the grammar to use for the slicing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bGrammarAsAttribute", EditConditionHides, DisplayAfter = bGrammarAsAttribute, PCG_Overridable))
	FPCGAttributePropertyInputSelector GrammarAttribute;

	/** Do a match and set with the incoming modules info, only if the modules info is passed as input. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Output Attributes", meta = (EditCondition = "bModuleInfoAsInput", EditConditionHides, PCG_Overridable))
	bool bForwardAttributesFromModulesInfo = false;

	/** Name of the Symbol output attribute name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Output Attributes", meta = (PCG_Overridable))
	FName SymbolAttributeName = PCGSlicingBaseConstants::SymbolAttributeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Output Attributes", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputSizeAttribute = true;

	/** Name of the Size output attribute name, ignored if match and set from module info is true. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Output Attributes", meta = (PCG_Overridable, EditCondition = "bOutputSizeAttribute"))
	FName SizeAttributeName = PCGSlicingBaseConstants::SizeAttributeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Output Attributes", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputScalableAttribute = true;

	/** Name of the Scalable output attribute name, ignored if match and set from module info is true. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Output Attributes", meta = (PCG_Overridable, EditCondition = "bOutputScalableAttribute"))
	FName ScalableAttributeName = PCGSlicingBaseConstants::ScalableAttributeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Output Attributes", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputDebugColorAttribute = false;

	/** Name of the Debug Color output attribute name, ignored if match and set from module info is true. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Output Attributes", meta = (PCG_Overridable, EditCondition = "bOutputDebugColorAttribute"))
	FName DebugColorAttributeName = PCGSlicingBaseConstants::DebugColorAttributeName;

private:
#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.5, "Use 'GrammarSelection' instead.")
	UPROPERTY()
	bool bGrammarAsAttribute_DEPRECATED = false;

	UE_DEPRECATED(5.5, "Use 'GrammarSelection' instead.")
	UPROPERTY()
	FString Grammar_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};

namespace PCGSlicingBase
{
	using FPCGModulesInfoMap = TMap<FName, FPCGSlicingSubmodule>;

#define PCG_SLICING_BASE_USES_CONCEPTS 0

#if PCG_SLICING_BASE_USES_CONCEPTS
	// To validate that the type has the right interface
	// To be enabled when this is supported on all our platforms.
	template <typename T>
	concept IsValidPCGSubDivModuleInstance =
		requires(T t) {
			{ t.IsValid() } -> std::same_as<bool>;
			{ t.GetSize() } -> std::same_as<double>;
			{ t.GetNumRepeat() } -> std::same_as<int>;
			{ t.IsScalable() } -> std::same_as<bool>;
			{ t.GetSubmodulesCount() } -> std::same_as<int32>;
			{ t.AreSubmodulesScalable() } -> std::same_as<TArrayView<const bool>>;
			{ t.SubmoduleSizes() } -> std::same_as<TArrayView<const double>>;
	};
#endif // PCG_SLICING_BASE_USES_CONCEPTS

	template <typename T>
	struct TPCGSubDivModuleInstance
	{
		const T* Module = nullptr;
		int32 NumRepeat = 0;
		// If the module is made of multiple submodules, we'll have 1 extra scale per submodule.
		// Extra scale to add to the module initial scale on the slicing direction.
		TArray<double> ExtraScales;
	};

	PCGGrammar::FTokenizedGrammar GetTokenizedGrammar(FPCGContext* InContext, const FString& InGrammar, const FPCGModulesInfoMap& InModulesInfo, double& OutMinSize);

	template <typename T>
	bool Subdivide(const TArray<T>& Modules, double Length, TArray<TPCGSubDivModuleInstance<T>>& OutModuleInstances, double& RemainingLength, FPCGContext* InOptionalContext = nullptr)
	{
		OutModuleInstances.Empty(Modules.Num());
		RemainingLength = Length;

		if (Modules.IsEmpty() || FMath::IsNearlyZero(Length))
		{
			return true;
		}

		for (const T& Module : Modules)
		{
			if (!Module.IsValid())
			{
				continue;
			}

			TPCGSubDivModuleInstance<T>& ModuleInstance = OutModuleInstances.Emplace_GetRef();
			ModuleInstance.Module = &Module;
			ModuleInstance.ExtraScales.SetNumZeroed(Module.GetSubmodulesCount());
			if (Module.GetNumRepeat() > 0)
			{
				ModuleInstance.NumRepeat = Module.GetNumRepeat();
				RemainingLength -= Module.GetSize() * ModuleInstance.NumRepeat;
				if (RemainingLength < 0)
				{
					PCGLog::LogErrorOnGraph(NSLOCTEXT("PCGSlicingBase", "SegmentCutFail", "Grammar doesn't fit for this segment."), InOptionalContext);
					return false;
				}
			}
		}

		if (OutModuleInstances.IsEmpty())
		{
			return false;
		}

		// When we are done and we still have some segemnt left, place the repeatable
		int32 CurrentModuleIndex = 0;
		bool bHasModifiedSomething = false;
		while (RemainingLength >= 0)
		{
			TPCGSubDivModuleInstance<T>& ModuleInstance = OutModuleInstances[CurrentModuleIndex];
			if (ModuleInstance.Module->GetNumRepeat() <= 0)
			{
				if (RemainingLength >= ModuleInstance.Module->GetSize())
				{
					++ModuleInstance.NumRepeat;
					RemainingLength -= ModuleInstance.Module->GetSize();
					bHasModifiedSomething = true;
				}
			}

			if (++CurrentModuleIndex == OutModuleInstances.Num())
			{
				if (!bHasModifiedSomething)
				{
					// Nothing left
					break;
				}

				bHasModifiedSomething = false;
				CurrentModuleIndex = 0;
			}
		}

		// Finally, try to stretch the scalable to get a complete match.
		if (!FMath::IsNearlyZero(RemainingLength))
		{
			int32 NumScalableSubmodules = 0;

			TArray<TPCGSubDivModuleInstance<T>*> ScalableInstances;
			for (TPCGSubDivModuleInstance<T>& ModuleInstance : OutModuleInstances)
			{
				if (ModuleInstance.Module->IsScalable() && ModuleInstance.NumRepeat > 0)
				{
					ScalableInstances.Add(&ModuleInstance);
					for (const bool bIsScalable : ModuleInstance.Module->AreSubmodulesScalable())
					{
						if (bIsScalable)
						{
							NumScalableSubmodules += ModuleInstance.NumRepeat;
						}
					}
				}
			}


			if (NumScalableSubmodules > 0 && !ScalableInstances.IsEmpty())
			{
				const double ExtraLengthPerSubmodule = RemainingLength / NumScalableSubmodules;

				for (TPCGSubDivModuleInstance<T>* ModuleInstance : ScalableInstances)
				{
					const TArrayView<const bool> AreSubmodulesScalable = ModuleInstance->Module->AreSubmodulesScalable();
					const TArrayView<const double> SubmoduleSizes = ModuleInstance->Module->SubmoduleSizes();
					check(AreSubmodulesScalable.Num() == SubmoduleSizes.Num());
					for (int32 i = 0; i < SubmoduleSizes.Num(); ++i)
					{
						if (AreSubmodulesScalable[i])
						{
							const double SubmoduleSize = SubmoduleSizes[i];
							if (!FMath::IsNearlyZero(SubmoduleSize))
							{
								ModuleInstance->ExtraScales[i] = ExtraLengthPerSubmodule / SubmoduleSize;
							}
						}
					}
				}

				RemainingLength = 0.0;
			}
		}

		return true;
	}
}

class FPCGSlicingBaseElement : public IPCGElement
{
protected:
	using FPCGModulesInfoMap = PCGSlicingBase::FPCGModulesInfoMap;

	FPCGModulesInfoMap GetModulesInfoMap(FPCGContext* InContext, const TArray<FPCGSlicingSubmodule>& SubmodulesInfo, const UPCGParamData*& OutModuleInfoParamData) const;
	FPCGModulesInfoMap GetModulesInfoMap(FPCGContext* InContext, const FPCGSlicingModuleAttributeNames& InSlicingModuleAttributeNames, const UPCGParamData*& OutModuleInfoParamData) const;
	FPCGModulesInfoMap GetModulesInfoMap(FPCGContext* InContext, const UPCGSlicingBaseSettings* InSettings, const UPCGParamData*& OutModuleInfoParamData) const;
	PCGGrammar::FTokenizedGrammar GetTokenizedGrammar(FPCGContext* InContext, const UPCGData* InputData, const UPCGSlicingBaseSettings* InSettings, const FPCGModulesInfoMap& InModulesInfo, double& OutMinSize) const;
	TMap<FString, PCGGrammar::FTokenizedGrammar> GetTokenizedGrammarForPoints(FPCGContext* InContext, const UPCGPointData* InputData, const UPCGSlicingBaseSettings* InSettings, const FPCGModulesInfoMap& InModulesInfo, double& OutMinSize) const;
	bool MatchAndSetAttributes(const TArray<FPCGTaggedData>& InputData, TArray<FPCGTaggedData>& OutputData, const UPCGParamData* InModuleInfoParamData, const UPCGSlicingBaseSettings* InSettings) const;
};
