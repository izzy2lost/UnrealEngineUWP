// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShowJoinSettingsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Settings/MultiUserReplicationSettings.h"

namespace UE::MultiUserClient
{
	TSharedRef<IDetailCustomization> FShowJoinSettingsCustomization::Make()
	{
		return MakeShared<FShowJoinSettingsCustomization>();
	}

	void FShowJoinSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		TSharedRef<IPropertyHandle> DefaultPropertySelection = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMultiUserReplicationSettings, DefaultPropertySelection));
		TSharedRef<IPropertyHandle> DefaultSessionSettings = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMultiUserReplicationSettings, DefaultSessionSettings));
		
		DetailBuilder.HideCategory("Editor");
		DetailBuilder.HideProperty(DefaultSessionSettings);
		DetailBuilder.HideProperty(DefaultPropertySelection);
		
		TSharedPtr<IPropertyHandle> AutoJoin = DefaultSessionSettings->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDefaultReplicationSessionSettings, bAutoJoin));
		TSharedPtr<IPropertyHandle> DefaultProfile = DefaultSessionSettings->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDefaultReplicationSessionSettings, DefaultProfile));
		check(AutoJoin && DefaultProfile);
		
		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory("Settings");
		CategoryBuilder.AddProperty(AutoJoin);
		CategoryBuilder.AddProperty(DefaultProfile);
	}
}
