// Copyright Epic Games, Inc. All Rights Reserved.

#include "KPIValue.h"
#include "Misc/ConfigCacheIni.h"

UE_DISABLE_OPTIMIZATION_SHIP

FKPIValue::EState FKPIValue::GetState() const
{
	return State;
}

void FKPIValue::SetValue(float Value)
{
	CurrentValue = Value;

	switch (Compare)
	{
		default:
		case FKPIValue::LessThan:
		{
			State = CurrentValue < ThresholdValue ? FKPIValue::Good : FKPIValue::Bad;
			break;
		}

		case FKPIValue::LessThanOrEqual:
		{
			State = CurrentValue <= ThresholdValue ? FKPIValue::Good : FKPIValue::Bad;
			break;
		}

		case FKPIValue::GreaterThan:
		{
			State = CurrentValue > ThresholdValue ? FKPIValue::Good : FKPIValue::Bad;
			break;
		}

		case FKPIValue::GreaterThanOrEqual:
		{
			State = CurrentValue >= ThresholdValue ? FKPIValue::Good : FKPIValue::Bad;
			break;
		}
	}
}


FString FKPIValue::GetComparisonAsString(FKPIValue::ECompare Compare )
{
	switch (Compare)
	{
		default:
		case ECompare::LessThan:
		{
			return TEXT("<");
			break;
		}

		case ECompare::LessThanOrEqual:
		{
			return TEXT("<=");
			break;
		}

		case ECompare::GreaterThan:
		{
			return TEXT(">");
			break;
		}

		case ECompare::GreaterThanOrEqual:
		{
			return TEXT(">=");
			break;
		}
	}
}

FString FKPIValue::GetComparisonAsPrettyString(FKPIValue::ECompare Compare)
{
	switch (Compare)
	{
	default:
	case ECompare::LessThan:
	{
		return TEXT("less than");
		break;
	}

	case ECompare::LessThanOrEqual:
	{
		return TEXT("less than or equal");
		break;
	}

	case ECompare::GreaterThan:
	{
		return TEXT("greater than");
		break;
	}

	case ECompare::GreaterThanOrEqual:
	{
		return TEXT("greater than or equal");
		break;
	}
	}
}

FString	FKPIValue::GetValueAsString( float Value, FKPIValue::EDisplayType DisplayType)
{
	switch (DisplayType)
	{
		default:
		{
			return FString::Printf(TEXT("%.2f"), Value);
			break;
		}

		case EDisplayType::Decimal:
		{
			return FString::Printf(TEXT("%.0f"), Value);
			break;
		}

		case EDisplayType::Minutes:
		{
			const float Minutes = FMath::Floor(Value / 60.0f);
			const float Seconds = FMath::Modulo(Value, 60.0f);
			return (Minutes > 0.0)? FString::Printf(TEXT("%.0fm %2.0fs"), Minutes, Seconds) : FString::Printf(TEXT("%2.2fs"), Seconds);
			break;
		}

		case EDisplayType::Seconds:
		{
			return FString::Printf(TEXT("%.2fs"), Value);
			break;
		}

		case EDisplayType::Milliseconds:
		{
			return FString::Printf(TEXT("%.2fms"), Value);
			break;
		}

		case EDisplayType::Bytes:
		{
			return FString::Printf(TEXT("%.2fb"), Value);
			break;
		}

		case EDisplayType::MegaBytes:
		{
			return FString::Printf(TEXT("%.2fMb"), Value);
			break;
		}

		case EDisplayType::GigaBytes:
		{
			return FString::Printf(TEXT("%.2fGb"), Value);
			break;
		}

		case EDisplayType::MegaBitsPerSecond:
		{
			return FString::Printf(TEXT("%.2fMbps"), Value);
			break;
		}
		
		case EDisplayType::Percent:
		{
			return FString::Printf(TEXT("%.2f%%"), Value);
			break;
		}
	}
}

bool FKPIRegistry::DeclareKPIValue(const FName Category, const FName Name, float InitialValue, float ThresholdValue, FKPIValue::ECompare Compare, FKPIValue::EDisplayType Type)
{
	return DeclareKPIValue(FKPIValue(Category, Name, InitialValue, ThresholdValue, Compare, Type, FKPIValue::EState::NotSet));
}

bool FKPIRegistry::DeclareKPIValue( const FKPIValue& Value )
{
	if (Values.Find(Value.Name) == nullptr)
	{
		Values.Emplace(Value.Name, Value);
		return true;
	}
	return false;
}

bool FKPIRegistry::InvalidateKPIValue(const FName Name)
{
	FKPIValue* ExistingValue = Values.Find(Name);

	if (ExistingValue != nullptr)
	{
		ExistingValue->State = FKPIValue::NotSet;
		return true;
	}

	return false;
}

	


bool FKPIRegistry::SetKPIValue(const FName Name, float CurrentValue)
{
	FKPIValue* ExistingValue = Values.Find(Name);

	if (ExistingValue != nullptr)
	{
		ExistingValue->SetValue(CurrentValue);
		return true;
	}	

	return false;
}

bool FKPIRegistry::SetKPIThreshold(const FName Name, float ThresholdValue)
{
	FKPIValue* ExistingValue = Values.Find(Name);

	if (ExistingValue != nullptr)
	{
		ExistingValue->ThresholdValue = ThresholdValue;
		return true;
	}

	return false;
}

bool FKPIRegistry::GetKPIValue(const FName Name, FKPIValue& Result) const
{
	const FKPIValue* ExistingValue = Values.Find(Name);

	if (ExistingValue != nullptr)
	{
		Result = *ExistingValue;
		return true;
	}

	return false;
}

const TMap<FName, FKPIValue>& FKPIRegistry::GetKPIValues() const
{
	return Values;
}

const FKPIProfiles& FKPIRegistry::GetKPIProfiles() const
{
	return Profiles;
}

void FKPIRegistry::LoadKPIProfiles(const FString& ProfileSectionName, const FString& FileName)
{
	TArray<FString> SectionNames;

	if (GConfig->GetSectionNames(FileName, SectionNames))
	{
		for (const FString& SectionName : SectionNames)
		{
			if (SectionName.Find(ProfileSectionName) != INDEX_NONE)
			{
				FString ProfileName;
				FString MapName;

				FKPIProfile Profile;

				if (GConfig->GetString(*SectionName, TEXT("ProfileName"), ProfileName, FileName))
				{
					if (GConfig->GetString(*SectionName, TEXT("MapName"), Profile.MapName, FileName))
					{

					}

					for (FKPIValues::TConstIterator It(GetKPIValues()); It; ++It)
					{
						const FKPIValue &KPIValue = It->Value;
						FString KPIName = FString::Printf(TEXT("%s_%s"), *KPIValue.Category.ToString(), *KPIValue.Name.ToString()).Replace(TEXT(" "), TEXT("_"));
					
						float ThresholdValue;

						if (GConfig->GetFloat(*SectionName, *KPIName, ThresholdValue, FileName))
						{
							Profile.Thresholds.Emplace(It->Key, ThresholdValue);
						}
					}

					Profiles.Emplace(ProfileName, Profile);
				}
			}
		}
	}
}

bool FKPIRegistry::ApplyKPIProfile(const FKPIProfile& KPIProfile)
{
	bool Result = true;

	for (FKPIThesholds::TConstIterator It(KPIProfile.Thresholds); It; ++It)
	{
		Result &= SetKPIThreshold(It->Key, It->Value);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

UE_ENABLE_OPTIMIZATION_SHIP
