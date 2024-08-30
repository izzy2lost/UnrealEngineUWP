// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"

class SWidget;
class UPackage;
struct FSlateBrush;

enum class EStatusSeverity
{
	Info,
	Warning,
	Error
};
ENUM_CLASS_FLAGS(EStatusSeverity)

struct FAssetStatusPriority
{
	FAssetStatusPriority()
		: Severity(EStatusSeverity::Info)
		, SeverityPriority(0)
	{}

	explicit FAssetStatusPriority(const TAttribute<EStatusSeverity>& InSeverity)
		: Severity(InSeverity)
		, SeverityPriority(0)
	{}

	FAssetStatusPriority(const TAttribute<EStatusSeverity>& InSeverity, int32 InSeverityPriority)
		: Severity(InSeverity)
		, SeverityPriority(InSeverityPriority)
	{}

public:
	bool operator==(const FAssetStatusPriority& InOtherStatusPriority) const
	{
		if (!Severity.IsSet() || !InOtherStatusPriority.Severity.IsSet())
		{
			return false;
		}

		return Severity.Get() == InOtherStatusPriority.Severity.Get() && SeverityPriority == InOtherStatusPriority.SeverityPriority;
	}

	bool operator<(const FAssetStatusPriority& InOtherStatusPriority) const
	{
		if (!Severity.IsSet())
		{
			return true;
		}

		if (!InOtherStatusPriority.Severity.IsSet())
		{
			return false;
		}

		if (Severity.Get() == InOtherStatusPriority.Severity.Get())
		{
			return SeverityPriority < InOtherStatusPriority.SeverityPriority;
		}

		return Severity.Get() < InOtherStatusPriority.Severity.Get();
	}

public:
	TAttribute<EStatusSeverity> Severity;
	int32 SeverityPriority;
};

struct FAssetStatus
{
public:
	TAttribute<const FSlateBrush*> StatusIcon;
	TAttribute<FText> StatusDescription;
	TAttribute<EVisibility> IsVisible;
	TAttribute<FAssetStatusPriority> Priority;
};

struct FAssetStatusInfo
{
public:
	TArray<FAssetStatus> AssetStatus;
};

class ASSETDEFINITION_API IAssetStatusInfoProvider
{
public:
	virtual ~IAssetStatusInfoProvider() = default;

	/** Try to find the Package without loading it, if the package is not loaded it will return nullptr */
	virtual UPackage* FindPackage() const = 0;

	/** Try to get the filename, return empty otherwise */
	virtual FString TryGetFilename() const = 0;
};
