// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ITraceToolsModule.h: Declares the ITraceToolsModule interface.
=============================================================================*/

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class ITraceController;
class SWidget;

namespace UE::TraceTools
{

/**
 * Interface for a trace tools module.
 */
class ITraceToolsModule
	: public IModuleInterface
{
public:
	
	/**
	 * Creates a trace control widget.
	 *
	 * @return The new widget
	 */
	virtual TSharedRef<SWidget> CreateTraceControlWidget(TSharedPtr<ITraceController> InTraceController) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~ITraceToolsModule( ) { }
};

} // namespace UE::TraceTools
