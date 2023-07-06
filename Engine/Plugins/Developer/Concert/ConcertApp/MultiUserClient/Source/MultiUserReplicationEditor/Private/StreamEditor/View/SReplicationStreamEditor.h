// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UMultiUserReplicationStreamAsset;

namespace UE::MultiUserReplicationEditor
{
	class FEditorObjectSelectionSourceModel;
	class FSelectPropertyFromUClassModel;
	class FPropertySelectionAssetModel;
	class SPropertyReplicationSelectionEditor;
	
	/** Root widget for editing UMultiUserReplicationStreamAsset. */
	class MULTIUSERREPLICATIONEDITOR_API SReplicationStreamEditor : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SReplicationStreamEditor)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UMultiUserReplicationStreamAsset& InEditedAsset);

	private:

		/** Edits the selected properties */
		TSharedPtr<SPropertyReplicationSelectionEditor> PropertySelectionEditor;
		
		/** Passed to SPropertyReplicationSelectionEditor which will read and write to the UMultiUserReplicationStreamAsset */
		TSharedPtr<FPropertySelectionAssetModel> AssetReadWriteModel;
		/** Passed to SPropertyReplicationSelectionEditor. It controls which objects can be selected for the replication list. */
		TSharedPtr<FEditorObjectSelectionSourceModel> ObjectSourceModel;
		/** Passed to SPropertyReplicationSelectionEditor. It controls which properties can be added to objects. */
		TSharedPtr<FSelectPropertyFromUClassModel> PropertySourceModel;
	};
}
