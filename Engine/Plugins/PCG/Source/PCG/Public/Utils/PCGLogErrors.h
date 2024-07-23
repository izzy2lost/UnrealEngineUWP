// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Internationalization.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"

enum class EPCGDataType : uint32;
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

	namespace Accessor
	{
		namespace ErrorFormat
		{
			extern const FTextFormat GetTypedAttributeFailure;
			extern const FTextFormat GetTypedAttributeFailureNoAccessor;
		}

		void LogFailToCreate(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext = nullptr);
		void LogFailToGet(FName AttributeName, const FPCGContext* InContext = nullptr);
		void LogFailToGet(FText AttributeName, const FPCGContext* InContext = nullptr);
		void LogFailToGet(const FPCGAttributePropertySelector& Selector, const FPCGContext* InContext = nullptr);

		template <typename T>
		void LogFailToGet(FText AttributeName, const IPCGAttributeAccessor* Accessor, const FPCGContext* InContext = nullptr)
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
		void LogFailToGet(FName AttributeName, const IPCGAttributeAccessor* Accessor, const FPCGContext* InContext = nullptr)
		{
			LogFailToGet<T>(FText::FromName(AttributeName), Accessor, InContext);
		}

		template <typename T>
		void LogFailToGet(const FPCGAttributePropertySelector& Selector, const IPCGAttributeAccessor* Accessor, const FPCGContext* InContext = nullptr)
		{
			return LogFailToGet<T>(Selector.GetDisplayText(), Accessor, InContext);
		}
	}
}
