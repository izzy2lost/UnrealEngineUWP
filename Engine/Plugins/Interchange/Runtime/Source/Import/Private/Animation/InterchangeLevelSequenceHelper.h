// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeAnimationTrackSetNode.h"
#include "InterchangeImportLog.h"

#include "UObject/ObjectRedirector.h"

class UMovieScene;
class UMovieSceneSection;

namespace UE::Interchange::Private
{
	/**
	 * Finds a UObject class by name.
	 * @param ClassName		The name of the class to look for (ie:UClass*->GetName()).
	 * @return				A sub class of UObject or nullptr.
	 */
	template<typename T>
	TSubclassOf<T> FindObjectClass(const TCHAR* ClassName)
	{
		if(!ensure(ClassName))
		{
			UE_LOG(LogInterchangeImport, Error, TEXT("Classname is null to find an appropriate animation property track."))
				return nullptr;
		}

		UClass* ExpressionClass = FindFirstObject<UClass>(ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);

		if(!ExpressionClass)
		{
			if(UObjectRedirector* RenamedClassRedirector = FindFirstObject<UObjectRedirector>(ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous))
			{
				ExpressionClass = CastChecked<UClass>(RenamedClassRedirector->DestinationObject);
			}
		}

		if(ExpressionClass && ExpressionClass->IsChildOf<T>())
		{
			return ExpressionClass;
		}
		else
		{
			return nullptr;
		}
	}

	struct FInterchangePropertyTracksHelper
	{
		static FInterchangePropertyTracksHelper& GetInstance();

		UMovieSceneSection* GetSection(UMovieScene* MovieScene, const UInterchangeAnimationTrackNode& AnimationTrackNode, const FGuid& ObjectBinding, EInterchangePropertyTracks Property) const;

	private:
		FInterchangePropertyTracksHelper();

		struct FInterchangeProperty
		{
			FInterchangeProperty(FString&& ClassType, FString&& Path, FString&& Name)
				: ClassType{ MoveTemp(ClassType) }
				, Path{ MoveTemp(Path) }
				, Name{ MoveTemp(Name) }
			{}

			FInterchangeProperty(FString&& ClassType, FString&& Path, FString&& Name, UEnum* EnumClass)
				: ClassType{ MoveTemp(ClassType) }
				, Path{ MoveTemp(Path) }
				, Name{ MoveTemp(Name) }
				, EnumClass{EnumClass}
			{}

			FInterchangeProperty(FString&& ClassType, FString&& Path, FString&& Name, int32 NumChannelsUsed)
				: ClassType{ std::move(ClassType) }
				, Path{ MoveTemp(Path) }
				, Name{ MoveTemp(Name) }
				, NumChannelsUsed{ NumChannelsUsed }
			{}

			FString ClassType; // Float, Double, Byte, etc. Basically the class name of the UMovieSceneTrack
			FString Path;
			FName Name;
			UEnum* EnumClass = nullptr; // Only used for Enum property tracks
			TOptional<int32> NumChannelsUsed; // Only used for Vector property tracks (Vector tracks can only have 2-4 channels)
		};

		TMap<EInterchangePropertyTracks, FInterchangeProperty> PropertyTracks;
	};
}