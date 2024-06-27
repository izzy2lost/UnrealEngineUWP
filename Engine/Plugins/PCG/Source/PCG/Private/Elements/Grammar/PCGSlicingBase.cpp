// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Grammar/PCGSlicingBase.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Grammar/PCGGrammarParser.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#define LOCTEXT_NAMESPACE "PCGSlicingBaseElement"

namespace PCGSlicingBase
{
	static const FText AccessorFailGetText = LOCTEXT("AccessorFailGet", "Couldn't retrieve attribute {0} value, could it be the wrong type? Expected type: {1}, Attribute Type: {2}.");
	static const FText AccessorGrammarFailCreate = LOCTEXT("GrammarAccessor", "Attribute {0} was not found for the grammar.");
	static const FText DuplicatedSymbolText = LOCTEXT("SymbolDuplicate", "Symbol {0} is duplicated, ignored.");
}

FPCGSlicingBaseElement::FPCGModulesInfoMap FPCGSlicingBaseElement::GetModulesInfoMap(FPCGContext* InContext, const UPCGSlicingBaseSettings* InSettings, const UPCGParamData*& OutModuleInfoParamData) const
{
	FPCGModulesInfoMap ModulesInfo;
	OutModuleInfoParamData = nullptr;

	if (InSettings->bModuleInfoAsInput)
	{
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

		const TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(ParamData, FPCGAttributePropertySelector::CreateAttributeSelector(InSettings->ModulesInfoAttributeNames.SymbolAttributeName));

		auto GetRange = [InContext, &Keys, ParamData]<typename T>(TArray<T>& OutValues, const FName AttributeName, const bool bProvided = true)
		{
			if (!bProvided)
			{
				return true;
			}

			const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(ParamData, FPCGAttributePropertySelector::CreateAttributeSelector(AttributeName));
			if (!Accessor || !Keys)
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("ModuleInfoMissingAttribute", "Module info input is missing attribute {0}."), FText::FromName(AttributeName)), InContext);
				return false;
			}

			OutValues.SetNum(Keys->GetNum());

			if (!Accessor->GetRange<T>(OutValues, 0, *Keys, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
			{
				PCGLog::LogErrorOnGraph(FText::Format(PCGSlicingBase::AccessorFailGetText, FText::FromName(AttributeName), PCG::Private::GetTypeNameText<T>(), PCG::Private::GetTypeNameText(Accessor->GetUnderlyingType())), InContext);
				return false;
			}

			return true;
		};

		TArray<FName> Symbols;
		TArray<double> Sizes;
		TArray<bool> Scalables;
		TArray<FVector4> DebugColors;

		if (!GetRange(Symbols, InSettings->ModulesInfoAttributeNames.SymbolAttributeName)
			|| !GetRange(Sizes, InSettings->ModulesInfoAttributeNames.SizeAttributeName)
			|| !GetRange(Scalables, InSettings->ModulesInfoAttributeNames.ScalableAttributeName, InSettings->ModulesInfoAttributeNames.bProvideScalable)
			|| !GetRange(DebugColors, InSettings->ModulesInfoAttributeNames.DebugColorAttributeName, InSettings->ModulesInfoAttributeNames.bProvideDebugColor))
		{
			return ModulesInfo;
		}

		check(Keys);
		ModulesInfo.Reserve(Keys->GetNum());

		for (int32 i = 0; i < Keys->GetNum(); ++i)
		{
			if (ModulesInfo.Contains(Symbols[i]))
			{
				PCGLog::LogWarningOnGraph(FText::Format(PCGSlicingBase::DuplicatedSymbolText, FText::FromName(Symbols[i])), InContext);
				continue;
			}

			FPCGSlicingModule& Module = ModulesInfo.Emplace(Symbols[i]);
			Module.Symbol = Symbols[i];
			Module.Size = Sizes[i];

			Module.bScalable = InSettings->ModulesInfoAttributeNames.bProvideScalable ? Scalables[i] : false;
			Module.DebugColor = InSettings->ModulesInfoAttributeNames.bProvideDebugColor ? DebugColors[i] : FVector4::One();
		}

		OutModuleInfoParamData = ParamData;
	}
	else
	{
		ModulesInfo.Reserve(InSettings->ModulesInfo.Num());
		for (const FPCGSlicingModule& SlicingModule : InSettings->ModulesInfo)
		{
			if (ModulesInfo.Contains(SlicingModule.Symbol))
			{
				PCGLog::LogWarningOnGraph(FText::Format(PCGSlicingBase::DuplicatedSymbolText, FText::FromName(SlicingModule.Symbol)), InContext);
				continue;
			}

			ModulesInfo.Emplace(SlicingModule.Symbol, SlicingModule);
		}
	}

	return ModulesInfo;
}

