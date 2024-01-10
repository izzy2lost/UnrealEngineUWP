// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/PredefinedReplicationColumns.h"
#include "Replication/Editor/View/ReplicationColumnsUtils.h"
#include "Templates/SharedPointer.h"

namespace UE::ConcertSharedSlate
{
	class IEditableReplicationStreamModel;
	class IReplicationStreamViewer;
	class IReplicationStreamModel;
	class ISubobjectModel;
}

namespace UE::ConcertSharedSlate::ReplicationColumns::TopLevel
{
	CONCERTSHAREDSLATE_API extern const FName IconColumnId;
	CONCERTSHAREDSLATE_API extern const FName LabelColumnId;
	CONCERTSHAREDSLATE_API extern const FName TypeColumnId;

	enum class ETopLevelColumnOrder : int32
	{
		/** Label of the object */
		Label = 20,
		/** Class of the object */
		Type = 30,
	};

	CONCERTSHAREDSLATE_API FReplicationTopLevelObjectColumn LabelColumn(TSharedRef<IReplicationStreamModel> Model, ISubobjectModel* SubobjectModel = nullptr);
	CONCERTSHAREDSLATE_API FReplicationTopLevelObjectColumn TypeColumn(TSharedRef<IReplicationStreamModel> Model);
}

namespace UE::ConcertSharedSlate::ReplicationColumns::Property
{
	CONCERTSHAREDSLATE_API extern const FName LabelColumnId;
	CONCERTSHAREDSLATE_API extern const FName TypeColumnId;
	
	enum class EReplicationPropertyColumnOrder : int32
	{
		/** Label of the property */
		Label = 10,
		/** Type of the property */
		Type = 20
	};
	
	CONCERTSHAREDSLATE_API FReplicationPropertyColumn LabelColumn();
	CONCERTSHAREDSLATE_API FReplicationPropertyColumn TypeColumn();
}
