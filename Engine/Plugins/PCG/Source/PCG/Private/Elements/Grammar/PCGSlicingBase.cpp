// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Grammar/PCGSlicingBase.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Grammar/PCGGrammarParser.h"
#include "Helpers/PCGPropertyHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#define LOCTEXT_NAMESPACE "PCGSlicingBaseElement"

namespace PCGSlicingBase
{
	static const FText DuplicatedSymbolText = LOCTEXT("SymbolDuplicate", "Symbol {0} is duplicated, ignored.");
}

void UPCGSlicingBaseSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (bGrammarAsAttribute_DEPRECATED != false)
	{
		GrammarSelection.bGrammarAsAttribute = bGrammarAsAttribute_DEPRECATED;
		bGrammarAsAttribute_DEPRECATED = false;
	}

	if (!Grammar_DEPRECATED.IsEmpty())
	{
		GrammarSelection.GrammarString = Grammar_DEPRECATED;
		Grammar_DEPRECATED.Empty();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR
}

FPCGSlicingBaseElement::FPCGModulesInfoMap FPCGSlicingBaseElement::GetModulesInfoMap(FPCGContext* InContext, const TArray<FPCGSlicingSubmodule>& SubmodulesInfo, const UPCGParamData*& OutModuleInfoParamData) const
{
	PCGSlicingBase::FPCGModulesInfoMap ModulesInfo;
	OutModuleInfoParamData = nullptr;

	ModulesInfo.Reserve(SubmodulesInfo.Num());
	for (const FPCGSlicingSubmodule& SlicingModule : SubmodulesInfo)
	{
		if (ModulesInfo.Contains(SlicingModule.Symbol))
		{
			PCGLog::LogWarningOnGraph(FText::Format(PCGSlicingBase::DuplicatedSymbolText, FText::FromName(SlicingModule.Symbol)), InContext);
			continue;
		}

		ModulesInfo.Emplace(SlicingModule.Symbol, SlicingModule);
	}

	return ModulesInfo;
}

FPCGSlicingBaseElement::FPCGModulesInfoMap FPCGSlicingBaseElement::GetModulesInfoMap(FPCGContext* InContext, const FPCGSlicingModuleAttributeNames& InSlicingModuleAttributeNames, const UPCGParamData*& OutModuleInfoParamData) const
{
	PCGSlicingBase::FPCGModulesInfoMap ModulesInfo;
	OutModuleInfoParamData = nullptr;

	const TArray<FPCGTaggedData> ModulesInfoInputs = InContext->InputData.GetInputsByPin(PCGSlicingBaseConstants::ModulesInfoPinLabel);

	if (ModulesInfoInputs.IsEmpty())
	{
		PCGLog::LogWarningOnGraph(LOCTEXT("NoModuleInfo", "No data was found on the module info pin."), InContext);
		return ModulesInfo;
	}

	const UPCGParamData* ParamData = Cast<UPCGParamData>(ModulesInfoInputs[0].Data);
	if (!ParamData)
	{
		PCGLog::LogWarningOnGraph(LOCTEXT("ModuleInfoWrongType", "Module info input is not of type attribute set."), InContext);
		return ModulesInfo;
	}

	TMap<FName, TTuple<FName, bool>> PropertyNameMapping;
	PropertyNameMapping.Emplace(GET_MEMBER_NAME_CHECKED(FPCGSlicingSubmodule, Symbol), {InSlicingModuleAttributeNames.SymbolAttributeName, /*bCanBeDefaulted=*/false});
	PropertyNameMapping.Emplace(GET_MEMBER_NAME_CHECKED(FPCGSlicingSubmodule, Size), {InSlicingModuleAttributeNames.SizeAttributeName, /*bCanBeDefaulted=*/false});
	PropertyNameMapping.Emplace(GET_MEMBER_NAME_CHECKED(FPCGSlicingSubmodule, bScalable), {InSlicingModuleAttributeNames.ScalableAttributeName, /*bCanBeDefaulted=*/!InSlicingModuleAttributeNames.bProvideScalable});
	PropertyNameMapping.Emplace(GET_MEMBER_NAME_CHECKED(FPCGSlicingSubmodule, DebugColor), {InSlicingModuleAttributeNames.DebugColorAttributeName, /*bCanBeDefaulted=*/!InSlicingModuleAttributeNames.bProvideDebugColor});

	const TArray<FPCGSlicingSubmodule> AllModules = PCGPropertyHelpers::ExtractAttributeSetAsArrayOfStructs<FPCGSlicingSubmodule>(ParamData, &PropertyNameMapping, InContext);

	ModulesInfo.Reserve(AllModules.Num());

	for (int32 i = 0; i < AllModules.Num(); ++i)
	{
		if (ModulesInfo.Contains(AllModules[i].Symbol))
		{
			PCGLog::LogWarningOnGraph(FText::Format(PCGSlicingBase::DuplicatedSymbolText, FText::FromName(AllModules[i].Symbol)), InContext);
			continue;
		}

		ModulesInfo.Emplace(AllModules[i].Symbol, std::move(AllModules[i]));
	}

	OutModuleInfoParamData = ParamData;

	return ModulesInfo;
}

