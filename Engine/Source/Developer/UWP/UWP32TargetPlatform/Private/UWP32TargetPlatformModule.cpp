#include "UWPTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* UWP32TargetSingleton = NULL;


/**
 * Module for the UWP64 target platform.
 */
class FUWP32TargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/** Default constructor. */
	FUWP32TargetPlatformModule( )
	{ }

	/** Destructor. */
	~FUWP32TargetPlatformModule( )
	{
	}

public:
	
	// ITargetPlatformModule interface
	
	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (UWP32TargetSingleton == NULL)
		{
			//@todo UWP: Check for SDK?

			UWP32TargetSingleton = new TUWPTargetPlatform<false>();
		}
		
		return UWP32TargetSingleton;
	}
};


IMPLEMENT_MODULE(FUWP32TargetPlatformModule, UWP32TargetPlatform);
