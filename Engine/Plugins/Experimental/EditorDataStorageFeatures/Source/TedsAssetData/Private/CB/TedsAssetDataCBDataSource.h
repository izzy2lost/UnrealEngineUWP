// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementCommonTypes.h"
#include "Elements/Common/TypedElementHandles.h"

#include "Containers/StringFwd.h"
#include "Interfaces/IPluginManager.h"

class IPlugin;
class ITypedElementDataStorageInterface;

namespace UE::Editor
{
	namespace AssetData::Private
	{

		class FTedsAssetDataCBDataSource
		{
		public:
			explicit FTedsAssetDataCBDataSource(ITypedElementDataStorageInterface& InDatabase);
			~FTedsAssetDataCBDataSource();

		private:
			ITypedElementDataStorageInterface& Database;
			DataStorage::QueryHandle ProcessPathQuery;
			DataStorage::QueryHandle ProcessAssetDataPathUpdateQuery;
			DataStorage::QueryHandle ProcessAssetDataAndPathUpdateQuery;
			DataStorage::QueryHandle ProcessAssetDataUpdateQuery;

			struct FVirtualPathProcessor
			{
				struct FCachedPluginData
				{
					EPluginLoadedFrom LoadedFrom;
					FString EditorCustomVirtualPath;
				};

				TMap<FString, FCachedPluginData> PluginNameToCachedData;

				bool bShowAllFolder = false;
				bool bOrganizeFolders = false;

				void ConvertInternalPathToVirtualPath(const FStringView InternalPath, FStringBuilderBase& OutVirtualPath) const;
			};

			FVirtualPathProcessor VirtualPathProcessor;

			void InitVirtualPathProcessor();

			void OnPluginContentMounted(IPlugin& InPlugin);
			void OnPluginUnmounted(IPlugin& InPlugin);
		};

	} // namespace AssetData::Private
} // End of namespace UE::Editor
