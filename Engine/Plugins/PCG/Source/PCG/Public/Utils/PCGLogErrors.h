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
		namespace ErrorFormat
		{
			extern const FTextFormat TypedInputNotFoundWarning;
			extern const FTextFormat FirstInputOnlyWarning;
		}

		void LogTypedDataNotFoundWarning(EPCGDataType DataType, const FName PinLabel, const FPCGContext* InContext = nullptr);
		void LogFirstInputOnlyWarning(const FName PinLabel, const FPCGContext* InContext = nullptr);
	}

	namespace Metadata
	{
		namespace ErrorFormat
		{
			extern const FTextFormat CreateAttributeFailure;
			extern const FTextFormat GetTypedAttributeFailure;
			extern const FTextFormat GetTypedAttributeFailureNoAccessor;
		}

		void LogFailToCreateAccessor(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext = nullptr);

		template <typename T>
		void LogFailToCreateAttribute(FText AttributeName, const FPCGContext* InContext = nullptr)
		{
			PCGLog::LogErrorOnGraph(FText::Format(ErrorFormat::CreateAttributeFailure, AttributeName, PCG::Private::GetTypeNameText<T>()), InContext);
		}

		template <typename T>
		void LogFailToCreateAttribute(FName AttributeName, const FPCGContext* InContext = nullptr)
		{
			LogFailToCreateAttribute<T>(FText::FromName(AttributeName), InContext);
		}

		void LogFailToGetAttribute(FText AttributeName, const FPCGContext* InContext = nullptr);
		void LogFailToGetAttribute(FName AttributeName, const FPCGContext* InContext = nullptr);
		void LogFailToGetAttribute(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext = nullptr);

		template <typename T>
		void LogFailToGetAttribute(FText AttributeName, const IPCGAttributeAccessor* Accessor, const FPCGContext* InContext = nullptr)
		{
			if (Accessor)
			{
				PCGLog::LogErrorOnGraph(FText::Format(ErrorFormat::GetTypedAttributeFailure, AttributeName, PCG::Private::GetTypeNameText<T>(), PCG::Private::GetTypeNameText(Accessor->GetUnderlyingType())), InContext);
			}
			else
			{
				PCGLog::LogErrorOnGraph(FText::Format(ErrorFormat::GetTypedAttributeFailureNoAccessor, AttributeName, PCG::Private::GetTypeNameText<T>()), InContext);
			}
		}

		template <typename T>
		void LogFailToGetAttribute(FName AttributeName, const IPCGAttributeAccessor* Accessor, const FPCGContext* InContext = nullptr)
		{
			LogFailToGetAttribute<T>(FText::FromName(AttributeName), Accessor, InContext);
		}

		template <typename T>
		void LogFailToGetAttribute(const FPCGAttributePropertySelector& Selector, const IPCGAttributeAccessor* Accessor, const FPCGContext* InContext = nullptr)
		{
			return LogFailToGetAttribute<T>(Selector.GetDisplayText(), Accessor, InContext);
		}
	}
}
