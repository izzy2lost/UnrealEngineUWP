// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeColour.h"
#include "MuT/Table.h"


namespace mu
{

	// Forward definitions
	class NodeColourTable;
	typedef Ptr<NodeColourTable> NodeColourTablePtr;
	typedef Ptr<const NodeColourTable> NodeColourTablePtrConst;


	//! This node provides the meshes stored in the column of a table.
	//! \ingroup transform
	class MUTABLETOOLS_API NodeColourTable : public NodeColour
	{
	public:

		FString ParameterName;
		Ptr<Table> Table;
		FString ColumnName;
		bool bNoneOption = false;
		FString DefaultRowName;

	public:

		// Node interface
		virtual const FNodeType* GetType() const override { return GetStaticType(); }
		static const FNodeType* GetStaticType() { return &StaticType; }

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Set the name of the implicit table parameter.
		void SetParameterName( const FString& strName );

		//!
		void SetColumn( const FString& strName );

		//!
		void SetNoneOption(bool bAddNoneOption);

		//! Set the row name to be used as default value
		void SetDefaultRowName(const FString RowName);


	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeColourTable() {}

	private:

		static FNodeType StaticType;

	};


}
