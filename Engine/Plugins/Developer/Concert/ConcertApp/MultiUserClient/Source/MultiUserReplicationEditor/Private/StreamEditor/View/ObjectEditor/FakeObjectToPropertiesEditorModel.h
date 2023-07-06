// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StreamEditor/Model/IObjectToPropertiesModel.h"

namespace UE::MultiUserReplicationEditor
{
	class IPropertySelectionSourceModel;
	/**
	 * This model is used by SPropertyReplicationSelectionEditor.
	 * 
	 * It forwards most functions except for ForEachProperty, which fakes to the SPropertyReplicationSelectionView
	 * that all properties are selected. The properties are added to the real model via checking and unchecking the box.
	 * 
	 * This allows the SPropertyReplicationSelectionView to continue to show the properties it is being told are in the model,
	 * which is important e.g. for the connection view (when a client is connected to a session and views the properties being sent).
	 */
	class FFakeObjectToPropertiesEditorModel : public IObjectToPropertiesModel
	{
	public:

		FFakeObjectToPropertiesEditorModel(TSharedRef<IObjectToPropertiesModel> RealModel, TSharedRef<IPropertySelectionSourceModel> PropertySelectionSource)
			: RealModel(MoveTemp(RealModel))
			, PropertySelectionSource(MoveTemp(PropertySelectionSource))
		{}

		//~ Begin IObjectToPropertiesModel Interface
		virtual uint32 GetNumReplicatedObjects() const override { return RealModel->GetNumReplicatedObjects(); }
		virtual uint32 GetNumProperties(const FSoftObjectPath& Object) const override { return RealModel->GetNumProperties(Object); }
		virtual FSoftClassPath GetObjectClass(const FSoftObjectPath& Object) const override { return RealModel->GetObjectClass(Object); }
		virtual bool ContainsObjects(const TSet<FSoftObjectPath>& Objects) const override { return RealModel->ContainsObjects(Objects); }
		// This function is by SPropertyReplicationSelectionEditor and not used by SPropertyReplicationSelectionView so forward it normally
		virtual bool ContainsProperties(const FSoftObjectPath& Object, const TSet<FConcertPropertyChain>& Properties) const override { return RealModel->ContainsProperties(Object, Properties); }
		virtual bool ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const override { return RealModel->ForEachReplicatedObject(Delegate); }
		virtual bool ForEachProperty(const FSoftObjectPath& Object, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const override;
		//~ End IObjectToPropertiesModel Interface

	private:

		/** The real model which is used to implement all the other functions. */
		TSharedRef<IObjectToPropertiesModel> RealModel;

		/** Determines the properties that can be selected. */
		TSharedRef<IPropertySelectionSourceModel> PropertySelectionSource;
	};
}

