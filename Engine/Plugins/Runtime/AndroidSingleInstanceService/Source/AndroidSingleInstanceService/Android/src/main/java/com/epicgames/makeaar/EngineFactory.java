package com.epicgames.makeaar;

import android.app.Activity;
import android.app.Application;
import android.util.Log;

//import com.epicgames.makeaar.Engine;

import android.os.HandlerThread;
import android.os.Looper;

/**
 * Shared singleton EngineFactory thread for the system.  This is a thread for
 * EngineFactory connectivity operations such as AsyncChannel connections to system services.
 * Various connectivity manager objects can use this singleton as a common
 * resource for their handlers instead of creating separate threads of their own.
 * @hide
 */
public class EngineFactory extends HandlerThread {
	static Engine engine;
	//Activity engineActivity = null;

	// A class implementing the lazy holder idiom: the unique static instance
	// of ConnectivityThread is instantiated in a thread-safe way (guaranteed by
	// the language specs) the first time that Singleton is referenced in get()
	// or getInstanceLooper().
	private static class Singleton {
		private static final EngineFactory INSTANCE = createInstance();
	}

	private EngineFactory() {
		super("EngineFactory");
	}

	private static EngineFactory createInstance() {
		EngineFactory t = new EngineFactory();
		t.start();
		return t;
	}

	public static EngineFactory get() {
		return Singleton.INSTANCE;
	}
	public static Engine getEngine() {
		return Singleton.INSTANCE.engine;
	}

	public static Looper getInstanceLooper() {
		return Singleton.INSTANCE.getLooper();
	}

	public static void Reset()
	{
		if (Singleton.INSTANCE.engine != null)
		{
			Log.d("UE", "EngineFactory::Reset() - releasing current engine resources, engine = " + Singleton.INSTANCE.engine);
			//engine.onDestroy(543210);
			Singleton.INSTANCE.engine.onDestroy(543210, "EngineFactory::Reset()");
		}
		
//		engineActivity = null;
		Singleton.INSTANCE.engine = null;
	}

	public static Engine getInstance(Activity activity, String OBBFilename, String projectModule, boolean enablePropagateAlpha) {
		
		if (Singleton.INSTANCE.engine != null)
		{
            Log.d("UE", "EngineFactory::GetInstance() - USING existing Engine instance, proc = " + Application.getProcessName() + ", engine = " + Singleton.INSTANCE.engine);
			return Singleton.INSTANCE.engine;
		}
//		engineActivity = activity;
		Singleton.INSTANCE.engine = new Engine(activity, OBBFilename, projectModule, enablePropagateAlpha);
		
        Log.d("UE", "EngineFactory::GetInstance(with OBBFilename) - CREATED new Engine instance, proc = " + Application.getProcessName() + ", engine = " + Singleton.INSTANCE.engine);
        return Singleton.INSTANCE.engine;
	}

	public static Engine getInstance(Activity activity) {
		return getInstance(activity, "", "", false);
	}

	public static Engine getInstance(Activity activity, String projectModule) {
		return getInstance(activity, "", projectModule, false);
	}

	public static Engine getInstance(Activity activity, String OBBFilename, String projectModule) {
		return getInstance(activity, OBBFilename, projectModule, false);
	}

	public static Engine getInstance(Activity activity, String projectModule, boolean enablePropagateAlpha) {
		return getInstance(activity, "", projectModule, enablePropagateAlpha);
	}

	public static Engine getInstance(String OBBFilename, String projectModule, boolean enablePropagateAlpha) {
		return getInstance(null, OBBFilename, projectModule, enablePropagateAlpha);
	}
}