TArray<FPCGTokenizedGrammar> FPCGSlicingBaseElement::GetTokenizeGrammar(FPCGContext* InContext, const UPCGData* InputData, const UPCGSlicingBaseSettings* InSettings, const FPCGModulesInfoMap& InModulesInfo, double& OutMinSize) const
{
	FString Grammar = InSettings->Grammar;

	if (InSettings->bGrammarAsAttribute)
	{
		const FPCGAttributePropertyInputSelector Selector = InSettings->GrammarAttribute.CopyAndFixLast(InputData);
		const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputData, Selector);
		if (!Accessor)
		{
			PCGLog::LogErrorOnGraph(FText::Format(PCGSlicingBase::AccessorGrammarFailCreate, Selector.GetDisplayText()), InContext);
			return {};
		}

		if (!Accessor->Get(Grammar, FPCGAttributeAccessorKeysEntries(PCGInvalidEntryKey), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
		{
			PCGLog::LogErrorOnGraph(FText::Format(PCGSlicingBase::AccessorFailGetText, Selector.GetDisplayText(), PCG::Private::GetTypeNameText<FString>(), PCG::Private::GetTypeNameText(Accessor->GetUnderlyingType())), InContext);
			return {};
		}
	}

	return GetTokenizeGrammar(InContext, Grammar, InModulesInfo, OutMinSize);
}

TArray<FPCGTokenizedGrammar> FPCGSlicingBaseElement::GetTokenizeGrammar(FPCGContext* InContext, const FString& InGrammar, const FPCGModulesInfoMap& InModulesInfo, double& OutMinSize) const
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

	TArray<FPCGTokenizedGrammar> TokenizeGrammar;
	OutMinSize = 0.0;

	for (const PCGGrammar::FModuleDescriptor& ModuleDescriptor : Result.Modules)
	{
		FPCGTokenizedGrammar& CurrentModule = TokenizeGrammar.Emplace_GetRef();
		CurrentModule.NumRepeat = ModuleDescriptor.Repetitions;

		for (const PCGGrammar::FModuleDescriptor::FSubmodule& SubmoduleDescriptor : ModuleDescriptor.Submodules)
		{
			if (const FPCGSlicingModule* It = InModulesInfo.Find(SubmoduleDescriptor.ID))
			{
				CurrentModule.Symbols.Add(SubmoduleDescriptor.ID);
				CurrentModule.Size += It->Size;
				CurrentModule.bScalable |= It->bScalable;
				CurrentModule.AreSymbolsScalable.Add(It->bScalable);
				CurrentModule.SymbolSizes.Add(It->Size);
			}
		}
		if (CurrentModule.NumRepeat > 0)
		{
			OutMinSize += CurrentModule.Size * CurrentModule.NumRepeat;
		}

		if (CurrentModule.Symbols.IsEmpty())
		{
			// If we have no symbol, we skip.
			continue;
		}
	}

	return TokenizeGrammar;
}

TMap<FString, TArray<FPCGTokenizedGrammar>> FPCGSlicingBaseElement::GetTokenizeGrammarForPoints(FPCGContext* InContext, const UPCGPointData* InputData, const UPCGSlicingBaseSettings* InSettings, const FPCGModulesInfoMap& InModulesInfo, double& OutMinSize) const
{
	TMap<FString, TArray<FPCGTokenizedGrammar>> Result;

	if (InSettings->bGrammarAsAttribute)
	{
		const FPCGAttributePropertyInputSelector Selector = InSettings->GrammarAttribute.CopyAndFixLast(InputData);
		const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputData, Selector);
		const TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(InputData, Selector);
		if (!Accessor || !Keys)
		{
			PCGLog::LogErrorOnGraph(FText::Format(PCGSlicingBase::AccessorGrammarFailCreate, Selector.GetDisplayText()), InContext);
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
			PCGLog::LogErrorOnGraph(FText::Format(PCGSlicingBase::AccessorFailGetText, Selector.GetDisplayText(), PCG::Private::GetTypeNameText<FString>(), PCG::Private::GetTypeNameText(Accessor->GetUnderlyingType())), InContext);
			return Result;
		}
	}
	else
	{
		Result.Emplace(InSettings->Grammar);
	}

	for (auto& [Grammar, TokenizeGrammar] : Result)
	{
		TokenizeGrammar = GetTokenizeGrammar(InContext, Grammar, InModulesInfo, OutMinSize);
	}

	return Result;
}

bool FPCGSlicingBaseElement::MatchAndSetAttributes(const TArray<FPCGTaggedData>& InputData, TArray<FPCGTaggedData>& OutputData, const UPCGParamData* InModuleInfoParamData, const UPCGSlicingBaseSettings* InSettings) const
{
	check(InModuleInfoParamData && InModuleInfoParamData->Metadata);

	const UPCGMetadata* InputMetadata = InModuleInfoParamData->Metadata;

	// We prepare everything to process all the output data afterwrads.
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