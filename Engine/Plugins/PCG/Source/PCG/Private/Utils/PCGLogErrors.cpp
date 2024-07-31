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

namespace PCGLog::Metadata
{
	namespace ErrorFormat
	{
		const FTextFormat CreateAccessorFailure = LOCTEXT("CreateAccessorFailure", "Couldn't create accessor. Attribute '{0}' was not found.");
		const FTextFormat CreateAttributeFailure = LOCTEXT("CreateTypedAttributeFailure", "Couldn't create attribute '{0}' of type '{1}'.");
		const FTextFormat GetAttributeFailure = LOCTEXT("GetAttributeFailure", "Couldn't retrieve attribute '{0}' value. Expected type: {1}, Actual Type: {2}.");
		const FTextFormat GetTypedAttributeFailure = LOCTEXT("GetAttributeFailure", "Couldn't retrieve attribute '{0}' value. Expected type: {1}, Actual Type: {2}.");
		const FTextFormat GetTypedAttributeFailureNoAccessor = LOCTEXT("GetAttributeFailureNoAccessor", "Couldn't retrieve attribute '{0}' value of type: '{1}.");
	}

	void LogFailToCreateAccessor(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext)
	{
		LogErrorOnGraph(FText::Format(ErrorFormat::CreateAccessorFailure, Selector.GetDisplayText()), InContext);
	}

	void LogFailToGetAttribute(FText AttributeName, const FPCGContext* InContext)
	{
		LogErrorOnGraph(FText::Format(ErrorFormat::GetAttributeFailure, std::move(AttributeName)), InContext);
	}

	void LogFailToGetAttribute(FName AttributeName, const FPCGContext* InContext)
	{
		LogFailToGetAttribute(FText::FromName(AttributeName), InContext);
	}

	void LogFailToGetAttribute(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext)
	{
		LogErrorOnGraph(FText::Format(ErrorFormat::GetAttributeFailure, Selector.GetDisplayText()), InContext);
	}
}

#undef LOCTEXT_NAMESPACE
