// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGLog"

namespace PCGLog::InputOutput
{
	namespace ErrorFormat
	{
		const FTextFormat TypedInputNotFoundWarning = LOCTEXT("DataInputNotFound", "Data of type {0} not found on pin '{1}'.");
		const FTextFormat FirstInputOnlyWarning = LOCTEXT("FirstInputOnly", "Multiple inputs found on single-input pin '{0}'. Only the first will be selected.");
	}

	void LogTypedDataNotFoundWarning(EPCGDataType DataType, const FName PinLabel, const FPCGContext* InContext)
	{
		const UEnum* PCGDataTypeEnum = StaticEnum<EPCGDataType>();
		const FText TypeText = PCGDataTypeEnum ? PCGDataTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(DataType)) : LOCTEXT("UnknownDataType", "Unknown");
		LogWarningOnGraph(FText::Format(ErrorFormat::TypedInputNotFoundWarning, FText::FromName(PinLabel), TypeText), InContext);
	}

	void LogFirstInputOnlyWarning(const FName PinLabel, const FPCGContext* InContext)
	{
		LogWarningOnGraph(FText::Format(ErrorFormat::FirstInputOnlyWarning, FText::FromName(PinLabel)), InContext);
	}
}

namespace PCGLog::Accessor
{
	namespace ErrorFormat
	{
		const FTextFormat CreateAccessorFailure = LOCTEXT("CreateAccessorFailure", "Attribute {0} was not found.");
		const FTextFormat GetAttributeFailure = LOCTEXT("GetAttributeFailure", "Couldn't retrieve attribute '{0}' value. Expected type: {1}, Actual Type: {2}.");
		const FTextFormat GetTypedAttributeFailure = LOCTEXT("GetAttributeFailure", "Couldn't retrieve attribute '{0}' value. Expected type: {1}, Actual Type: {2}.");
		const FTextFormat GetTypedAttributeFailureNoAccessor = LOCTEXT("GetAttributeFailureNoAccessor", "Couldn't retrieve attribute '{0}' value of type: '{1}.");
	}

	void LogFailToCreate(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext)
	{
		LogErrorOnGraph(FText::Format(ErrorFormat::CreateAccessorFailure, Selector.GetDisplayText()), InContext);
	}

	void LogFailToGet(FName AttributeName, const FPCGContext* InContext)
	{
		LogFailToGet(FText::FromName(AttributeName), InContext);
	}

	void LogFailToGet(FText AttributeName, const FPCGContext* InContext)
	{
		LogErrorOnGraph(FText::Format(ErrorFormat::GetAttributeFailure, std::move(AttributeName)), InContext);
	}

	void LogFailToGet(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext)
	{
		LogErrorOnGraph(FText::Format(ErrorFormat::GetAttributeFailure, Selector.GetDisplayText()), InContext);
	}
}

#undef LOCTEXT_NAMESPACE
