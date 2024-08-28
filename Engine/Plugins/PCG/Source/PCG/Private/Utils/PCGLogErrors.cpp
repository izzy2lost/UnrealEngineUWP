// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGLog"

namespace PCGLog::InputOutput
{
	namespace Format
	{
		const FTextFormat TypedInputNotFound = LOCTEXT("TypedInputNotFound", "Data of type {0} not found on pin '{1}'.");
		const FTextFormat FirstInputOnly = LOCTEXT("FirstInputOnly", "Multiple inputs found on single-input pin '{0}'. Only the first will be selected.");
		const FText InvalidInputData = LOCTEXT("InvalidInputData", "Invalid input data.");
	}

	void LogTypedDataNotFoundWarning(EPCGDataType DataType, const FName PinLabel, const FPCGContext* InContext)
	{
		const UEnum* PCGDataTypeEnum = StaticEnum<EPCGDataType>();
		const FText TypeText = PCGDataTypeEnum ? PCGDataTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(DataType)) : LOCTEXT("UnknownDataType", "Unknown");
		LogWarningOnGraph(FText::Format(Format::TypedInputNotFound, FText::FromName(PinLabel), TypeText), InContext);
	}

	void LogFirstInputOnlyWarning(const FName PinLabel, const FPCGContext* InContext)
	{
		LogWarningOnGraph(FText::Format(Format::FirstInputOnly, FText::FromName(PinLabel)), InContext);
	}

	void LogInvalidInputDataError(const FPCGContext* InContext)
	{
		LogErrorOnGraph(Format::InvalidInputData, InContext);
	}
}

namespace PCGLog::Metadata
{
	namespace Format
	{
		const FTextFormat CreateAccessorFailure = LOCTEXT("CreateAccessorFailure", "Couldn't create accessor. Attribute '{0}' was not found.");
		const FTextFormat CreateAttributeFailure = LOCTEXT("CreateAttributeFailure", "Couldn't create attribute '{0}' of type '{1}'.");
		const FTextFormat GetAttributeFailure = LOCTEXT("GetAttributeFailure", "Couldn't retrieve attribute '{0}' value. Expected type: {1}, Actual Type: {2}.");
		const FTextFormat GetTypedAttributeFailure = LOCTEXT("GetTypedAttributeFailure", "Couldn't retrieve attribute '{0}' value. Expected type: {1}, Actual Type: {2}.");
		const FTextFormat GetTypedAttributeFailureNoAccessor = LOCTEXT("GetTypedAttributeFailureNoAccessor", "Couldn't retrieve attribute '{0}' value of type: '{1}.");
	}

	void LogFailToCreateAccessorError(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext)
	{
		LogErrorOnGraph(FText::Format(Format::CreateAccessorFailure, Selector.GetDisplayText()), InContext);
	}

	void LogFailToGetAttributeError(FText AttributeName, const FPCGContext* InContext)
	{
		LogErrorOnGraph(FText::Format(Format::GetAttributeFailure, std::move(AttributeName)), InContext);
	}

	void LogFailToGetAttributeError(FName AttributeName, const FPCGContext* InContext)
	{
		LogFailToGetAttributeError(FText::FromName(AttributeName), InContext);
	}

	void LogFailToGetAttributeError(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext)
	{
		LogErrorOnGraph(FText::Format(Format::GetAttributeFailure, Selector.GetDisplayText()), InContext);
	}
}

#undef LOCTEXT_NAMESPACE
