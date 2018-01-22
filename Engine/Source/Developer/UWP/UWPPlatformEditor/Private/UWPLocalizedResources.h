#pragma once

#include "UWPLocalizedResources.generated.h"

USTRUCT()
struct FUWPCorePackageStringResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString PackageDisplayName;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString PublisherDisplayName;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString PackageDescription;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString ApplicationDisplayName;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString ApplicationDescription;
};

USTRUCT()
struct FUWPDlcStringResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString PackageDisplayName;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString PackageDescription;
};

USTRUCT()
struct FUWPCorePackageImageResources
{
	GENERATED_BODY()
};

USTRUCT()
struct FUWPDlcImageResources
{
	GENERATED_BODY()
};

USTRUCT()
struct FUWPCorePackageLocalizedResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString CultureId;

	UPROPERTY(EditAnywhere, config, Category = Packaging, Meta=(ShowOnlyInnerProperties))
	FUWPCorePackageStringResources Strings;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FUWPCorePackageImageResources Images;
};

USTRUCT()
struct FUWPDlcLocalizedResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString AppliesToDlcPlugin;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString CultureId;

	UPROPERTY(EditAnywhere, config, Category = Packaging, Meta = (ShowOnlyInnerProperties))
	FUWPDlcStringResources Strings;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FUWPDlcImageResources Images;
};