#include "UWPTargetDevice.h"
#include "ITargetPlatformModule.h"
#include "UWPTargetPlatform.h"
#include "AllowWindowsPlatformTypes.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"
#include "Package.h"

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


#include "HideWindowsPlatformTypes.h"
