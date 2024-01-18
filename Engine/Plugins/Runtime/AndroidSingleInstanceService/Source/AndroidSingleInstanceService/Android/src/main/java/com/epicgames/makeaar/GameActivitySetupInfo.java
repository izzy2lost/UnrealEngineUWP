package com.epicgames.makeaar;

import android.app.Activity;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.Surface;
import android.content.pm.PackageManager;
import android.os.Parcel;
import android.os.Parcelable;

import java.io.File;
import java.util.Map;
import java.util.HashMap;

import com.epicgames.unreal.Logger;

import static android.content.Context.CLIPBOARD_SERVICE;
import static android.content.Context.WINDOW_SERVICE;

import com.epicgames.unreal.SimpleContextWrapper;

//import androidx.annotation.RequiresApi;

public class GameActivitySetupInfo implements Parcelable {

	public static Logger Log = new Logger("UES", "GameActivitySetupInfo");

	public static final int MSG_ATTACH_EXTERNAL_SURFACE = 2;
		
	// Class Attributes
	public String InternalFilesDir;
	public String ExternalFilesDir;
	public String packageResourcePath;
	public boolean OrientationPortrait;
	public int DepthBufferPreference = 32;
	public int PropagateAlpha;
	public boolean UseExternalFilesDir = false;
	public boolean PublicLogFiles = false;

	public String _OBBFilename;
	public boolean packageDataInsideApkValue;
	public int[] ViewPosition = new int[2];
	public int[] ViewSize = new int[2];


	public int VersionCode;
	public int targetSdkVersion;

	public ClipboardManager clipboardManager;
	public AssetManager AssetManagerReference;
	public PackageManager packageManager;
	
	public Bundle _extrasBundle;
	public Bundle _bundle;
	public String appPackageName;
	public String ProjectName;
	public boolean IsForDistribution;
	public String[] keyValues;
	public Surface renderSurface;

	/**
	 Get the SDK level of the OS we are running in.
	 We do this instead of accessing the SDK_INT
	 with JNI from C++ as the new ART runtime seems to have
	 problems dynamically finding/loading static inner classes.
	 */
	public static final int ANDROID_BUILD_VERSION = android.os.Build.VERSION.SDK_INT;



