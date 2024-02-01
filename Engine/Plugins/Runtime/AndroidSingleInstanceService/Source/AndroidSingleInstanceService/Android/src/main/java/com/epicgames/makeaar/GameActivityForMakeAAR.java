package com.epicgames.makeaar;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;

import java.io.File;
import java.util.Map;
import java.util.HashMap;
import java.util.concurrent.Executor;

import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.FeatureInfo;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.Looper;

import android.os.Handler;

import android.content.ClipboardManager;
import android.content.res.Configuration;
import android.content.pm.PackageManager;
import android.content.pm.PackageInfo;

import android.media.AudioManager;

import android.view.Display;
import android.view.View;
import android.view.Surface;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.epicgames.unreal.GameActivity;
import com.epicgames.unreal.Logger;
import com.epicgames.unreal.SimpleContextWrapper;

public class GameActivityForMakeAAR extends com.epicgames.unreal.GameActivity
{
	//static GameActivityForMakeAAR _gameActivityInstance = null;

	//========== MAKEAAR specifics ====================

	//public SimpleContextWrapper activityContext = null;
	

	String _OBBFilename = null;

	public static com.epicgames.unreal.Logger Log = new Logger("UE_ASIS", "MakeAARBaseForGameActivity");
	
//	String commandLine = "";

	GameActivityForMakeAAR()
	{
		isStandalone = true;
		Log.debug("GameActivityForMakeAAR constructor called with _gameActivityInstance=" + _gameActivityInstance + ", activityContext=" + activityContext + ", this=" + this + ", isStandalone=" + isStandalone);
		_gameActivityInstance = this;
		//activityContext = new SimpleContextWrapper(this);
	}

	public static GameActivityForMakeAAR Instance()
	{
		if (_gameActivityInstance != null)
		{
			Log.error("GameActivityForMakeAAR.Instance() called but _gameActivityInstance already exists!!! in current thread= " + Thread.currentThread() + ", _gameActivityInstance=" + _gameActivityInstance);
			
		}
		else
		{
			GameActivityForMakeAAR newInstance = new GameActivityForMakeAAR();
			
			Log.debug("GameActivityForMakeAAR::Instance() constructor called with prev _gameActivityInstance=" + _gameActivityInstance + ", newInstance.activityContext=" + newInstance.activityContext + ", newInstance=" + newInstance + ", newInstance.isStandalone=" + newInstance.isStandalone);
			_gameActivityInstance = newInstance;
		}

		assert( _gameActivityInstance instanceof GameActivityForMakeAAR);

		return (GameActivityForMakeAAR)_gameActivityInstance;
	}

	public static GameActivityForMakeAAR Create()
	{
		return Instance();
	}
	
	@Override
	public  @Nullable Object getSystemService(@NonNull String name) 
	{
		return GetCurrentActivityContext().getSystemService(name);
	}

	@Override
	public Resources getResources()
	{
		return GetCurrentActivityContext().getResources();
	}

	@Override
    public @Nullable File getExternalFilesDir(@Nullable String type) 
	{
		return GetCurrentActivityContext().getExternalFilesDir(type);
    }

	@Override
	public File getFilesDir()
	{
		return GetCurrentActivityContext().getFilesDir();
	}

	@Override
	public String getPackageName()
	{
		return GetCurrentActivityContext().getPackageName();
	}

	@Override
	public String getPackageResourcePath()
	{
		return GetCurrentActivityContext().getPackageResourcePath();
	}

	@Override
	public Executor getMainExecutor()
	{
		return GetCurrentActivityContext().getMainExecutor();
	}

	@Override
	public @Nullable Display getDisplay()
	{
		return GetCurrentActivityContext().getDisplay();
	}

	@Override
	public PackageManager getPackageManager()
	{
		return GetCurrentActivityContext().getPackageManager();
	}

	@Override
	public Context getApplicationContext()
	{
		return GetCurrentActivityContext().getApplicationContext();
	}



	void setCommandline(String inCommandline)
	{
		nativeSetCommandline(inCommandline);
	}

	public static GameActivityForMakeAAR Get()
	{
		return (GameActivityForMakeAAR)_gameActivityInstance;
	}

