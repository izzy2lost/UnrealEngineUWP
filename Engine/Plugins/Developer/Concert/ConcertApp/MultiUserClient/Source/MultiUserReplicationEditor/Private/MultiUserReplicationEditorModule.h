// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMultiUserReplicationEditorModule.h"
#include "Misc/AssetCategoryPath.h"

namespace UE::MultiUserReplicationEditor
{
	class FMultiUserReplicationEditorModule : public IMultiUserReplicationEditorModule
	{
	public:

		FMultiUserReplicationEditorModule();

		//~ Begin IModuleInterface Interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface Interface

		//~ Begin IMultiUserReplicationEditor Interface
		virtual FAssetCategoryPath GetMultiUserReplicationCategory() const override { return MultiUserReplicationCategory; }
		//~ End IMultiUserReplicationEditor Interface

	private:

		FAssetCategoryPath MultiUserReplicationCategory;

		void UpdateSettingsRegistrationBasedOnCVar();
		void RegisterSettings();
		void UnregisterSettings();
	};
}
