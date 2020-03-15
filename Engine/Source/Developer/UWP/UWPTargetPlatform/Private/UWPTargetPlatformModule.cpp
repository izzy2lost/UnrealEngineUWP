#include "UWPTargetDevice.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "UWPTargetPlatform.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FUWPTargetPlatformModule"


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* UWPTargetSingleton = NULL;


/**
 * Module for the UWP target platform.
 */
class FUWPTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/** Default constructor. */
	FUWPTargetPlatformModule( )
	{ }

	/** Destructor. */
	~FUWPTargetPlatformModule( )
	{
	}

public:
	
	// ITargetPlatformModule interface
	
	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (UWPTargetSingleton == NULL)
		{
			//@todo UWP: Check for SDK?

			UWPTargetSingleton = new TUWPTargetPlatform<true>();
		}
		
		return UWPTargetSingleton;
	}
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FUWPTargetPlatformModule, UWPTargetPlatform);


#include "Windows/HideWindowsPlatformTypes.h"
