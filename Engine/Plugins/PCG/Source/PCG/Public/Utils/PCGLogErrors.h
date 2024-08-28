// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"

#include "Internationalization/Internationalization.h"

struct FPCGContext;

namespace PCGLog
{
	/** Convenience function that would either log error on the graph if there is a context, or in the console if not. */
	PCG_API void LogErrorOnGraph(const FText& InMsg, const FPCGContext* InContext = nullptr);
	/** Convenience function that would either log warning on the graph if there is a context, or in the console if not. */
	PCG_API void LogWarningOnGraph(const FText& InMsg, const FPCGContext* InContext = nullptr);

	namespace InputOutput
	{
		namespace Format
		{
			extern const FTextFormat TypedInputNotFound;
			extern const FTextFormat FirstInputOnly;
			extern const FText InvalidInputData;
		}

		// Warnings
		void LogTypedDataNotFoundWarning(EPCGDataType DataType, const FName PinLabel, const FPCGContext* InContext = nullptr);
		void LogFirstInputOnlyWarning(const FName PinLabel, const FPCGContext* InContext = nullptr);

		// Errors
		void LogInvalidInputDataError(const FPCGContext* InContext = nullptr);
	}

	namespace Metadata
	{
		namespace Format
		{
			extern const FTextFormat CreateAttributeFailure;
			extern const FTextFormat GetTypedAttributeFailure;
			extern const FTextFormat GetTypedAttributeFailureNoAccessor;
		}

		// Errors
		void LogFailToCreateAccessorError(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext = nullptr);

		template <typename T>
		void LogFailToCreateAttributeError(FText AttributeName, const FPCGContext* InContext = nullptr)
		{
			PCGLog::LogErrorOnGraph(FText::Format(Format::CreateAttributeFailure, AttributeName, PCG::Private::GetTypeNameText<T>()), InContext);
		}

		template <typename T>
		void LogFailToCreateAttributeError(FName AttributeName, const FPCGContext* InContext = nullptr)
		{
			LogFailToCreateAttributeError<T>(FText::FromName(AttributeName), InContext);
		}

		void LogFailToGetAttributeError(FText AttributeName, const FPCGContext* InContext = nullptr);
		void LogFailToGetAttributeError(FName AttributeName, const FPCGContext* InContext = nullptr);
		void LogFailToGetAttributeError(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext = nullptr);

		template <typename T>
		void LogFailToGetAttributeError(FText AttributeName, const IPCGAttributeAccessor* Accessor, const FPCGContext* InContext = nullptr)
		{
			if (Accessor)
			{
				PCGLog::LogErrorOnGraph(FText::Format(Format::GetTypedAttributeFailure, AttributeName, PCG::Private::GetTypeNameText<T>(), PCG::Private::GetTypeNameText(Accessor->GetUnderlyingType())), InContext);
			}
			else
			{
				PCGLog::LogErrorOnGraph(FText::Format(Format::GetTypedAttributeFailureNoAccessor, AttributeName, PCG::Private::GetTypeNameText<T>()), InContext);
			}
		}

		template <typename T>
		void LogFailToGetAttributeError(FName AttributeName, const IPCGAttributeAccessor* Accessor, const FPCGContext* InContext = nullptr)
		{
			LogFailToGetAttributeError<T>(FText::FromName(AttributeName), Accessor, InContext);
		}

		template <typename T>
		void LogFailToGetAttributeError(const FPCGAttributePropertySelector& Selector, const IPCGAttributeAccessor* Accessor, const FPCGContext* InContext = nullptr)
		{
			return LogFailToGetAttributeError<T>(Selector.GetDisplayText(), Accessor, InContext);
		}
	}
}
