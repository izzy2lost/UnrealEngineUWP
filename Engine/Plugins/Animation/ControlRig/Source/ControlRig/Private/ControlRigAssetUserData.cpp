// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigAssetUserData.h"

void UControlRigShapeLibraryLink::SetShapeLibrary(TSoftObjectPtr<UControlRigShapeLibrary> InShapeLibrary)
{
	InvalidateCache();
	ShapeLibrary = InShapeLibrary;
	ShapeNames.Reset();
	if(ShapeLibrary.IsValid())
	{
		for(const FControlRigShapeDefinition& Shape : ShapeLibrary->Shapes)
		{
			ShapeNames.Add(Shape.ShapeName);
		}
	}
}

const UNameSpacedUserData::FUserData* UControlRigShapeLibraryLink::GetUserData(const FString& InPath, FString* OutErrorMessage) const
{
	// rely on super to look it up from the cache
	if(const FUserData* ResultFromSuper = Super::GetUserData(InPath))
	{
		return ResultFromSuper;
	}

	if(const UControlRigShapeLibrary* LoadedShapeLibrary = ShapeLibrary.LoadSynchronous())
	{
		if(InPath.Equals(GET_MEMBER_NAME_STRING_CHECKED(UControlRigShapeLibraryLink, ShapeLibrary), ESearchCase::CaseSensitive))
		{
			static const FProperty* ShapeLibraryProperty = FindPropertyByName(StaticClass(), GET_MEMBER_NAME_CHECKED(UControlRigShapeLibraryLink, ShapeLibrary));
			return StoreCacheForUserData({InPath, ShapeLibraryProperty, reinterpret_cast<const uint8*>(LoadedShapeLibrary)});
		}
		if(InPath.Equals(DefaultShapePath, ESearchCase::CaseSensitive))
		{
			static const FProperty* DefaultShapeProperty = FindPropertyByName(FControlRigShapeDefinition::StaticStruct(), GET_MEMBER_NAME_CHECKED(FControlRigShapeDefinition, ShapeName));
			return StoreCacheForUserData({InPath, DefaultShapeProperty, reinterpret_cast<const uint8*>(&LoadedShapeLibrary->DefaultShape.ShapeName)});
		}
		if(InPath.Equals(ShapeNamesPath, ESearchCase::CaseSensitive))
		{
			static const FProperty* ShapeNamesProperty = FindPropertyByName(StaticClass(), GET_MEMBER_NAME_CHECKED(UControlRigShapeLibraryLink, ShapeNames));
			return StoreCacheForUserData({InPath, ShapeNamesProperty, reinterpret_cast<const uint8*>(&ShapeNames)});
		}
		if(OutErrorMessage && OutErrorMessage->IsEmpty())
		{
			(*OutErrorMessage) = FString::Printf(PathNotFoundFormat, *InPath);
		}
		return nullptr;
	}

	if(OutErrorMessage && OutErrorMessage->IsEmpty())
	{
		(*OutErrorMessage) = FString::Printf(ShapeLibraryNullFormat, *InPath);
	}
	return nullptr;
}

const TArray<const UNameSpacedUserData::FUserData*>& UControlRigShapeLibraryLink::GetUserDataArray(const FString& InParentPath, FString* OutErrorMessage) const
{
	const TArray<const FUserData*>& ResultFromSuper = Super::GetUserDataArray(InParentPath);
	if(!ResultFromSuper.IsEmpty())
	{
		return ResultFromSuper;
	}

	if(!ShapeLibrary.IsNull())
	{
		// UControlRigShapeLibraryLink doesn't offer any arrays other than the top level 
		if(InParentPath.IsEmpty())
		{
			const FUserData* ShapeLibraryUserData = GetUserData(GET_MEMBER_NAME_STRING_CHECKED(UControlRigShapeLibraryLink, ShapeLibrary), OutErrorMessage);
			const FUserData* DefaultShapeUserData = GetUserData(DefaultShapePath, OutErrorMessage);
			const FUserData* ShapeNamesUserData = GetUserData(ShapeNamesPath, OutErrorMessage);

			if (ensure(ShapeLibraryUserData) &&
				ensure(DefaultShapeUserData) &&
				ensure(ShapeNamesUserData))
			{
				return StoreCacheForUserDataArray(InParentPath, {ShapeLibraryUserData, DefaultShapeUserData, ShapeNamesUserData});
			}
		}
		return EmptyUserDatas;
	}

	if(OutErrorMessage && OutErrorMessage->IsEmpty())
	{
		(*OutErrorMessage) = FString::Printf(ShapeLibraryNullFormat, *InParentPath);
	}
	return EmptyUserDatas;
}

#if WITH_EDITOR

void UControlRigShapeLibraryLink::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FProperty* ShapeLibraryProperty = FindPropertyByName(GetClass(), GET_MEMBER_NAME_CHECKED(UControlRigShapeLibraryLink, ShapeLibrary));
	if(PropertyChangedEvent.Property == ShapeLibraryProperty ||
		PropertyChangedEvent.MemberProperty == ShapeLibraryProperty)
	{
		SetShapeLibrary(ShapeLibrary.Get());
	}
}

#endif