PCGSlicingBase::FPCGModulesInfoMap FPCGSlicingBaseElement::GetModulesInfoMap(FPCGContext* InContext, const UPCGSlicingBaseSettings* InSettings, const UPCGParamData*& OutModuleInfoParamData) const
{
	if (InSettings->bModuleInfoAsInput)
	{
		return GetModulesInfoMap(InContext, InSettings->ModulesInfoAttributeNames, OutModuleInfoParamData);
	}
	else
	{
		return GetModulesInfoMap(InContext, InSettings->ModulesInfo, OutModuleInfoParamData);
	}
}

PCGGrammar::FTokenizedGrammar FPCGSlicingBaseElement::GetTokenizedGrammar(FPCGContext* InContext, const UPCGData* InputData, const UPCGSlicingBaseSettings* InSettings, const FPCGModulesInfoMap& InModulesInfo, double& OutMinSize) const
{
	FString Grammar = InSettings->GrammarSelection.GrammarString;

	if (InSettings->GrammarSelection.bGrammarAsAttribute)
	{
		const FPCGAttributePropertyInputSelector Selector = InSettings->GrammarSelection.GrammarAttribute.CopyAndFixLast(InputData);
		const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputData, Selector);
		if (!Accessor)
		{
			PCGLog::Metadata::LogFailToCreateAccessor(Selector, InContext);
			return {};
		}

		if (!Accessor->Get(Grammar, FPCGAttributeAccessorKeysEntries(PCGInvalidEntryKey), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
		{
			PCGLog::Metadata::LogFailToGetAttribute<FString>(Selector, Accessor.Get(), InContext);
			return {};
		}
	}

	return PCGSlicingBase::GetTokenizedGrammar(InContext, Grammar, InModulesInfo, OutMinSize);
}

PCGGrammar::FTokenizedGrammar PCGSlicingBase::GetTokenizedGrammar(FPCGContext* InContext, const FString& InGrammar, const FPCGModulesInfoMap& InModulesInfo, double& OutMinSize)
{
	const FPCGGrammarResult Result = PCGGrammar::Parse(InGrammar);

	if (!Result.bSuccess)
	{
		PCGLog::LogErrorOnGraph(LOCTEXT("GrammarParseFail", "Problem while parsing grammar:"), InContext);

		for (const FPCGGrammarResult::FLog& Log : Result.GetLogs())
		{
			switch (Log.Verbosity)
			{
			case FPCGGrammarResult::ELogType::Error:
				PCGLog::LogErrorOnGraph(Log.Message, InContext);
				break;
			case FPCGGrammarResult::ELogType::Warning:
				PCGLog::LogWarningOnGraph(Log.Message, InContext);
				break;
			default:
				UE_LOG(LogPCG, Log, TEXT("%s"), *Log.Message.ToString());
				break;
			}
		}

		return {};
	}

	PCGGrammar::FTokenizedGrammar TokenizedGrammar;
	OutMinSize = 0.0;

	for (const PCGGrammar::FModuleDescriptor& ModuleDescriptor : Result.Modules)
	{
		PCGGrammar::FTokenizedModule& CurrentModule = TokenizedGrammar.Emplace_GetRef();
		CurrentModule.NumRepeat = ModuleDescriptor.Repetitions;

		for (const PCGGrammar::FModuleDescriptor::FSubmodule& SubmoduleDescriptor : ModuleDescriptor.Submodules)
		{
			if (const FPCGSlicingSubmodule* It = InModulesInfo.Find(SubmoduleDescriptor.ID))
			{
				CurrentModule.Symbols.Add(SubmoduleDescriptor.ID);
				CurrentModule.Size += It->Size;
				CurrentModule.bScalable |= It->bScalable;
				CurrentModule.AreSymbolsScalable.Add(It->bScalable);
				CurrentModule.SymbolSizes.Add(It->Size);
			}
		}

		if (CurrentModule.Symbols.IsEmpty())
		{
			// If we have no symbol, we skip.
			continue;
		}

		if (CurrentModule.NumRepeat > 0)
		{
			OutMinSize += CurrentModule.Size * CurrentModule.NumRepeat;
		}
	}

	return TokenizedGrammar;
}

TMap<FString, PCGGrammar::FTokenizedGrammar> FPCGSlicingBaseElement::GetTokenizedGrammarForPoints(FPCGContext* InContext, const UPCGPointData* InputData, const UPCGSlicingBaseSettings* InSettings, const FPCGModulesInfoMap& InModulesInfo, double& OutMinSize) const
{
	TMap<FString, PCGGrammar::FTokenizedGrammar> Result;

	if (InSettings->GrammarSelection.bGrammarAsAttribute)
	{
		const FPCGAttributePropertyInputSelector Selector = InSettings->GrammarSelection.GrammarAttribute.CopyAndFixLast(InputData);
		const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputData, Selector);
		const TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(InputData, Selector);
		if (!Accessor || !Keys)
		{
			PCGLog::Metadata::LogFailToCreateAccessor(Selector, InContext);
			return Result;
		}

		const bool bSuccess = PCGMetadataElementCommon::ApplyOnAccessor<FString>(*Keys, *Accessor, [&Result](const FString& InValue, int32)
		{
			if (!Result.Contains(InValue))
			{
				Result.Emplace(InValue);
			}
		});

		if (!bSuccess)
		{
			PCGLog::Metadata::LogFailToGetAttribute<FString>(Selector, Accessor.Get(), InContext);
			return Result;
		}
	}
	else
	{
		Result.Emplace(InSettings->GrammarSelection.GrammarString);
	}

	for (auto& [Grammar, TokenizeGrammar] : Result)
	{
		TokenizeGrammar = PCGSlicingBase::GetTokenizedGrammar(InContext, Grammar, InModulesInfo, OutMinSize);
	}

	return Result;
}

