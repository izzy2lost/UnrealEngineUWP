// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Revisions/DMXGDTFRevision.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFRevision::FDMXGDTFRevision(const TSharedRef<FDMXGDTFFixtureType>& InFixtureType)
		: OuterFixtureType(InFixtureType)
	{}

	void FDMXGDTFRevision::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Text"), Text)
			.GetAttribute(TEXT("Date"), Date, this, &FDMXGDTFRevision::ParseDateTime)
			.GetAttribute(TEXT("User"), User)
			.GetAttribute(TEXT("ModifiedBy"), ModifiedBy);
	}

	FDateTime FDMXGDTFRevision::ParseDateTime(const FString& GDTFString) const
	{
		FDateTime DateTime;
		if (FDateTime::Parse(GDTFString, DateTime))
		{
			return DateTime;
		}
		else
		{
			return FDateTime::MinValue();
		}
	}
}