	public void GetVulkanInfoFromPackage( GameActivityForMakeAAR ref )
	{
		// Look for Vulkan support if Nougat or later
		if (ANDROID_BUILD_VERSION >= 24)
		{
			FeatureInfo[] features = this.packageManager.getSystemAvailableFeatures();
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
								ref.VulkanLevel = Integer.parseInt(dump.substring(0, index));
								Log.debug("Vulkan level: " + ref.VulkanLevel);
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
								ref.VulkanVersion = Integer.parseInt(dump.substring(0, index));
								int VersionMajor = (ref.VulkanVersion >> 22) & 0x03ff;
								int VersionMinor = (ref.VulkanVersion >> 12) & 0x03ff;
								int VersionPatch = ref.VulkanVersion & 0x0fff;
								ref.VulkanVersionString = VersionMajor + "." + VersionMinor + "." + VersionPatch;
								Log.debug("GetVulkanInfoFromPackage: Vulkan version: " + VersionMajor + "." + VersionMinor + "." + VersionPatch);
							}
						}
					}
				}
			}
		}
	}

	public GameActivitySetupInfo(Surface externalSurface, SimpleContextWrapper inActivity, String OBBFilename, boolean enablePropagateAlpha) {
		if (inActivity == null)
		{
			return;
		}

		this.clipboardManager = (ClipboardManager) inActivity.getSystemService(CLIPBOARD_SERVICE);

		this.packageManager = inActivity.getApplicationContext().getPackageManager();
		
		// Grab a reference to the asset manager
		this.AssetManagerReference = inActivity.getAssets();

		this.InternalFilesDir = inActivity.getFilesDir().getPath() + "/";
		//this.InternalFilesDir = inActivity.getFilesDir().getAbsolutePath() + "/";
		File _externalFilesDir = inActivity.getExternalFilesDir(null);
		this.ExternalFilesDir = (_externalFilesDir != null ? _externalFilesDir.getPath() : "") + "/";
		this.packageResourcePath = inActivity.getPackageResourcePath();

		this.renderSurface = externalSurface;

		this._extrasBundle = inActivity.mAppActivityContext != null && inActivity.mAppActivityContext instanceof Activity ? ((Activity)inActivity.mAppActivityContext).getIntent().getExtras() : null;

		if (this._extrasBundle == null)
		{
			this._extrasBundle = new Bundle();

		}
		if (this.renderSurface != null) {
			this._extrasBundle.putParcelable("renderSurface", this.renderSurface);
			Log.debug("====> GameActivitySetupInfo(PUT) is storing renderSurface = " + this.renderSurface);
		}

		this.appPackageName = inActivity.getApplicationContext().getPackageName(); //inActivity.getApplicationContext().getPackageName()
		Log.warn("====> GameActivitySetupInfo() check OBBFilename = " + OBBFilename);

		this.ProjectName = this.appPackageName.substring(this.appPackageName.lastIndexOf('.') + 1);



		String AppType = "";
		try
		{
			ApplicationInfo ai = inActivity.getPackageManager().getApplicationInfo(this.appPackageName, PackageManager.GET_META_DATA);
			Bundle bundle = ai.metaData;
			this._bundle = bundle;
			PackageInfo packageInfo = inActivity.getPackageManager().getPackageInfo(this.appPackageName, 0);
			this.VersionCode = packageInfo.versionCode;
			this.targetSdkVersion = packageInfo.applicationInfo.targetSdkVersion;
		}
		catch (PackageManager.NameNotFoundException e)
		{
			Log.debug( "Failed to load meta-data: NameNotFound: " + e.getMessage());
		}
		catch (NullPointerException e)
		{
			Log.debug( "Failed to load meta-data: NullPointer: " + e.getMessage());
		}

		setOBBFilename(OBBFilename);

		Log.verbose("====> GameActivitySetupInfo(constructor) with ProjectName = " + this.ProjectName
				+ ", appPackageName = " + this.appPackageName
				+ ", _OBBFilename = " + this._OBBFilename
				+ ", packageDataInsideApkValue = " + this.packageDataInsideApkValue
		);


		this.PropagateAlpha = enablePropagateAlpha ? 1 : 0;
		this.OrientationPortrait = inActivity.getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
		
		// store the screen resolution (adjusted for immersive mode)
		Map<String, String> variables = new HashMap<String, String>();
		android.view.Display display = inActivity.getWindowManager().getDefaultDisplay();
		android.graphics.Point displaySize = new android.graphics.Point();
		
		// check if immersive mode on window
//		View decorView = inActivity.getWindow().getDecorView();
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
		this.keyValues = new String[variables.size() * 2];
		int keyValueIndex = 0;
		for (Map.Entry<String, String> entry : variables.entrySet())
		{
			this.keyValues[keyValueIndex++] = entry.getKey();
			this.keyValues[keyValueIndex++] = entry.getValue();
		}
	}



	public void AssignContext(Context context)
	{
		AssetManagerReference = context.getAssets();
		clipboardManager = (ClipboardManager) context.getSystemService(CLIPBOARD_SERVICE);
		packageManager = context.getApplicationContext().getPackageManager();
	}

	public void DoNativeUpdates(GameActivityForMakeAAR gameActivity)
	{
		Log.debug("DoNativeUpdates() ExternalFilesDir path: " + this.ExternalFilesDir);

		gameActivity.nativeSetGlobalActivity(this.UseExternalFilesDir, this.PublicLogFiles, this.InternalFilesDir, this.ExternalFilesDir, this.packageDataInsideApkValue, this.packageResourcePath);
		if (this.packageDataInsideApkValue)
		{
			Log.debug("OBB override: " + this._OBBFilename);
			gameActivity.nativeSetObbFilePaths(this._OBBFilename, "", "", "");
		}

		// tell the engine if this is a portrait app and if it will propagate alpha
		gameActivity.nativeSetWindowInfo(this.OrientationPortrait, this.DepthBufferPreference, this.PropagateAlpha);

		// get the full language code, like en-US
		// note: this may need to be Locale.getDefault().getLanguage()
		String Language = java.util.Locale.getDefault().toString();

		boolean bDebuggerAttached = android.os.Debug.isDebuggerConnected();
		gameActivity.nativeSetAndroidStartupState(bDebuggerAttached);

		// Retrieve version code, name, and target SDK
		try
		{
			PackageInfo packageInfo = gameActivity.getPackageManager().getPackageInfo(gameActivity.getPackageName(), 0);
			VersionCode = packageInfo.versionCode;
			gameActivity.VersionName = packageInfo.versionName;
			gameActivity.targetSdkVersion = packageInfo.applicationInfo.targetSdkVersion;
		}
		catch (Exception e)
		{
			Log.debug("Error accessing packageInfo: " + e.getMessage());
		}

		String productName = gameActivity.getProductName();
		Log.debug( "Android version is " + android.os.Build.VERSION.RELEASE );
		Log.debug( "Android manufacturer is " + android.os.Build.MANUFACTURER );
		Log.debug( "Android model is " + android.os.Build.MODEL );
		Log.debug( "Android build number is " + android.os.Build.DISPLAY );
		Log.debug( "OS language is set to " + Language);
		Log.debug( "Product name is " + (productName.isEmpty() ? "[not set]" : productName));
		Log.debug( "Debugger attached is " + bDebuggerAttached );
		Log.debug( "Android targetSdkVersion version is " + gameActivity.targetSdkVersion );
		
		gameActivity.nativeSetAndroidVersionInformation(android.os.Build.VERSION.RELEASE, gameActivity.targetSdkVersion, android.os.Build.MANUFACTURER, android.os.Build.MODEL, android.os.Build.DISPLAY, Language, productName );

		try
		{
			int PatchVersion = 0;
			String AppType = "";
			gameActivity.nativeSetObbInfo(this.ProjectName, this.appPackageName, this.VersionCode, PatchVersion, AppType);
		}
		catch (Exception e)
		{
			// if the above failed, then, we can't use obbs
			Log.debug("==================================> PackageInfo failure getting .obb info: " + e.getMessage());
		}

		// tell Android that we want volume controls to change the media volume, aka music
		// enable the physical volume controls to the game
//			_activity.setVolumeControlStream(AudioManager.STREAM_MUSIC);


		gameActivity.nativeSetConfigRulesVariables(this.keyValues);
	}

	private void setOBBFilename(String obbFilename)
	{
		this._OBBFilename = obbFilename;
		this.packageDataInsideApkValue = obbFilename.equals("") ? true : false;
	}