	@Override
	protected void RestartApplication(String RestartExtra)
	{
		super.RestartApplication(RestartExtra);
		//Context context = activityContext.getApplicationContext();
		Log.warn("GameActivityForMakeAAR.RestartApplication() in current thread= " + Thread.currentThread() + ", _gameActivityInstance=" + _gameActivityInstance);
	}


	// ========	override defined in:	AndroidJNI.cpp
	public native void nativeSetGlobalActivity(boolean bUseExternalFilesDir, boolean bPublicLogFiles, String internalFilePath, String externalFilePath, boolean bOBBInAPK, String APKPath);

	// ========	override defined in:	AndroidWindow.cpp
	public native static void nativeSetSurfaceOverride(Surface surface, int x, int y);

	// ========	override defined in:	AndroidPlatformFile.cpp
	public native static String nativeGetObbComment();


	// ========	MakeAAR specific defined in:	LaunchAndroid.cpp
	public native static void nativeMain(String projectModule);
	public native static void nativeSetCommandline(String commandLine);
	public native static void nativeAppCommand(int cmd);
	public native static int nativeInputTouch(int device, int action, int pointerId, int x, int y);


	// ========            MAKEAAR specific non-native functions			===========
	
	public boolean setActivity( Activity inActivity, String OBBFilename, boolean enablePropagateAlpha) {
		return setActivity(new SimpleContextWrapper(inActivity), OBBFilename, enablePropagateAlpha);
	}

