// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShowJoinSettingsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Settings/MultiUserReplicationSettings.h"
#include "UObject/PropertyIterator.h"

namespace UE::MultiUserClient
{
	TSharedRef<IDetailCustomization> FShowJoinSettingsCustomization::Make()
	{
		return MakeShared<FShowJoinSettingsCustomization>();
	}

	void FShowJoinSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		for (TFieldIterator<FProperty> FieldIt(UMultiUserReplicationSettings::StaticClass()); FieldIt; ++FieldIt)
		{
			DetailBuilder.HideProperty(DetailBuilder.GetProperty(FieldIt->GetFName()));
		}
		
		TSharedRef<IPropertyHandle> DefaultSessionSettings = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMultiUserReplicationSettings, DefaultSessionSettings));
		TSharedPtr<IPropertyHandle> AutoJoin = DefaultSessionSettings->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDefaultReplicationSessionSettings, bAutoJoin));
		TSharedPtr<IPropertyHandle> DefaultProfile = DefaultSessionSettings->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDefaultReplicationSessionSettings, DefaultProfile));
		check(AutoJoin && DefaultProfile);
		
		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory("Settings");
		CategoryBuilder.AddProperty(AutoJoin);
		CategoryBuilder.AddProperty(DefaultProfile);
	}
}