//	@RequiresApi(api = Build.VERSION_CODES.Q)
	protected GameActivitySetupInfo(Parcel in)
	{
		this.InternalFilesDir = in.readString();
		this.ExternalFilesDir = in.readString();
		this.packageResourcePath = in.readString();
		this.OrientationPortrait = in.readBoolean();
		this.DepthBufferPreference = in.readInt();
		this.PropagateAlpha = in.readInt();
		this.UseExternalFilesDir = in.readBoolean();
		this.PublicLogFiles = in.readBoolean();

		this._OBBFilename = in.readString();
		this.packageDataInsideApkValue = in.readBoolean();
		in.readIntArray(this.ViewPosition);
		in.readIntArray(this.ViewSize);

		this.VersionCode = in.readInt();
		this.targetSdkVersion = in.readInt();

		this._extrasBundle = in.readBundle(getClass().getClassLoader());
		if (this._extrasBundle == null)
		{
			Log.error("=====> PackageInfo _extrasBundle so no renderSurface is passed!" );
		}
		this.renderSurface = this._extrasBundle == null ? null : this._extrasBundle.<Surface>getParcelable("renderSurface");
		Log.debug( "====> GameActivitySetupInfo(GET) is storing renderSurface = " + this.renderSurface);

		Log.error("PackageInfo from _extrasBundle =====>  renderSurface = " + this.renderSurface );

		this._bundle = in.readBundle(getClass().getClassLoader());
		this.appPackageName = in.readString();
		this.ProjectName = in.readString();
		this.IsForDistribution = in.readBoolean();
		this.keyValues = new String[4];

		in.readStringArray(this.keyValues);

	}

	public final static Creator<GameActivitySetupInfo> CREATOR = new Creator<GameActivitySetupInfo>() {
//		@RequiresApi(api = Build.VERSION_CODES.Q)
		@Override
		public GameActivitySetupInfo createFromParcel(Parcel in) {
			return new GameActivitySetupInfo(in);
		}

		@Override
		public GameActivitySetupInfo[] newArray(int size) {
			return new GameActivitySetupInfo[size];
		}
	};

	@Override
	public int describeContents() {
		return 0;
	}

//	@RequiresApi(api = Build.VERSION_CODES.Q)
	@Override
	public void writeToParcel(Parcel parcel, int i) {

		parcel.writeString(this.InternalFilesDir);
		parcel.writeString(this.ExternalFilesDir);
		parcel.writeString(this.packageResourcePath);
		parcel.writeBoolean(this.OrientationPortrait);
		parcel.writeInt(this.DepthBufferPreference);
		parcel.writeInt(this.PropagateAlpha);
		parcel.writeBoolean(this.UseExternalFilesDir);
		parcel.writeBoolean(this.PublicLogFiles);

		parcel.writeString(this._OBBFilename);
		parcel.writeBoolean(this.packageDataInsideApkValue);

		parcel.writeIntArray(this.ViewPosition);
		parcel.writeIntArray(this.ViewSize);

		parcel.writeInt(this.VersionCode);
		parcel.writeInt(this.targetSdkVersion);

		parcel.writeBundle(this._extrasBundle);
		parcel.writeBundle(this._bundle);
		parcel.writeString(this.appPackageName);
		parcel.writeString(this.ProjectName);
		parcel.writeBoolean(this.IsForDistribution);
		parcel.writeStringArray(this.keyValues);
	}
}