	public boolean setActivity(SimpleContextWrapper inActivity, String OBBFilename, boolean enablePropagateAlpha) {


		Log.verbose( "**LIFECYCLE** GameActivity::setActivity(with OBBFilename) inActivity=" + inActivity + ", _activity=" + activityContext + ", obb=" + OBBFilename);

		if (activityContext != null)
		{
			// we need to shut down the old activity
			Log.debug( "**IMPORTANT** GameActivity::setActivity(with OBBFilename) change inActivity=" + inActivity + ", _activity=" + activityContext);
			Log.debug( "**TODO** cleanup old activity");

		}

		activityContext = inActivity;
		
		if (activityContext == null)
		{
			Log.error( "GameActivity::setActivity(with OBBFilename) _activity is null" );
			return false;
		}

		clipboardManager = (ClipboardManager) activityContext.getSystemService(CLIPBOARD_SERVICE);

		// Grab a reference to the asset manager
		AssetManagerReference = activityContext.getAssets();

		InternalFilesDir = activityContext.getFilesDir().getAbsolutePath() + "/";
		File _externalFilesDir = activityContext.getExternalFilesDir(null);
		ExternalFilesDir = (_externalFilesDir != null ? _externalFilesDir.getAbsolutePath() : "") + "/";
		
		_extrasBundle = activityContext.mAppActivityContext != null && activityContext.mAppActivityContext instanceof Activity ? ((Activity)activityContext.mAppActivityContext).getIntent().getExtras() : null;
		appPackageName = activityContext.getPackageName();
		String ProjectName = appPackageName;
		ProjectName = ProjectName.substring(ProjectName.lastIndexOf('.') + 1);
		Log.verbose("from SetActivity(with OBBFilename), appPackageName: " + appPackageName + ", ProjectName = " + ProjectName);

		String AppType = "";
		try
		{
			ApplicationInfo ai = activityContext.getPackageManager().getApplicationInfo(appPackageName, PackageManager.GET_META_DATA);
			Bundle bundle = ai.metaData;
			_bundle = bundle;

			if ((ai.flags & ApplicationInfo.FLAG_DEBUGGABLE) == 0) {
				IsForDistribution = true;
			}
		}
		catch (PackageManager.NameNotFoundException e)
		{
			Log.debug( "Failed to load meta-data: NameNotFound: " + e.getMessage());
		}
		catch (NullPointerException e)
		{
			Log.debug( "Failed to load meta-data: NullPointer: " + e.getMessage());
		}

		// Look for Vulkan support if Nougat or later
		if (ANDROID_BUILD_VERSION >= 24)
		{
			FeatureInfo[] features = activityContext.getPackageManager().getSystemAvailableFeatures();
			for (FeatureInfo feature : features) {
				if (feature.name != null)
				{
					if (feature.name.equals("android.hardware.vulkan.level"))
					{
						// since we may not be compiled against android-24 or higher, use .toString to get the version field
						String dump = feature.toString();
						int index = dump.indexOf("v=");
						if (index >= 0)
						{
							dump = dump.substring(index+2);
							index = dump.indexOf(" ");
							if (index >= 0)
							{
								VulkanLevel = Integer.parseInt(dump.substring(0, index));
								Log.debug("Vulkan level: " + VulkanLevel);
							}
						}
					}
					else
					if (feature.name.equals("android.hardware.vulkan.version"))
					{
						// since we may not be compiled against android-24 or higher, use .toString to get the version field
						String dump = feature.toString();
						int index = dump.indexOf("v=");
						if (index >= 0)
						{
							dump = dump.substring(index+2);
							index = dump.indexOf(" ");
							if (index >= 0)
							{
								VulkanVersion = Integer.parseInt(dump.substring(0, index));
								int VersionMajor = (VulkanVersion >> 22) & 0x03ff;
								int VersionMinor = (VulkanVersion >> 12) & 0x03ff;
								int VersionPatch = VulkanVersion & 0x0fff;
								VulkanVersionString = VersionMajor + "." + VersionMinor + "." + VersionPatch;
								Log.debug("SetActivity: Vulkan version: " + VersionMajor + "." + VersionMinor + "." + VersionPatch);
							}
						}
					}
				}
			}
		}

		boolean bDebuggerAttached = android.os.Debug.isDebuggerConnected();
		nativeSetAndroidStartupState(bDebuggerAttached);

		_OBBFilename = OBBFilename;

		GameActivity.setOBBInAPK(OBBFilename.equals(""));
		Log.debug("APK path: " + activityContext.getPackageResourcePath());
		Log.debug("OBB in APK: " + (PackageDataInsideApkValue==1));
		boolean UseExternalFilesDir = false;
		boolean PublicLogFiles = false;
		int DepthBufferPreference = 32;
		int PropagateAlpha = enablePropagateAlpha ? 1 : 0;

		Log.debug("SetActivity(with OBBFilename) ExternalFilesDir path: " + ExternalFilesDir);
		Log.debug("activityContext.getExternalFilesDir(null).getPath(): " + activityContext.getExternalFilesDir(null).getPath());

		nativeSetGlobalActivity(UseExternalFilesDir, PublicLogFiles, activityContext.getFilesDir().getPath(), activityContext.getExternalFilesDir(null).getPath(), PackageDataInsideApkValue==1, activityContext.getPackageResourcePath());
		if (PackageDataInsideApkValue == 0)
		{
			Log.debug("OBB override: " + OBBFilename);
			nativeSetObbFilePaths(OBBFilename, "", "", "");
		}

		// tell the engine if this is a portrait app and if it will propagate alpha
		boolean bPortrait = activityContext.getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
		nativeSetWindowInfo(bPortrait, DepthBufferPreference, PropagateAlpha);

		// get the full language code, like en-US
		// note: this may need to be Locale.getDefault().getLanguage()
		String Language = java.util.Locale.getDefault().toString();

		// Retrieve version code, name, and target SDK
		try 
		{
			PackageInfo packageInfo = activityContext.getPackageManager().getPackageInfo(activityContext.getPackageName(), 0);
			VersionCode = packageInfo.versionCode;
			VersionName = packageInfo.versionName;
			targetSdkVersion = packageInfo.applicationInfo.targetSdkVersion;
		}
		catch (Exception e)
		{
			Log.debug("Error accessing packageInfo: " + e.getMessage());
		}

		String productName = getProductName();
		Log.debug("Android version is " + android.os.Build.VERSION.RELEASE);
		Log.debug("Android manufacturer is " + android.os.Build.MANUFACTURER);
		Log.debug("Android model is " + android.os.Build.MODEL);
		Log.debug("Android build number is " + android.os.Build.DISPLAY);
		Log.debug("OS language is set to " + Language);
		Log.debug("Product name is " + (productName.isEmpty() ? "[not set]" : productName));
		Log.debug("Debugger attached is " + bDebuggerAttached);
		Log.debug("Android targetSdkVersion version is " + targetSdkVersion);

		nativeSetAndroidVersionInformation(android.os.Build.VERSION.RELEASE, targetSdkVersion, android.os.Build.MANUFACTURER, android.os.Build.MODEL, android.os.Build.DISPLAY, Language, productName);


		try
		{
			VersionCode = activityContext.getPackageManager().getPackageInfo(appPackageName, 0).versionCode;
			int PatchVersion = 0;
			nativeSetObbInfo(ProjectName, activityContext.getApplicationContext().getPackageName(), VersionCode, PatchVersion, AppType);
		}
		catch (Exception e)
		{
			// if the above failed, then, we can't use obbs
			Log.debug("==================================> PackageInfo failure getting .obb info: " + e.getMessage());
		}

		// tell Android that we want volume controls to change the media volume, aka music
		// enable the physical volume controls to the game
		if (inActivity != null && activityContext.mAppActivityContext != null && activityContext.mAppActivityContext instanceof Activity) {
			((Activity)activityContext.mAppActivityContext).setVolumeControlStream(AudioManager.STREAM_MUSIC);
		}

		// store the screen resolution (adjusted for immersive mode)
		Map<String, String> variables = new HashMap<String, String>();
		android.view.Display display = activityContext.getWindowManager().getDefaultDisplay();
		android.graphics.Point displaySize = new android.graphics.Point();

		// check if immersive mode on window
//		View decorView = activityContext.getWindow().getDecorView();
//		if ((decorView.getSysteUiVisibility() & View.System_UI_FLAG_IMMERSIVE_STICKY) != 0)
		{
			display.getRealSize(displaySize);
		}
//		else
//		{
//			display.getSize(displaySize);
//		}
		variables.put("screenWidth", Integer.toString(displaySize.x));
		variables.put("screenHeight", Integer.toString(displaySize.y));

		// send the variables to the engine
		String[] keyValues = new String[variables.size() * 2];
		int keyValueIndex = 0;
		for (Map.Entry<String, String> entry : variables.entrySet())
		{
			keyValues[keyValueIndex++] = entry.getKey();
			keyValues[keyValueIndex++] = entry.getValue();
		}
		GameActivity.Get().nativeSetConfigRulesVariables(keyValues);

		return true;
	}