bool FPCGSlicingBaseElement::MatchAndSetAttributes(const TArray<FPCGTaggedData>& InputData, TArray<FPCGTaggedData>& OutputData, const UPCGParamData* InModuleInfoParamData, const UPCGSlicingBaseSettings* InSettings) const
{
	check(InModuleInfoParamData && InModuleInfoParamData->Metadata);

	const UPCGMetadata* InputMetadata = InModuleInfoParamData->Metadata;

	// We prepare everything to process all the output data afterward.
	// Build the Symbol -> EntryKey mapping
	// Since we don't know if it is a Name or a String, we need to get both.
	TMap<FName, PCGMetadataEntryKey> SymbolToEntryKeyMapping;
	if (const FPCGMetadataAttribute<FName>* InSymbolNameAttribute = InputMetadata->GetConstTypedAttribute<FName>(PCGSlicingBaseConstants::SymbolAttributeName))
	{
		for (PCGMetadataEntryKey EntryKey = InputMetadata->GetItemKeyCountForParent(); EntryKey < InputMetadata->GetItemCountForChild(); ++EntryKey)
		{
			const FName Symbol = InSymbolNameAttribute->GetValueFromItemKey(EntryKey);
			if (!SymbolToEntryKeyMapping.Contains(Symbol))
			{
				SymbolToEntryKeyMapping.Emplace(Symbol, EntryKey);
			}
		}
	}
	else if (const FPCGMetadataAttribute<FString>* InSymbolStrAttribute = InputMetadata->GetConstTypedAttribute<FString>(PCGSlicingBaseConstants::SymbolAttributeName))
	{
		for (PCGMetadataEntryKey EntryKey = InputMetadata->GetItemKeyCountForParent(); EntryKey < InputMetadata->GetItemCountForChild(); ++EntryKey)
		{
			const FName Symbol(InSymbolStrAttribute->GetValueFromItemKey(EntryKey));
			if (!SymbolToEntryKeyMapping.Contains(Symbol))
			{
				SymbolToEntryKeyMapping.Emplace(Symbol, EntryKey);
			}
		}
	}
	else
	{
		return false;
	}

	// Also gather all the attribute in the input metadata
	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;
	InModuleInfoParamData->Metadata->GetAttributes(AttributeNames, AttributeTypes);
	check(AttributeNames.Num() == AttributeTypes.Num());

	for (FPCGTaggedData& TaggedData : OutputData)
	{
		// Be careful with the const cast, only allow it if the data is not present in the input too (forwarded)
		if (InputData.ContainsByPredicate([TaggedData](const FPCGTaggedData& InData) { return InData.Data == TaggedData.Data; }))
		{
			continue;
		}

		UPCGData* OutData = const_cast<UPCGData*>(TaggedData.Data.Get());
		check(OutData && OutData->MutableMetadata());

		UPCGMetadata* OutputMetadata = OutData->MutableMetadata();

		// Look for the Symbol Attribute in the output metadata to query its value.
		const FPCGMetadataAttribute<FName>* OutSymbolAttribute = OutputMetadata->GetConstTypedAttribute<FName>(InSettings->SymbolAttributeName);
		if (!ensure(OutSymbolAttribute))
		{
			continue;
		}

		if (UPCGPointData* OutPointData = Cast<UPCGPointData>(OutData))
		{
			TArray<const FPCGMetadataAttributeBase*> InAttributes;
			TArray<FPCGMetadataAttributeBase*> OutAttributes;

			InAttributes.Reserve(AttributeNames.Num());
			OutAttributes.Reserve(AttributeNames.Num());

			// Copy all the attributes in the output metadata
			for (const FName AttributeName : AttributeNames)
			{
				// Skip the symbol, it already exists, but perhaps with a different attribute name.
				if (AttributeName == PCGSlicingBaseConstants::SymbolAttributeName)
				{
					continue;
				}

				// Skip any attribute that already exists in the output data
				if (OutputMetadata->HasAttribute(AttributeName))
				{
					continue;
				}

				InAttributes.Add(InputMetadata->GetConstAttribute(AttributeName));
				OutAttributes.Add(OutputMetadata->CopyAttribute(InAttributes.Last(), AttributeName, /*bKeepParent=*/false, /*bCopyEntries=*/false, /*bCopyValues=*/true));
			}

			for (const FPCGPoint& OutPoint : OutPointData->GetPoints())
			{
				const FName Symbol = OutSymbolAttribute->GetValueFromItemKey(OutPoint.MetadataEntry);

				if (!SymbolToEntryKeyMapping.Contains(Symbol))
				{
					continue;
				}

				const PCGMetadataEntryKey InputEntryKey = SymbolToEntryKeyMapping[Symbol];

				if (InputEntryKey == PCGInvalidEntryKey)
				{
					continue;
				}

				for (int32 i = 0; i < InAttributes.Num(); ++i)
				{
					PCGMetadataValueKey InputValueKey = InAttributes[i]->GetValueKey(InputEntryKey);
					OutAttributes[i]->SetValueFromValueKey(OutPoint.MetadataEntry, InputValueKey);
				}
			}
		}
		else
		{
			// If we have a spatial data that is not a point data, we can only operate on Default values.
			// So add all the attributes, and set the default value to the same value for the chosen entry key.
			const FName Symbol = OutSymbolAttribute->GetValue(PCGDefaultValueKey);
			const PCGMetadataEntryKey InputEntryKey = SymbolToEntryKeyMapping[Symbol];

			auto AddAttribute = [InputMetadata, OutputMetadata, InputEntryKey]<typename T>(T Dummy, const FName AttributeName)
			{
				const FPCGMetadataAttribute<T>* InAttribute = InputMetadata->GetConstTypedAttribute<T>(AttributeName);
				check(InAttribute);

				// Overwrite any existing attribute
				if (OutputMetadata->HasAttribute(AttributeName))
				{
					OutputMetadata->DeleteAttribute(AttributeName);
				}

				OutputMetadata->CreateAttribute<T>(AttributeName, InAttribute->GetValueFromItemKey(InputEntryKey), InAttribute->AllowsInterpolation(), /*bOverrideParent=*/true);
			};

			for (int32 i = 0; i < AttributeNames.Num(); ++i)
			{
				// Skip the symbol attribute, as it is already in the OutData (with perhaps a different name)
				if (AttributeNames[i] == PCGSlicingBaseConstants::SymbolAttributeName)
				{
					continue;
				}

				// Skip any attribute that already exists in the output data
				if (OutputMetadata->HasAttribute(AttributeNames[i]))
				{
					continue;
				}

				PCGMetadataAttribute::CallbackWithRightType(static_cast<int16>(AttributeTypes[i]), AddAttribute, AttributeNames[i]);
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