	public boolean setActivity(SimpleContextWrapper inActivity, GameActivitySetupInfo gameActivitySetupInfo)
	{

		// pre super.onCreate {
		//

		if (activityContext != null)
		{
			// we need to shut down the old activity
			Log.error( "**IMPORTANT** GameActivity::setActivity(with gameActivitySetupInfo) change inActivity=" + inActivity + ", activityContext=" + activityContext );
			Log.error( "**TODO** cleanup old activity");
		}

		activityContext = inActivity;
		Log.verbose( "**LIFECYCLE** GameActivity::setActivity(with gameActivitySetupInfo) inActivity=" + inActivity + ", activityContext=" + activityContext);


		if (activityContext == null)
		{
			Log.error( "GameActivity::setActivity(with gameActivitySetupInfo) activityContext is null" );
		}
		

		assert(_gameActivityInstance != null);
		bOnCreateCalled = true;

		InternalFilesDir = gameActivitySetupInfo.InternalFilesDir;
		ExternalFilesDir = gameActivitySetupInfo.ExternalFilesDir;
//$${gameActivityOnCreateBeginningAdditions}$$
		Logger.RegisterCallback(this);

		// Grab a reference to the asset manager
		AssetManagerReference = gameActivitySetupInfo.AssetManagerReference;

	//
	//} pre super.onCreate
	//

		Bundle savedInstanceState = new Bundle();
		onCreateBody(savedInstanceState);

		
		clipboardManager = gameActivitySetupInfo.clipboardManager;

		_extrasBundle = gameActivitySetupInfo._extrasBundle;

		if (this._extrasBundle == null)
		{
			Log.error("=====> PackageInfo _extrasBundle so no renderSurface is passed!" );
		}
		else if (gameActivitySetupInfo.renderSurface == null)
		{
			gameActivitySetupInfo.renderSurface = this._extrasBundle == null ? null : this._extrasBundle.<Surface>getParcelable("renderSurface");
			Log.warn("====> GameActivitySetupInfo(GET) is storing renderSurface = " + gameActivitySetupInfo.renderSurface);
		}
		else
		{
			Log.debug("====> GameActivitySetupInfo(GET) is has renderSurface = " + gameActivitySetupInfo.renderSurface);

		}

		appPackageName = gameActivitySetupInfo.appPackageName;

		String ProjectName = appPackageName;
		ProjectName = gameActivitySetupInfo.ProjectName.isEmpty() ? ProjectName.substring(ProjectName.lastIndexOf('.') + 1) : gameActivitySetupInfo.ProjectName;
		String AppType = "";
		Log.verbose("from gameActivitySetupInfo, appPackageName: " + appPackageName + ", ProjectName = " + ProjectName);

		_bundle = gameActivitySetupInfo._bundle;
		IsForDistribution = gameActivitySetupInfo.IsForDistribution;


		// Look for Vulkan support if Nougat or later
		gameActivitySetupInfo.GetVulkanInfoFromPackage(this);

		boolean bDebuggerAttached = android.os.Debug.isDebuggerConnected();
		nativeSetAndroidStartupState(bDebuggerAttached);

		_OBBFilename = gameActivitySetupInfo._OBBFilename;

		PropagateAlpha = gameActivitySetupInfo.PropagateAlpha;

		GameActivity.setOBBInAPK(_OBBFilename.equals(""));

		Log.debug("APK path: " + activityContext.getPackageResourcePath());
		Log.debug("OBB in APK: " + (PackageDataInsideApkValue==1));
		boolean UseExternalFilesDir = false;
		boolean PublicLogFiles = false;
		int DepthBufferPreference = 32;

		Log.debug("SetActivity(with gameActivitySetupInfo) ExternalFilesDir path: " + ExternalFilesDir);
		Log.debug("activityContext.getExternalFilesDir(null).getPath(): " + activityContext.getExternalFilesDir(null).getPath());

		nativeSetGlobalActivity(UseExternalFilesDir, PublicLogFiles, activityContext.getFilesDir().getPath(), activityContext.getExternalFilesDir(null).getPath(), PackageDataInsideApkValue==1, activityContext.getPackageResourcePath());
		if (PackageDataInsideApkValue == 0)
		{
			Log.debug("OBB override: " + _OBBFilename);
			nativeSetObbFilePaths(_OBBFilename, "", "", "");
		}

		// tell the engine if this is a portrait app and if it will propagate alpha
		boolean bPortrait = activityContext.getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
		nativeSetWindowInfo(bPortrait, DepthBufferPreference, PropagateAlpha);

		// get the full language code, like en-US
		// note: this may need to be Locale.getDefault().getLanguage()
		String Language = java.util.Locale.getDefault().toString();

		// Retrieve version code, name, and target SDK
		try
		{
			PackageInfo packageInfo = activityContext.getPackageManager().getPackageInfo(activityContext.getPackageName(), 0);
			VersionCode = packageInfo.versionCode;
			VersionName = packageInfo.versionName;
			targetSdkVersion = packageInfo.applicationInfo.targetSdkVersion;
		}
		catch (Exception e)
		{
			Log.debug("Error accessing packageInfo: " + e.getMessage());
		}

		String productName = getProductName();
		Log.debug( "Android version is " + android.os.Build.VERSION.RELEASE );
		Log.debug( "Android manufacturer is " + android.os.Build.MANUFACTURER );
		Log.debug( "Android model is " + android.os.Build.MODEL );
		Log.debug( "Android build number is " + android.os.Build.DISPLAY );
		Log.debug( "OS language is set to " + Language);
		Log.debug( "Product name is " + (productName.isEmpty() ? "[not set]" : productName));
		Log.debug( "Debugger attached is " + bDebuggerAttached );
		Log.debug( "Android targetSdkVersion version is " + targetSdkVersion );
		
		nativeSetAndroidVersionInformation(android.os.Build.VERSION.RELEASE, targetSdkVersion, android.os.Build.MANUFACTURER, android.os.Build.MODEL, android.os.Build.DISPLAY, Language, productName);


		try
		{
			Log.debug( "do nativeSetObbInfo ProjectName=" + ProjectName + ", PackageName=" + appPackageName );

			//VersionCode = activityContext.getPackageManager().getPackageInfo(appPackageName, 0).versionCode;
			VersionCode = gameActivitySetupInfo.VersionCode;
			int PatchVersion = 0;
			//nativeSetObbInfo(ProjectName, activityContext.getApplicationContext().getPackageName(), VersionCode, PatchVersion, AppType);
			nativeSetObbInfo(ProjectName, gameActivitySetupInfo.appPackageName, VersionCode, PatchVersion, AppType);
		}
		catch (Exception e)
		{
			// if the above failed, then, we can't use obbs
			Log.debug("==================================> PackageInfo failure getting .obb info: " + e.getMessage());
		}
		this._extrasBundle = activityContext.mAppActivityContext != null && activityContext.mAppActivityContext instanceof Activity ? ((Activity)activityContext.mAppActivityContext).getIntent().getExtras() : null;

		// tell Android that we want volume controls to change the media volume, aka music
		// enable the physical volume controls to the game
		if (inActivity != null && activityContext.mAppActivityContext != null && activityContext.mAppActivityContext instanceof Activity) {
			 ((Activity)activityContext.mAppActivityContext).setVolumeControlStream(AudioManager.STREAM_MUSIC);
		}

		String[] keyValues = gameActivitySetupInfo.keyValues;

		if (keyValues.length == 0) {
			// store the screen resolution (adjusted for immersive mode)
			Map<String, String> variables = new HashMap<String, String>();
			android.view.Display display = activityContext.getWindowManager().getDefaultDisplay();
			android.graphics.Point displaySize = new android.graphics.Point();

			// check if immersive mode on window
			View decorView = activityContext.getWindow().getDecorView();
	//		if ((decorView.getSysteUiVisibility() & View.System_UI_FLAG_IMMERSIVE_STICKY) != 0)
			{
				display.getRealSize(displaySize);
			}
	//		else
	//		{
	//			display.getSize(displaySize);
	//		}
			variables.put("screenWidth", Integer.toString(displaySize.x));
			variables.put("screenHeight", Integer.toString(displaySize.y));

			// send the variables to the engine
			keyValues = new String[variables.size() * 2];
			int keyValueIndex = 0;
			for (Map.Entry<String, String> entry : variables.entrySet())
			{
				keyValues[keyValueIndex++] = entry.getKey();
				keyValues[keyValueIndex++] = entry.getValue();
			}
		}

		nativeSetConfigRulesVariables(keyValues);

		return true;
	}

//	public void GameActivityForMakeAAR(Activity inActivity, String OBBFilename, boolean enablePropagateAlpha)
//	{
//		gameActivity = this;
//
//		//SimpleContextWrapper inActivity = SimpleContextWrapper.getSimpleActivityOfContext(inContext);
//		//Activity inActivity = SimpleContextWrapper.getActivityOfContext(inContext);
//		
//		setActivity(inActivity, OBBFilename, enablePropagateAlpha);
//	}
//
//	public void GameActivityForMakeAAR(Context context, GameActivitySetupInfo gameActivitySetupInfo)
//	{
//		gameActivity = this;
//		activityContext = new SimpleContextWrapper(context);
//		setActivity(activityContext, gameActivitySetupInfo);
//	}
//
//	public void GameActivityForMakeAAR(GameActivitySetupInfo gameActivitySetupInfo)
//	{
//		gameActivity = this;
//		initFromData(gameActivitySetupInfo);
//	}

	void initFromData(GameActivitySetupInfo gameActivitySetupInfo)
	{

		AssetManagerReference = gameActivitySetupInfo.AssetManagerReference;
		clipboardManager = gameActivitySetupInfo.clipboardManager;

		//Activity _activity = null;
		_OBBFilename = gameActivitySetupInfo._OBBFilename;
		PropagateAlpha = gameActivitySetupInfo.PropagateAlpha;
		_bundle = gameActivitySetupInfo._bundle;
		_extrasBundle = gameActivitySetupInfo._extrasBundle;
		PackageDataInsideApkValue = gameActivitySetupInfo.packageDataInsideApkValue ? 1 : 0;
		InternalFilesDir = gameActivitySetupInfo.InternalFilesDir;
		ExternalFilesDir = gameActivitySetupInfo.ExternalFilesDir;
		appPackageName = gameActivitySetupInfo.appPackageName;
		VersionCode = gameActivitySetupInfo.VersionCode;
		//---- Discovered Vulkan Version and Level from getSystemAvailableFeatures() ---//
		gameActivitySetupInfo.GetVulkanInfoFromPackage(this);

	}

	@Override
	public void onPause()
	{
		super.onPause();
	}
}