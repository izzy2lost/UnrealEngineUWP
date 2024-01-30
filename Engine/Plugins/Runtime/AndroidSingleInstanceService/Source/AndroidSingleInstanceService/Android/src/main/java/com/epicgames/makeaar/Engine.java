package com.epicgames.makeaar;

import static android.content.Context.CONTEXT_IGNORE_SECURITY;
import static android.content.Context.CONTEXT_INCLUDE_CODE;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.graphics.SurfaceTexture;
import android.view.ViewManager;
import android.graphics.Canvas;
import android.view.ViewTreeObserver;
import com.epicgames.unreal.GameActivity;
import com.epicgames.unreal.SimpleContextWrapper;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Objects;
import java.util.Queue;
import java.lang.Math;

import static java.lang.System.loadLibrary;

public class Engine
{

	public static final int EVENTTYPE_INIT = 0;
	public static final int EVENTTYPE_POST_ENGINE_INIT = 1;				// engine preInit and postInit are complete and all config files have been loaded and command lines processed.
	public static final int EVENTTYPE_ENGINELOOP_INIT_COMPLETE = 2;		//  indicated the engine is ready to start rendering and the RHI will start creating resources and processing app events. This is where external apps can do any special handling and expect the engine to be in a ready state.
	public static final int EVENTTYPE_FRAME_BEGIN = 3;
	public static final int EVENTTYPE_FRAME_END = 4;
	public static final int EVENTTYPE_PRE_LOAD_MAP = 5;					// load pak files is processed before load map is handled such as L_Load
	public static final int EVENTTYPE_POST_LOAD_MAP = 6;				// map is loaded, this on first use is followed by EVENTTYPE_ENGINELOOP_INIT_COMPLETE to indicate the engine is about to prepare to render it's first frame.
	public static final int EVENTTYPE_ACTION = 7;
	public static final int EVENTTYPE_ACTIVITY_HAS_CHANGED = 8;         //activity change was recognized as part of engine init, this can be used to handle language changes and other activity related changes on app side. Activity should not be changed after this until and unless view has lost focus or onPause is issued.
	public static final int EVENTTYPE_ENGINELOOP_SUSPENDED = 9;         //engine is suspeneded and put in an idle state.
	

	private static final String TAG = "UE_ASIS_Engine";


	static boolean CheckEvent(long event)
	{
		return (maskEngineEvents & (1<<event)) != 0;
	}

	public interface IEventCallback
	{
		IEventCallback NO_OP = new IEventCallback() {
			@Override
			public void eventCallback(int event, String param1, int param2, int param3, float param4) {
				// no op
			}
		};
		void eventCallback(int event, String param1, int param2, int param3, float param4);
	}

	private static WeakReference<IEventCallback> mCallbackRef = new WeakReference<>(IEventCallback.NO_OP);

	interface IViewInterface
	{
		public void GetPosSize(int[] pos, int[] size);
		public void SetupAlpha(Boolean enablePropagateAlpha);
		void Detach();
	}

	public static class ViewInterface<T> implements IViewInterface, SurfaceHolder.Callback, TextureView.SurfaceTextureListener, ViewTreeObserver.OnWindowFocusChangeListener, ViewTreeObserver.OnWindowAttachListener//, ViewTreeObserver.OnWindowVisibilityChangeListener
	{
		private static final String TAG = "UE_ASIS_ViewInterface";
		private T view;

		private Class<? extends Object> type;

		public ViewInterface(T inView) {
			this.type = inView.getClass();
			this.view = inView;

			((View)inView).getViewTreeObserver().addOnWindowFocusChangeListener(this);
			((View)inView).getViewTreeObserver().addOnWindowAttachListener(this);
		}

		public void onWindowVisibilityChanged(int visibility)
		{
			if (visibility == View.VISIBLE) {
				QueueResume("ViewInterface::onWindowVisibilityChanged handle VISIBLE");
			} else {
				QueuePause("ViewInterface::onWindowVisibilityChanged handle gone or invisible, visibility=" + visibility);
			}
		}

		public void onWindowFocusChanged(boolean hasFocus)
		{
			Engine engine = Get();
			Log.i(TAG, "** ViewInterface::handleViewFocusChange hasFocus=" + hasFocus
					+ ", maskEngineEvents=" + maskEngineEvents
					//+ ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone
					+ ", bCalledStart:" + engine.bCalledStart
					+ ", startCounter:" + startCounter
					+ ", bResuming:" + bResuming
					+ ", onPauseCounter:" + onPauseCounter
					+ ", renderSurfaceValid:" + engine.renderSurfaceValid
					+ ", renderSurface:" + engine.renderSurface
					+ ", pendingChangedSurface:" + engine.pendingChangedSurface
			);

			if (!hasFocus)
			{
				QueuePause("ViewInterface::handleViewFocusChange, hasFocus=" + hasFocus);
			}
			else
			{
				QueueResume("ViewInterface::handleViewFocusChange, hasFocus=" + hasFocus + ", maskEngineEvents=" + maskEngineEvents);
			}
		}
		@Override
		public void onWindowAttached() {
			Engine engine = Get();
			Log.i(TAG, "** ViewInterface::onWindowAttached"
				//+ ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone
				+ ", bCalledStart:" + engine.bCalledStart
				+ ", startCounter:" + startCounter
				+ ", bResuming:" + bResuming
				+ ", onPauseCounter:" + onPauseCounter
				+ ", renderSurfaceValid:" + engine.renderSurfaceValid
				+ ", renderSurface:" + engine.renderSurface
				+ ", pendingChangedSurface:" + engine.pendingChangedSurface
			);
		}
		@Override
		public void onWindowDetached() {
			Engine engine = Get();
			Log.i(TAG, "** ViewInterface::onWindowDetached"
				//+ ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone
				+ ", bCalledStart:" + engine.bCalledStart
				+ ", startCounter:" + startCounter
				+ ", bResuming:" + bResuming
				+ ", onPauseCounter:" + onPauseCounter
				+ ", renderSurfaceValid:" + engine.renderSurfaceValid
				+ ", renderSurface:" + engine.renderSurface
				+ ", pendingChangedSurface:" + engine.pendingChangedSurface
			);
		}

		public boolean canAcceptType(Class<?> candidate) {
			return type.isAssignableFrom(candidate);
		}

		public boolean operator_not_equals(final Class<?> e2) {
			return this.view != e2;
		}

		public void GetPosSize(int[] pos, int[] size)
		{
			if (pos.length == 2) {
				((View) view).getLocationInWindow(pos);
			}

			if (size.length == 2) {
				size[0] = ((View) view).getWidth();
				size[1] = ((View) view).getHeight();
			}
		}

		public void SetupAlpha(Boolean enablePropagateAlpha)
		{
			if (view instanceof SurfaceView)
			{
				SurfaceView surfaceView = ((SurfaceView)view);
				surfaceView.setZOrderOnTop(true);
				surfaceView.setAlpha(1.0f);
				if (enablePropagateAlpha)
				{
					surfaceView.getHolder().setFormat(PixelFormat.TRANSPARENT);
				}
				surfaceView.getHolder().addCallback(this);
				if (surfaceView.isAttachedToWindow())
				{
					surfaceCreated(surfaceView.getHolder());
				}
			}
			else if (view instanceof TextureView)
			{
				TextureView textureView = ((TextureView)view);
				textureView.setAlpha(1.0f);
				textureView.setOpaque(!enablePropagateAlpha);
				textureView.setSurfaceTextureListener(this);
				if (textureView.isAttachedToWindow())
				{
					if (textureView.getSurfaceTexture() != null) {
						onSurfaceTextureAvailable(textureView.getSurfaceTexture(), textureView.getWidth(), textureView.getHeight());
					}
				}
			}
		}


		@Override
		public void surfaceCreated(SurfaceHolder holder) {
			int[] surfacePosition = new int[] {holder.getSurfaceFrame().left, holder.getSurfaceFrame().top};
			int[] surfaceSize = new int[]{holder.getSurfaceFrame().width(), holder.getSurfaceFrame().height()};

			Log.v(TAG, "surfaceCreated, surfacePosition = " + Arrays.toString(surfacePosition) + ", surfaceSize = " + Arrays.toString(surfaceSize) + ", holder.getSurface().isValid()= " + holder.getSurface().isValid());

			Engine engine = Get();

			synchronized (syncLock) {				
				engine.renderSurfaceViewPosition = surfacePosition;
				engine.renderSurfaceViewSize = surfaceSize;
				engine.pendingChangedSurface = holder.getSurface();
			}
		}

		@Override
		public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
			Log.v(TAG, "surfaceChanged, format = " + format + ", width = " + width);

			int[] surfacePosition = new int[] {holder.getSurfaceFrame().left, holder.getSurfaceFrame().top};
			int[] surfaceSize = new int[]{width, height};

			if (view != null)
			{
				GetPosSize(surfacePosition, surfaceSize);
				Log.v(TAG, "surfaceChanged with view, format = " + format + ", surfacePosition = " + Arrays.toString(surfacePosition) + ", surfaceSize = " + Arrays.toString(surfaceSize));
			}

			Engine engine = Get();

			synchronized (syncLock) {
				if (holder.getSurface() != engine.renderSurface && engine.renderSurface != null)
				{
					Log.w(TAG, "surfaceChanged and releaseVolatileResources cause holder surface is different than prev");

					engine.releaseVolatileResources("surfaceChanged cause holder surface is different than prev");
				}

				engine.renderSurfaceViewPosition = surfacePosition;
				engine.renderSurfaceViewSize = surfaceSize;
				engine.pendingChangedSurface = holder.getSurface();
				engine.QueueResume("surfaceChanged");
			}
		}

		@Override
		public void surfaceDestroyed(SurfaceHolder holder) {

			Engine engine = Get();

			Log.v(TAG, "surfaceDestroyed, holder = " + holder
				+ ", lifecycleContextID=" + lifecycleContextID
				+ ", bResuming=" + bResuming
				+ ", surfaceCreatedCounter=" + surfaceCreatedCounter
				+ ", onPauseCounter=" + onPauseCounter
				+ ", renderSurfaceValid=" + engine.renderSurfaceValid
				+ ", renderSurface=" + engine.renderSurface
				+ ", holder.surface=" + holder.getSurface()
			);

			synchronized (syncLock) {
				if (engine.renderSurfaceValid && engine.renderSurface == holder.getSurface()) {
					Log.v(TAG, "surfaceDestroyed, doing cleanup and pausing, holder = " + holder
							+ ", lifecycleContextID=" + lifecycleContextID
							+ ", bResuming=" + bResuming
							+ ", surfaceCreatedCounter=" + surfaceCreatedCounter
							+ ", onPauseCounter=" + onPauseCounter
							+ ", renderSurfaceValid=" + engine.renderSurfaceValid
							+ ", renderSurface=" + engine.renderSurface
							+ ", holder.surface=" + holder.getSurface()
					);

					engine.onPause(lifecycleContextID, "surfaceDestroyed(doing cleanup)" );

				} else if (holder.getSurface() != null && engine.renderSurface != null) {
					Log.v(TAG, "surfaceDestroyed, re-bind surface, holder = " + holder
							+ ", lifecycleContextID=" + lifecycleContextID
							+ ", bResuming=" + bResuming
							+ ", surfaceCreatedCounter=" + surfaceCreatedCounter
							+ ", onPauseCounter=" + onPauseCounter
							+ ", renderSurfaceValid=" + engine.renderSurfaceValid
							+ ", renderSurface=" + engine.renderSurface
							+ ", holder.surface=" + holder.getSurface()
					);

					engine.pendingChangedSurface = holder.getSurface();

					if (engine.pendingChangedSurface != null) {
						bResuming = true;
						engine.QueueResume("surfaceDestroyed valid pendingChangedSurface");
					}
				}
			}
		}

		private Surface surfaceForTextureView = null;

		@Override
		public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture, int width, int height) {

			synchronized (syncLock) {
				TextureView textureView = ((TextureView) view);

				int[] surfacePosition = new int[]{0, 0};
				textureView.getLocationOnScreen(surfacePosition);
				int[] surfaceSize = new int[]{width, height};
				Log.v(TAG, "onSurfaceTextureAvailable, surfacePosition = " + Arrays.toString(surfacePosition) + ", surfaceSize = " + Arrays.toString(surfaceSize));

				renderSurfaceViewPosition = surfacePosition;
				renderSurfaceViewSize = surfaceSize;


				if (surfaceForTextureView != null) {
					surfaceForTextureView.release();
					surfaceForTextureView = null;
				}

				Engine.Get().pendingChangedSurface = new Surface(surfaceTexture);
				surfaceForTextureView = Engine.Get().pendingChangedSurface;
			}
			Engine.Get().QueueResume("onSurfaceTextureAvailable");
		}

		@Override
		public void onSurfaceTextureSizeChanged(SurfaceTexture surfaceTexture, int width, int height) {
			synchronized (syncLock) {
				TextureView textureView = ((TextureView) view);

				int[] surfacePosition = new int[]{0, 0};
				textureView.getLocationOnScreen(surfacePosition);
				int[] surfaceSize = new int[]{width, height};
				Log.v(TAG, "onSurfaceTextureSizeChanged, surfacePosition = " + Arrays.toString(surfacePosition) + ", surfaceSize = " + Arrays.toString(surfaceSize));

				renderSurfaceViewSize = surfaceSize;
				GetOverrideGameActivity().nativeSetSurfaceViewInfo(renderSurfaceViewSize[0], renderSurfaceViewSize[1]);
			}
		}

		// Invoked when the specified SurfaceTexture is about to be destroyed.
		// If returns true, no rendering should happen inside the surface texture after this method is invoked.
		// If returns false, the client needs to call SurfaceTexture.release().
		// Most applications should return true.
		@Override
		public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {

			synchronized (syncLock) {

				Log.v(TAG, "onSurfaceTextureDestroyed, surfaceForTextureView = " + surfaceForTextureView + ", renderSurface = " + Engine.Get().renderSurface);

				Engine.Get().releaseVolatileResources("onSurfaceTextureDestroyed called");
				if (surfaceForTextureView != null)
				{
					surfaceForTextureView.release();
					surfaceForTextureView = null;
				}

			}
			return true;
		}

		@Override
		public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {
		}


		public void Detach()
		{
			Log.i(TAG, "* Engine::Detach"
					+ ", lifecycleContextID=" + lifecycleContextID
					+ ", bResuming=" + bResuming
					+ ", surfaceCreatedCounter=" + surfaceCreatedCounter
					+ ", onPauseCounter=" + onPauseCounter
					+ ", surfaceForTextureView=" + surfaceForTextureView
			);

			if (view instanceof SurfaceView)
			{
				SurfaceView surfaceView = ((SurfaceView)view);
				surfaceView.getHolder().removeCallback(this);
			}
			else if (view instanceof TextureView)
			{
				TextureView textureView = ((TextureView)view);
				textureView.setSurfaceTextureListener(null);
			}

			((View)view).getViewTreeObserver().removeOnWindowFocusChangeListener(this);
            ((View)view).getViewTreeObserver().removeOnWindowAttachListener(this);
			//((View)view).getViewTreeObserver().removeOnWindowVisibilityChangeListener(this);

			if (surfaceForTextureView != null)
			{
				surfaceForTextureView.release();
				surfaceForTextureView = null;
			}
		}


		public SurfaceHolder getHolder() {
			return type == SurfaceView.class ? ((SurfaceView)view).getHolder() : null;
		}

		@Override
		public boolean equals(Object o) {
			if (this == o) return true;
			if (o == null || this.type != o.getClass()) return false;

			return view.equals(o);
		}

		@Override
		public int hashCode() {
			return Objects.hash(view, type);
		}
	}

	// Volatile Variables...
	private static int surfaceCreatedCounter = 0;
	private SimpleContextWrapper activity = null;
	private static boolean bLibraryLoaded = false;
	IViewInterface renderView = null;

	private static long maskEngineEvents = 0;
	private boolean renderSurfaceValid = false;
	private Surface renderSurface = null;
	private Surface pendingChangedSurface = null;
	public static int renderSurfaceViewPosition[] = new int[2];
	public static int renderSurfaceViewSize[] = new int[2];
	private static final Object syncLock = new Object();
	//private GameActivityForMakeAAR gameActivity = null; //TODO remove ref here and use Get() on GameActivity
	private String OBBFilename = null;

	private boolean bOBBinAPK = true;

	private static boolean bResuming = false;
	private boolean bAllowConsole = false;
	private boolean bGameActivityNativeMainIsDone = false;


	private static GameActivityForMakeAAR GetOverrideGameActivity()
	{
		assert(GameActivity.Get() != null);
		return (GameActivityForMakeAAR)GameActivity.Get();
	}

	private static GameActivityForMakeAAR GetOverrideGameActivityWhenInitComplete()
	{
		Log.w(TAG, "GetOverrideGameActivityWhenInitComplete called, maskEngineEvents=" + Long.toBinaryString(maskEngineEvents));
		return (GameActivityForMakeAAR)GameActivity.Get();

	//assert(GetCurrentActivityContext() != null);
	//synchronized(GetCurrentActivityContext()) {

	//	try
	//	{
	//		while (!CheckEvent(EVENTTYPE_POST_ENGINE_INIT))
	//		{
	//			Log.w(TAG, "GetOverrideGameActivityWhenInitComplete waiting for EVENTTYPE_POST_ENGINE_INIT!, maskEngineEvents=" + Long.toBinaryString(maskEngineEvents));
	//			GetCurrentActivityContext().wait(10);
	//			if (CheckEvent(EVENTTYPE_ENGINELOOP_SUSPENDED))
	//			{
	//				Log.e(TAG, "GetOverrideGameActivityWhenInitComplete is suspended!, maskEngineEvents=" + Long.toBinaryString(maskEngineEvents));

	//				break;
	//			}
	//		}
	//	}
	//	catch (Exception e)
	//	{
	//		Log.e(TAG, "GetOverrideGameActivityWhenInitComplete EXCEPTION!, maskEngineEvents=" + Long.toBinaryString(maskEngineEvents));
	//		e.printStackTrace();
	//	}
	//	finally() {
	//		Log.i(TAG, "GetOverrideGameActivityWhenInitComplete ready!, maskEngineEvents=" + Long.toBinaryString(maskEngineEvents));
	//		return (GameActivityForMakeAAR)GameActivity.Get();
	//	}

	//}
}

void DoGameActivityNativeMain(String projectModule, String reasonString)
	{
		Log.i(TAG, "DoGameActivityNativeMain is called with reasonString=" + reasonString + ", projectModule=" + projectModule);
	
		GetOverrideGameActivity().nativeMain(projectModule);

		bGameActivityNativeMainIsDone = true;
	}

	public static String[] pendingPakList = null;

	private static int TOUCHTYPE_BEGIN = 0;
	private static int TOUCHTYPE_MOVE = 1;
	private static int TOUCHTYPE_END = 2;

	private static int APP_CMD_INPUT_CHANGED = 0;
	private static int APP_CMD_INIT_WINDOW = 1;
	private static int APP_CMD_TERM_WINDOW = 2;
	private static int APP_CMD_WINDOW_RESIZED = 3;
	private static int APP_CMD_WINDOW_REDRAW_NEEDED = 4;
	private static int APP_CMD_CONTENT_RECT_CHANGED = 5;
	private static int APP_CMD_GAINED_FOCUS = 6;
	private static int APP_CMD_LOST_FOCUS = 7;
	private static int APP_CMD_CONFIG_CHANGED = 8;
	private static int APP_CMD_LOW_MEMORY = 9;
	private static int APP_CMD_START = 10;
	private static int APP_CMD_RESUME = 11;
	private static int APP_CMD_SAVE_STATE = 12;
	private static int APP_CMD_PAUSE = 13;
	private static int APP_CMD_STOP = 14;
	private static int APP_CMD_DESTROY = 15;

	private static int logOrder = 0;

	private static boolean _enablePropagateAlpha = false;

	private static int onPauseCounter=0;

	public static Engine Get()
	{
		assert (EngineFactory.engine != null);
		return EngineFactory.engine;
	}

	public Surface GetRenderSurface()
	{
		return renderSurface;
	}

	public Engine(Activity inActivity, String inOBBFilename, String projectModule, boolean enablePropagateAlpha)
	{
		Log.d(TAG, "Engine::Engine() - CONSTRUCTING Engine = " + this);// + ", AARVersion= " + BuildConfig.AAR_VERSION);

		_enablePropagateAlpha = enablePropagateAlpha;

		Log.i(TAG, "Engine - attempting to load engine with inActivity = " + inActivity);

		try {
			loadLibrary("Unreal");
		}
		catch (java.lang.UnsatisfiedLinkError e)
		{
			Log.e(TAG, "Engine - failed to load libUE4! UnsatisfiedLinkError: " + e.getMessage());
			return;
		}

		// now try the engine
		if (OBBFilename == null || !inOBBFilename.isEmpty())
		{
			Log.i(TAG, "Engine - constructor is setting the OBBFilename: " + inOBBFilename);
			OBBFilename = inOBBFilename;
		}

		if (inActivity != null && GameActivity.Get() == null && !OBBFilename.isEmpty())
		{
			Log.i(TAG, "Engine - using inActivity to setup new GameActivity: " + inActivity);
			GameActivityForMakeAAR.Create().setActivity(inActivity, OBBFilename, _enablePropagateAlpha);
			Log.i(TAG, "** Engine(constructor case1) - calling GetOverrideGameActivity().nativeMain() with : projectModule = " + projectModule);
			
			DoGameActivityNativeMain(projectModule, "Engine(case 1)");
		}
		else if (GameActivity.Get() != null && !projectModule.isEmpty())
		{
			Log.i(TAG, "Engine - inActivity is passed in and projectModule is defined so setup GameActivity nativeMain");
			// start game thread
			Log.i(TAG, "** Engine(constructor case2) - calling GetOverrideGameActivity().nativeMain() with : projectModule = " + projectModule);
			DoGameActivityNativeMain(projectModule, "Engine(case 2)");
		}
		else
		{
			Log.e(TAG, "Engine - constructor but not creating GameActivity instance!");
			bGameActivityNativeMainIsDone = false;
		}


		bLibraryLoaded = true;

		Log.i(TAG, "Engine - engine load success");
	}

	public boolean ApplyPendingSurfaceChanges()
	{
		synchronized (syncLock) {
			if (pendingChangedSurface == null || pendingChangedSurface == renderSurface) {
				Log.w(TAG, "logOrder:" + logOrder + " *** Engine::ApplyPendingSurfaceChanges(DO NOTHING pending is NULL, or same as current) - renderSurfaceValid=" + renderSurfaceValid + ", renderSurface=" + renderSurface);
				return false;
			}

			final boolean isChanging = (pendingChangedSurface != renderSurface) && renderSurface != null;

			Log.i(TAG, "logOrder:" + logOrder + " *** Engine::ApplyPendingSurfaceChanges() - bResuming=" + bResuming
					+ ", isChanging=" + isChanging
					+ ", onPauseCounter=" + onPauseCounter
					+ ", renderSurfaceValid=" + renderSurfaceValid
					+ ", renderSurface = " + renderSurface
					+ ", pendingChangedSurface=" + pendingChangedSurface
					+ ", pendingChangedSurface.isValid()=" + pendingChangedSurface.isValid()
					+ ", renderSurfaceViewSize=" + Arrays.toString(renderSurfaceViewSize)
			);

			if (isChanging) {
				++surfaceCreatedCounter;
			}

			GetOverrideGameActivityWhenInitComplete().nativeSetSurfaceOverride(pendingChangedSurface, renderSurfaceViewPosition[0], renderSurfaceViewPosition[1]);
			GetOverrideGameActivityWhenInitComplete().nativeSetSurfaceViewInfo(renderSurfaceViewSize[0], renderSurfaceViewSize[1]);
			renderSurface = pendingChangedSurface;
			renderSurfaceValid = true;
			pendingChangedSurface = null;

/*
			if (!CheckEvent(EVENTTYPE_ENGINELOOP_INIT_COMPLETE)) {
				GameActivity.GetCurrentActivityContext().runOnUiThread(() ->
					{
						synchronized(syncLock)
						{
							Log.w(TAG, "FORCED -> Engine::ApplyPendingSurfaceChanges(!EVENTTYPE_ENGINELOOP_INIT_COMPLETE) - renderSurfaceValid=" + renderSurfaceValid + ", renderSurface=" + renderSurface);

							//GetOverrideGameActivity().nativeAppCommand(APP_CMD_INIT_WINDOW);
							//GetOverrideGameActivity().nativeAppCommand(APP_CMD_GAINED_FOCUS);
							GetOverrideGameActivity().nativeResumeMainInit();
						}
					}
				);

			}
*/

			return isChanging;
		}
	}

	public boolean DoHousekeepingForViewChange(SimpleContextWrapper inActivity, GameActivitySetupInfo gameActivitySetupInfo) {

		if (!bLibraryLoaded)
		{
			Log.e(TAG, "** Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) - bLibraryLoaded is false.");
			return false;
		}


		if (inActivity == null)
		{
			Log.e(TAG, "** Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) - inView does not have a valid Activity so do nothing... perhaps should do cleanup on any pre-existing setup?");
			return false;
		}

		Log.i(TAG, "Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) ENTERING, current Activity=" + activity + ", inActivity=" + inActivity + ", viewPosition=" + Arrays.toString(gameActivitySetupInfo.ViewPosition) + ", viewSize=" + Arrays.toString(gameActivitySetupInfo.ViewSize) );


		boolean bAcitivityNeedsSetup = true;

		// Check for Activity Change...
		//
		if (activity != null)
		{
			if (activity.equals(inActivity))
			{
				Log.i(TAG, "** Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) - inView is using the same acitivity as before so proceed to just updating view...");
				bAcitivityNeedsSetup = false;
			}
			else
			{
				Log.i(TAG, "** Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) - previous activity is being changing so should do full cleanup..."
					+ ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone
					+ ", bCalledStart:" + bCalledStart
					+ ", startCounter:" + startCounter
					+ ", bResuming:" + bResuming
					+ ", onPauseCounter:" + onPauseCounter
					+ ", renderSurfaceValid:" + renderSurfaceValid
					+ ", renderSurface:" + renderSurface
					+ ", renderSurfaceViewPosition:" + Arrays.toString(renderSurfaceViewPosition)
					+ ", renderSurfaceViewSize:" + Arrays.toString(renderSurfaceViewSize)
					+ ", surfaceCreatedCounter:" + surfaceCreatedCounter
					+ ", pendingChangedSurface:" + pendingChangedSurface
				);

				bAcitivityNeedsSetup = true;
			}
		}


		boolean bFirstInit = GameActivity.Get() == null;

		if (gameActivitySetupInfo.ViewPosition.length == 2)
		{
			renderSurfaceViewPosition = gameActivitySetupInfo.ViewPosition;
		}

		if (gameActivitySetupInfo.ViewSize.length == 2)
		{
			if (gameActivitySetupInfo.ViewSize[0] != 0 && gameActivitySetupInfo.ViewSize[1] != 0) {
				renderSurfaceViewSize = gameActivitySetupInfo.ViewSize;
			}
		}

		if (bAcitivityNeedsSetup)
		{
			synchronized(syncLock)
			{
				if (GameActivity.Get() == null)
				{
					Log.i(TAG, "Engine ::DoHousekeepingForViewChange(GameActivitySetupInfo) - using inActivity to setup new GameActivity: " + inActivity + ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone);

					GameActivityForMakeAAR.Create().setActivity(inActivity, gameActivitySetupInfo);
					pendingChangedSurface = gameActivitySetupInfo.renderSurface;
				}
				else
				{
					Log.i(TAG, "Engine ::DoHousekeepingForViewChange(GameActivitySetupInfo) - using inActivity update GameActivity: " + inActivity + ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone);

					GetOverrideGameActivity().setActivity(inActivity, gameActivitySetupInfo);
					pendingChangedSurface = gameActivitySetupInfo.renderSurface;
				}
			}
			bFirstInit = true;
		}

		if ((gameActivitySetupInfo.renderSurface != renderSurface) && renderSurface != null)
		{
			Log.i(TAG, "Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) - remove old renderSurface =" + renderSurface );
			releaseVolatileResources("DoHousekeepingForViewChange(GameActivitySetupInfo) - remove old renderSurface");
		}

		Log.i(TAG, "* Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) - Before recovery checks, bFirstInit=" + bFirstInit + ", bResuming=" + bResuming + ", renderSurfaceValid=" + renderSurfaceValid + ", renderSurface = " + renderSurface);

		++logOrder;
		++surfaceCreatedCounter;

		if (renderSurface != null || surfaceCreatedCounter > 1)
		{
			Log.e(TAG, "logOrder:" + logOrder + " ** Engine::onSurfaceTextureAvailable(ERROR SURFACE POSSIBLY ALREADY EXISTS) - surfaceChanged size=" + Arrays.toString(gameActivitySetupInfo.ViewSize) + ", bResuming=" + bResuming + ", renderSurfaceValid=" + renderSurfaceValid + "renderSurface" + renderSurface + ", surfaceCreatedCounter=" + surfaceCreatedCounter );
		}

		synchronized(syncLock)
		{
			pendingChangedSurface = gameActivitySetupInfo.renderSurface;
		}

		Log.i(TAG, "* Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) - After recovery checks, bFirstInit=" + bFirstInit + ", bResuming=" + bResuming + ", renderSurfaceValid=" + renderSurfaceValid + ", renderSurface = " + renderSurface);

		activity = inActivity;

		if (bFirstInit && !bResuming)
		{
			bResuming = true;
			Log.i(TAG, "* Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) - check for resuming if renderSurface is not null, bFirstInit=" + bFirstInit
					+ ", bResuming=" + bResuming
					+ ", bCalledStart=" + bCalledStart
					+ ", onPauseCounter=" + onPauseCounter
					+ ", renderSurfaceValid=" + renderSurfaceValid
					+ ", renderSurface = " + renderSurface
			);

			if (!bCalledStart)
			{
				QueueStart("DoHousekeepingForViewChange(GameActivitySetupInfo)");
			}

			if (renderSurface != null)
			{
				QueueResume("DoHousekeepingForViewChange(GameActivitySetupInfo)");
			}
		}

		Log.i(TAG, "* Engine::DoHousekeepingForViewChange(GameActivitySetupInfo) - setup done!, bFirstInit=" + bFirstInit
				+ ", bResuming=" + bResuming
				+ ", bCalledStart=" + bCalledStart
				+ ", onPauseCounter=" + onPauseCounter
				+ ", renderSurfaceValid=" + renderSurfaceValid
				+ ", renderSurface = " + renderSurface
		);

		return true;
	}

	public boolean DoHousekeepingForViewChange(Context inContext, IViewInterface view, String OBBFilename, boolean enablePropagateAlpha) {

		if (!bLibraryLoaded)
		{
			Log.e(TAG, "** Engine::DoHousekeepingForViewChange(IViewInterface) - bLibraryLoaded is false.");
			return false;
		}

		Activity inActivity = SimpleContextWrapper.getActivityOfContext(inContext);

		if (inActivity == null)
		{
			Log.e(TAG, "** Engine::DoHousekeepingForViewChange(IViewInterface) - inView does not have a valid Activity so do nothing... perhaps should do cleanup on any pre-existing setup?");
			return false;
		}

		boolean bAcitivityNeedsSetup = true;

		// Check for Activity Change...
		//
		if (activity != null)
		{
			if (activity.equals(inActivity))
			{
				Log.w(TAG, "** Engine::DoHousekeepingForViewChange(IViewInterface) - inView is using the same acitivity as before so proceed to just updating view...");
				bAcitivityNeedsSetup = false;
			}
			else
			{
				Log.i(TAG, "** Engine::DoHousekeepingForViewChange(IViewInterface) - previous activity is being changing so should do full cleanup..."
					+ ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone
					+ ", bCalledStart:" + bCalledStart
					+ ", startCounter:" + startCounter
					+ ", bResuming:" + bResuming
					+ ", onPauseCounter:" + onPauseCounter
					+ ", renderSurfaceValid:" + renderSurfaceValid
					+ ", renderSurface:" + renderSurface
					+ ", renderSurfaceViewPosition:" + Arrays.toString(renderSurfaceViewPosition)
					+ ", renderSurfaceViewSize:" + Arrays.toString(renderSurfaceViewSize)
					+ ", surfaceCreatedCounter:" + surfaceCreatedCounter
					+ ", pendingChangedSurface:" + pendingChangedSurface
					+ ", renderView:" + renderView
				);

				bAcitivityNeedsSetup = true;
			}
		}


		boolean bFirstInit = GameActivity.Get() == null;

		if (renderView != null) {
			renderView.Detach();
			renderView = null;
		}

		renderView = view;

		view.GetPosSize(renderSurfaceViewPosition, renderSurfaceViewSize);

		if (bAcitivityNeedsSetup)
		{
			synchronized(syncLock)
			{
				if (GameActivity.Get() == null)
				{
					Log.i(TAG, "Engine ::DoHousekeepingForViewChange(IViewInterface) - using inActivity to setup new GameActivity: " + inActivity + ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone);
					
					GameActivityForMakeAAR.Create().setActivity(inActivity, OBBFilename, enablePropagateAlpha);
				}
				else
				{
					Log.i(TAG, "Engine ::DoHousekeepingForViewChange(IViewInterface) - using inActivity update GameActivity: " + inActivity + ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone);

					GetOverrideGameActivity().setActivity(inActivity, OBBFilename, enablePropagateAlpha);
				}
			}
			bFirstInit = true;
		}

		view.SetupAlpha(enablePropagateAlpha);

		Log.i(TAG, "* Engine::DoHousekeepingForViewChange(IViewInterface) - Before recovery checks, bFirstInit=" + bFirstInit + ", bResuming=" + bResuming + ", renderSurfaceValid=" + renderSurfaceValid + ", renderSurface = " + renderSurface);

		++logOrder;

		Log.i(TAG, "* Engine::DoHousekeepingForViewChange(IViewInterface) - After recovery checks, bFirstInit=" + bFirstInit + ", bResuming=" + bResuming + ", renderSurfaceValid=" + renderSurfaceValid + ", renderSurface = " + renderSurface);

		activity = new SimpleContextWrapper(inActivity);

		if (bFirstInit && !bResuming)
		{
			bResuming = true;
			Log.i(TAG, "* Engine::DoHousekeepingForViewChange(IViewInterface) - check for resuming if renderSurface is not null, bFirstInit=" + bFirstInit
					+ ", bResuming=" + bResuming
					+ ", bCalledStart=" + bCalledStart
					+ ", onPauseCounter=" + onPauseCounter
					+ ", renderSurfaceValid=" + renderSurfaceValid
					+ ", renderSurface = " + renderSurface
			);

			if (!bCalledStart)
			{
				QueueStart("DoHousekeepingForViewChange(IViewInterface)");
			}

			if (renderSurface != null)
			{
				QueueResume("DoHousekeepingForViewChange(IViewInterface)");
			}
		}

		Log.i(TAG, "* Engine::DoHousekeepingForViewChange(IViewInterface) - setup done!, bFirstInit=" + bFirstInit
				+ ", bResuming=" + bResuming
				+ ", bCalledStart=" + bCalledStart
				+ ", onPauseCounter=" + onPauseCounter
				+ ", renderSurfaceValid=" + renderSurfaceValid
				+ ", renderSurface = " + renderSurface
			);


		return true;
	}

	private boolean VerifyContextID(int contextID, String reasonString)
	{
		if (contextID != 0 && lifecycleContextID != 0 && contextID != lifecycleContextID) {
			Log.e(TAG, "[lifecycleContextID=" + lifecycleContextID + "] contextID:" + contextID + " ABNORMAL VerifyContextID. with reasonString = " + reasonString );
			return false;
		}

		return true;
	}


	public boolean AttachExternalRenderSurface(int contextID, Surface externalSurface, int viewPos[], int viewSize[])
	{
		if (!bHasBeenInit || GameActivity.Get() == null || externalSurface == null)
		{
			return false;
		}

		Log.i(TAG, "** Engine::AttachExternalRenderSurface() - bFirstInit"
			+ ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone
			+ ", contextID:" + contextID
			+ ", lifecycleContextID:" + lifecycleContextID
			+ ", bCalledStart:" + bCalledStart
			+ ", startCounter:" + startCounter
			+ ", bResuming:" + bResuming
			+ ", onPauseCounter:" + onPauseCounter
			+ ", renderSurfaceValid:" + renderSurfaceValid
			+ ", renderSurface:" + renderSurface
			+ ", viewPos:" + (viewPos.length == 2 ? String.format(" position[%d,%d]", viewPos[0], viewPos[1]) : viewPos.length)
			+ ", renderSurfaceViewSize:" + (renderSurfaceViewSize.length == 2 ? String.format(" size[%d,%d]", renderSurfaceViewSize[0], renderSurfaceViewSize[1]) : renderSurfaceViewSize.length)
			+ ", viewSize:" + (viewSize.length == 2 ? String.format(" size[%d,%d]", viewSize[0], viewSize[1]) : viewSize.length)
			+ ", surfaceCreatedCounter:" + surfaceCreatedCounter
			+ ", pendingChangedSurface:" + pendingChangedSurface
		);

		if (renderSurface != null || surfaceCreatedCounter>0)
		{
			ReleaseRenderSurfaces("AttachExternalRenderSurface(renderSurface != null || surfaceCreatedCounter>0)");
		}

		++logOrder;
		++surfaceCreatedCounter;

		synchronized (syncLock) {
			Engine.Get().sendConsoleCommand("a.AllowFrameTimestamps 0");	// needed to avoid E/FrameEvents: updateAcquireFence: Did not find frame. spamming.
			pendingChangedSurface = externalSurface;
			if (viewSize.length==2)
			{
				renderSurfaceViewSize[0] = viewSize[0];
				renderSurfaceViewSize[1] = viewSize[1];
			}
			else
			{
				Log.w(TAG, "** Engine::AttachExternalRenderSurface() - input viewSize is not defined, so use existing!, renderSurfaceViewSize = " + Arrays.toString(renderSurfaceViewSize));
			}

			if (viewPos.length==2)
			{
				renderSurfaceViewPosition[0] = viewPos[0];
				renderSurfaceViewPosition[1] = viewPos[1];
			}
			else
			{
				Log.w(TAG, "** Engine::AttachExternalRenderSurface() - input viewPos is not defined, so use existing!, renderSurfaceViewPosition = " + Arrays.toString(renderSurfaceViewPosition));
			}
		}


		Log.i(TAG, "** Engine::AttachExternalRenderSurface() - about to ApplyPendingSurfaceChanges...");

		bResuming |= ApplyPendingSurfaceChanges();

		Log.i(TAG, "** Engine::AttachExternalRenderSurface() - after to ApplyPendingSurfaceChanges...");

		if (bResuming && renderSurface != null)
		{
			Log.i(TAG, "** Engine::AttachExternalRenderSurface() - about to QueueResume ...");

			QueueResume("AttachExternalRenderSurface");
		}

		Log.i(TAG, "Engine::AttachExternalRenderSurface(DONE)"
			+ ", startCounter:" + startCounter
			+ ", bResuming:" + bResuming
			+ ", onPauseCounter:" + onPauseCounter
			+ ", renderSurfaceValid:" + renderSurfaceValid
			+ ", renderSurface:" + renderSurface
			+ ", surfaceCreatedCounter:" + surfaceCreatedCounter
			+ ", pendingChangedSurface:" + pendingChangedSurface
		);

		return bResuming;
	}

	public static void QueuePause(String reasonString)
	{
		if (bResuming && onPauseCounter <= 0) {
			Log.e(TAG, "QueuePause is CALLED with lifecycleContextID=" + lifecycleContextID + ", bResuming=" + bResuming);
			QueueResume("QueuePause but was resuming and reasonString=" + reasonString);
			return;
		}

		if (GameActivity.Get() == null) {
			Log.e(TAG, "QueuePause is REJECTED cause GameActivity is not valid with lifecycleContextID=" + lifecycleContextID + ", bResuming=" + bResuming + ", reasonString=" + reasonString);

			return;
		}

		GameActivityForMakeAAR.GetCurrentActivityContext().runOnMainThread(() ->
				{
					synchronized(syncLock)
					{
						Log.i(TAG, "QueuePause is calling engine.onPause with lifecycleContextID=" + lifecycleContextID + ", bResuming=" + bResuming);
						Engine.Get().onPause(lifecycleContextID, reasonString);
					}
				}
		);
	}

	public static void QueueResume(String reasonString)
	{
		if (!GameActivityForMakeAAR.isValidGameActivity())
		{
			return;
		}

		if ( Get().pendingChangedSurface == null && Get().renderSurface == null)
		{
			Log.e(TAG, "QueueResume is IGNORED, lifecycleContextID=" + lifecycleContextID + ", reasonString=" + reasonString + ", bResuming= " + bResuming + ", pendingChangedSurface=" + Get().pendingChangedSurface);
			return;
		}

		GameActivityForMakeAAR.GetCurrentActivityContext().runOnMainThread(() ->
			{
				synchronized (syncLock)
				{
					Log.i(TAG, "QueueResume is calling engine.onResume with lifecycleContextID=" + lifecycleContextID + ", reasonString=" + reasonString);
					
					Engine.Get().onResume(lifecycleContextID, "QueueResume with reasonString=" + reasonString);
				}
			}
		);
	}

	public static void QueueStart(String reasonString)
	{
		if (!GameActivityForMakeAAR.isValidGameActivity())
		{
			Log.e(TAG, "QueueStart called with invalid GameActivity!, lifecycleContextID=" + lifecycleContextID + ", reasonString=" + reasonString);
			return;
		}

		GameActivityForMakeAAR.GetCurrentActivityContext().runOnMainThread(new Runnable()
		{
			@Override
			public void run()
			{
				synchronized (syncLock)
				{
					Log.i(TAG, "QueueStart is calling engine.onStart() with lifecycleContextID=" + lifecycleContextID + ", reasonString=" + reasonString);
	
					Engine.Get().onStart(lifecycleContextID, "QueueStart with reasonString=" + reasonString);
				}
			}
		});
	}


	public static void QueueStop(String reasonString)
	{
		if (false == GameActivityForMakeAAR.isValidGameActivity())
		{
			Log.e(TAG, "QueueStop called with invalid GameActivity!, lifecycleContextID=" + lifecycleContextID + ", reasonString=" + reasonString);
			return;
		}

		GameActivityForMakeAAR.GetCurrentActivityContext().runOnMainThread(() ->
			{
				synchronized (syncLock) {
				Log.i(TAG, "QueueStop is calling engine.onStop with lifecycleContextID=" + lifecycleContextID + ", reasonString=" + reasonString);
				Engine.Get().onStop(lifecycleContextID, "QueueStop with reasonString=" + reasonString);
			}
		});
	}

	boolean bHasBeenInit = false;

	public boolean IsInitialized() { return  bHasBeenInit; }


	public boolean Init(Context context, GameActivitySetupInfo gameActivitySetupInfo, String commandline, String projectModule, String[] mountPAKs)
	{
		incCreateContextID();

		++logOrder;
		Log.i(TAG, "logOrder:" + logOrder + " * Engine::Init(context, gameActivitySetupInfo) -" + ", bLibraryLoaded=" + bLibraryLoaded + ", bResuming=" + bResuming + ", renderSurfaceValid=" + renderSurfaceValid + ", context = " + context);
		if (renderSurfaceValid || renderSurface != null)
		{
			releaseVolatileResources("Engine::Init(context, gameActivitySetupInfo) called, so remove old resources");
		}
		if (!bLibraryLoaded)
		{
			return false;
		}

		boolean bFirstInit = (GameActivity.Get() == null || renderSurface == null);

		if (!DoHousekeepingForViewChange(new SimpleContextWrapper(context), gameActivitySetupInfo))
		{
			return false;
		}

		bHasBeenInit = true;

		if (bFirstInit)
		{
			Log.i(TAG, "** Engine::Init(context, gameActivitySetupInfo) - bFirstInit"
					+ ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone
					+ ", bCalledStart:" + bCalledStart
					+ ", startCounter:" + startCounter
					+ ", bResuming:" + bResuming
					+ ", onPauseCounter:" + onPauseCounter
					+ ", renderSurfaceValid:" + renderSurfaceValid
					+ ", renderSurface:" + renderSurface
					+ ", pendingChangedSurface:" + pendingChangedSurface
					+ ", commandline:" + commandline
					+ ", projectModule:" + projectModule
			);

			if (commandline.isEmpty() || !commandline.startsWith("../../"))
			{
				commandline = "../../../" + projectModule + "/" + projectModule + ".uproject " + commandline;
				Log.i(TAG, "** Engine::Init(External View) - commandline updated: " + commandline);
			}

			GetOverrideGameActivity().setCommandline(commandline);

			if (mountPAKs.length > 0) {
				pendingPakList = mountPAKs.clone();
			}

			// start game thread
			if (!bGameActivityNativeMainIsDone)
			{
				Log.i(TAG, "** Engine::Init(context, gameActivitySetupInfo) - calling GetOverrideGameActivity().nativeMain() with : projectModule = " + projectModule);
				DoGameActivityNativeMain(projectModule, "Init(context, gameActivitySetupInfo)");
			}

			AndroidThunkJava_Engine_ReceiveEvent(EVENTTYPE_ACTIVITY_HAS_CHANGED, "", 0, 0, 0.0f);
		}


//		if (!bCalledStart)
//		{
//			QueueStart("Init(context, gameActivitySetupInfo)");
//		}
//
//		if (renderSurface != null || pendingChangedSurface != null)
//		{
//			QueueResume("Init(context, gameActivitySetupInfo) refresh surface");
//		}

		return bResuming;
	}


	public String getObbVersion(boolean bFull)
	{
		String rawComment = GetOverrideGameActivity().nativeGetObbComment();
		if (bFull)
		{
			return rawComment;
		}
		int separatorIndex = rawComment.indexOf(":");
		return (separatorIndex >= 0) ? rawComment.substring(0, separatorIndex) : "0.0.0";
	}

	private int getResourceId(String VariableName, String ResourceName, String PackageName)
	{
		Log.i(TAG, "** Engine::getResourceId(...), VariableName=" + VariableName + ", ResourceName=" + ResourceName + ", PackageName=" + PackageName );
		
		try {
			return GameActivityForMakeAAR.GetCurrentActivityContext().getApplicationContext().getResources().getIdentifier(VariableName, ResourceName, PackageName);
		}
		catch (Exception e) {
			e.printStackTrace();
			return -1;
		}
	}

	public String getAARVersion()
	{
		if (GameActivityForMakeAAR.isValidGameActivity() == false)
		{
			return "0.0.0";
		}

		try {
			int resourceId = getResourceId("UnrealEngineAARVersion", "string", activity.getPackageName());
			return (resourceId < 1) ? "0.0.0" : activity.getString(resourceId);
		}
		catch (Exception e)
		{
			e.printStackTrace();
			return "0.0.-1";
		}
	}

	public void allowConsole(boolean bEnable)
	{
		bAllowConsole = bEnable;
	}

	public void sendConsoleCommand(String command)
	{
		if (GetOverrideGameActivity() != null) 
		{
			if (!CheckEvent(EVENTTYPE_ENGINELOOP_INIT_COMPLETE))
			{
				Log.e(TAG, "Engine::sendConsoleCommand(aborted cause not ready), maskEngineEvents=" + Long.toBinaryString(maskEngineEvents) );
				return;
			}
			GetOverrideGameActivityWhenInitComplete().nativeConsoleCommand(command);
		}
	}

	private boolean bCalledStart = false;
	static int startCounter = 0;
	static int lifecycleContextID = 0;
	static int lifecycleActivityContextCounter = 0;

	public static int getCurrentContextID()
	{
		return lifecycleContextID;
	}

	//public int getCreateContext()
	//{
	//	return lifecycleContextID++;
	//}

	public void incCreateContextID()
	{
		++logOrder;
		Log.w(TAG, "[lifecycleContextID=" + lifecycleContextID + "] logOrder:" + logOrder + " ** Engine::incCreateContextID(), "
				+ ", lifecycleActivityContextCounter=" + lifecycleActivityContextCounter
				+ ", bResuming=" + bResuming
				+ ", renderSurfaceValid=" + renderSurfaceValid
				+ ", surfaceCreatedCounter = " + surfaceCreatedCounter
				+ ", renderSurface = " + renderSurface
		);
		if (startCounter > 0 && GameActivityForMakeAAR.isValidGameActivity())
		{
			onStop(lifecycleContextID, "incCreateContextID");
		}
		lifecycleContextID = (lifecycleContextID%100) + (++lifecycleActivityContextCounter) * 100;
	}

	public void onPause(int contextID, String reasonString)
	{
		if (!CheckEvent(EVENTTYPE_ENGINELOOP_INIT_COMPLETE))
		{
			Log.e(TAG, "Engine::onPause(aborted cause not ready), maskEngineEvents=" + Long.toBinaryString(maskEngineEvents) );
			return;
		}
		
		if (contextID != 0 && lifecycleContextID != 0 && contextID != lifecycleContextID) {
			Log.e(TAG, "[lifecycleContextID=" + lifecycleContextID + "] contextID:" + contextID + " ABNORMAL Engine::onPause(contextID) - onPause called. with contextID change = " + contextID + ", reasonString=" + reasonString);
			return;
		}

		if (!bHasBeenInit) {
			Log.e(TAG, "[lifecycleContextID=" + lifecycleContextID + "] logOrder:" + logOrder + " * Engine::onPause - BUT bHasBeenInit is FALSE, DO NOTHING. renderSurfaceValid: " + renderSurfaceValid + ", bResuming: " + bResuming + ", bCalledStart = " + bCalledStart + ", onPauseCounter=" + onPauseCounter + ", renderSurface = " + renderSurface + ", reasonString=" + reasonString);
			return;
		}

		Log.i(TAG, "[lifecycleContextID=" + lifecycleContextID + "] logOrder:" + logOrder + " * Engine::onPause - renderSurfaceValid: " + renderSurfaceValid + ", bResuming: " + bResuming + ", bCalledStart = " + bCalledStart + ", onPauseCounter=" + onPauseCounter + ", renderSurface = " + renderSurface + ", pendingChangedSurface=" + pendingChangedSurface + ", reasonString=" + reasonString);

		if (GetOverrideGameActivity() != null) {
			GetOverrideGameActivityWhenInitComplete().onPause();
		}

		if (onPauseCounter > 0 || bResuming) {
			Log.w(TAG, "** Engine::onPause - Already Paused! onPauseCounter=" + onPauseCounter + ", bResuming=" + bResuming + ", reasonString=" + reasonString);
			return;
		}

		++onPauseCounter;
		Log.i(TAG, "** Engine::onPause - DOING a full Pause!" + ", reasonString=" + reasonString);
		GetOverrideGameActivityWhenInitComplete().nativeAppCommand(APP_CMD_PAUSE);
		GetOverrideGameActivityWhenInitComplete().nativeAppCommand(APP_CMD_LOST_FOCUS);

		if (!bResuming && renderSurfaceValid)
		{
			GetOverrideGameActivityWhenInitComplete().nativeSetSurfaceOverride(null, renderSurfaceViewPosition[0], renderSurfaceViewPosition[1]);

			if (renderSurface != null) 
			{
				pendingChangedSurface = renderSurface;
				// com.epicgames.makeaar.GameActivity.nativeAppCommand(APP_CMD_TERM_WINDOW);
			}

			bResuming |= pendingChangedSurface != null;
	
			renderSurface = null;
			renderSurfaceValid = false;
		}

		//releaseVolatileResources("onPause called with reason=" + reasonString);
	}

	public void onResume(int contextID, String reasonString)
	{
		if (contextID != 0 && lifecycleContextID != 0 && contextID != lifecycleContextID) {
			Log.e(TAG, "[lifecycleContextID=" + lifecycleContextID + "] contextID:" + contextID + " ABNORMAL Engine::onResume - onResume called. with contextID change = " + contextID + ", reasonString=" + reasonString);
			//return;
		}

		lifecycleContextID = contextID;
		onResume(reasonString);
	}

	private void onResume(String reasonString)
	{
		if (!CheckEvent(EVENTTYPE_ENGINELOOP_INIT_COMPLETE))
		{
			Log.e(TAG, "Engine::onResume(WARNING cause not ready), maskEngineEvents=" + Long.toBinaryString(maskEngineEvents) + ", reasonString=" + reasonString );
			//ApplyPendingSurfaceChanges();
			//return;
		}
		
		++logOrder;
		Log.i(TAG, "[lifecycleContextID=" + lifecycleContextID + "] logOrder:" + logOrder + " * Engine::onResume() - onResume called. bCalledStart was = " + bCalledStart
			+ ", bHasBeenInit = " + bHasBeenInit
			+ ", maskEngineEvents= " + Long.toBinaryString(maskEngineEvents)
			+ ", bGameActivityNativeMainIsDone = " + bGameActivityNativeMainIsDone
			+ ", prev startCounter = " + startCounter
			+ ", surfaceCreatedCounter = " + surfaceCreatedCounter
			+ ", bResuming was = " + bResuming
			+ ", onPauseCounter=" + onPauseCounter
			+ ", renderSurfaceValid: " + renderSurfaceValid
			+ ", renderSurface = " + renderSurface
		);

		if (bCalledStart == false) {
			if (surfaceCreatedCounter > 0 && renderSurface != null) {
				Log.w(TAG, "** Engine::onResume() - FORCING onstart from onresume because bCalledStart is false! bResuming=" + bResuming + ", onPauseCounter=" + onPauseCounter + ", width = " + renderSurfaceViewSize[0] + ", height = " + renderSurfaceViewSize[1]);

				onStart(reasonString + "->onResume");

			} else {
				Log.w(TAG, "** Engine::onResume() - SKIPPING because bCalledStart is false! bResuming=" + bResuming + ", onPauseCounter=" + onPauseCounter + ", width = " + renderSurfaceViewSize[0] + ", height = " + renderSurfaceViewSize[1]);
				return;
			}
		}

		if (GetOverrideGameActivity() != null && CheckEvent(EVENTTYPE_ENGINELOOP_INIT_COMPLETE)) {
			GetOverrideGameActivity().onResume();
		}


		bResuming |= ApplyPendingSurfaceChanges();

		if (renderSurface == null || !renderSurfaceValid)
		{
			Log.i(TAG, "** Engine::onResume - no valid surface to resume... bResuming=" + bResuming + ", surfaceCreatedCounter" + surfaceCreatedCounter + ", onPauseCounter=" + onPauseCounter + ", renderSurfaceValid = " + renderSurfaceValid + ", renderSurface = " + renderSurface);

			return;
		}

		Log.i(TAG, "** Engine::onResume - CHECKING resume cases... bResuming=" + bResuming + ", onPauseCounter=" + onPauseCounter + ", width = " + renderSurfaceViewSize[0] + ", height = " + renderSurfaceViewSize[1]);

		onPauseCounter = onPauseCounter > 0 ? --onPauseCounter : onPauseCounter;

		GetOverrideGameActivity().nativeAppCommand(APP_CMD_RESUME);

		bResuming |= onPauseCounter > 0;

		if (!bResuming && (bCalledStart && surfaceCreatedCounter>0 && renderSurfaceValid && renderSurface != null))
		{
			Log.i(TAG, "** Engine::onResume - setup renderSurface because we already are in a start state! width = " + renderSurfaceViewSize[0] + ", height = " + renderSurfaceViewSize[1]);

			GetOverrideGameActivityWhenInitComplete().nativeSetSurfaceOverride(renderSurface, renderSurfaceViewPosition[0], renderSurfaceViewPosition[1]);
			GetOverrideGameActivityWhenInitComplete().nativeSetSurfaceViewInfo(renderSurfaceViewSize[0], renderSurfaceViewSize[1]);
			bResuming = true;
		}

		if (bResuming && renderSurface != null) {
			Log.i(TAG, "** Engine::onResume - DOING full Resume! width = " + renderSurfaceViewSize[0] + ", height = " + renderSurfaceViewSize[1] + ", maskEngineEvents= " + maskEngineEvents);

			GetOverrideGameActivityWhenInitComplete().nativeAppCommand(APP_CMD_INIT_WINDOW);
			GetOverrideGameActivityWhenInitComplete().nativeAppCommand(APP_CMD_GAINED_FOCUS);
			GetOverrideGameActivityWhenInitComplete().nativeResumeMainInit();
			bResuming = false;
		}
		else if (renderSurface != null)
		{
			Log.e(TAG, "***- Engine::onResume - UNHANDLED case for resume! renderSurfaceValid = " + renderSurfaceValid + ", renderSurface = " + renderSurface);
			GetOverrideGameActivityWhenInitComplete().nativeSetSurfaceOverride(renderSurface, renderSurfaceViewPosition[0], renderSurfaceViewPosition[1]);
			GetOverrideGameActivityWhenInitComplete().nativeSetSurfaceViewInfo(renderSurfaceViewSize[0], renderSurfaceViewSize[1]);
			GetOverrideGameActivityWhenInitComplete().nativeAppCommand(APP_CMD_INIT_WINDOW);
			GetOverrideGameActivityWhenInitComplete().nativeAppCommand(APP_CMD_GAINED_FOCUS);
			GetOverrideGameActivityWhenInitComplete().nativeResumeMainInit();
		}
		else
		{
			Log.e(TAG, "***- Engine::onResume - EMPTY unhandled case for resume! renderSurfaceValid = " + renderSurfaceValid + ", renderSurface = " + renderSurface);
		}
	}

	public void onStart(int contextID, String reasonString)
	{
		if (contextID != 0 && lifecycleContextID != 0 && contextID != lifecycleContextID) {
			Log.e(TAG, "[lifecycleContextID=" + lifecycleContextID + "] contextID:" + contextID + " ABNORMAL Engine::onStart(contextID) - onStart called. with contextID change = " + contextID + ", reasonString=" + reasonString);
			//return;
		}

		lifecycleContextID = contextID;
		onStart(reasonString);
	}

	private void onStart(String reasonString)
	{
		++logOrder;
		Log.i(TAG, "[lifecycleContextID=" + lifecycleContextID + "] logOrder:" + logOrder + " * Engine::onStart - onStart called. bCalledStart was = " + bCalledStart
			+ "\n\t, maskEngineEvents= " + Long.toBinaryString(maskEngineEvents)
			+ ", bHasBeenInit = " + bHasBeenInit
			+ ", prev startCounter = " + startCounter
			+ ", bResuming was = " + bResuming
			+ ", onPauseCounter=" + onPauseCounter
			+ ", renderSurfaceValid: " + renderSurfaceValid
			+ ", renderSurface = " + renderSurface
		);

		if (!bHasBeenInit) {
			//RegisterLifecycleEvent( Lifecycle_Start );
			//return;
		}

		++startCounter;
		if (!bCalledStart)
		{
			//GetOverrideGameActivity().nativeResumeMainInit();
			//GetOverrideGameActivity().nativeAppCommand(APP_CMD_INIT_WINDOW);
			//GetOverrideGameActivity().nativeAppCommand(APP_CMD_GAINED_FOCUS);
			bCalledStart = true;
			GetOverrideGameActivity().nativeAppCommand(APP_CMD_START);
		}
	}

	public void onStop(int contextID, String reasonString)
	{
		if (contextID != 0 && lifecycleContextID != 0 && contextID != lifecycleContextID) {
			Log.e(TAG, "[lifecycleContextID=" + lifecycleContextID + "] contextID:" + contextID + " ABNORMAL Engine::onStop(contextID) - onStop called. with contextID change = " + contextID + ", reasonString=" + reasonString);
			//return;
		}
		lifecycleContextID = contextID;
		onStop(reasonString);
	}

	private void onStop(String reasonString)
	{
		++logOrder;
		Log.v(TAG, "[lifecycleContextID=" + lifecycleContextID + "] logOrder:" + logOrder + " * Engine::onStop - onStop called. bCalledStart was = " + bCalledStart
			+ "\n\t, maskEngineEvents= " + Long.toBinaryString(maskEngineEvents)
				+ ", reasonString=" + reasonString
				+ ", bHasBeenInit = " + bHasBeenInit
				+ ", prev startCounter = " + startCounter
				+ ", surfaceCreatedCounter = " + surfaceCreatedCounter
				+ ", bResuming was = " + bResuming
				+ ", onPauseCounter=" + onPauseCounter
				+ ", renderSurfaceValid: " + renderSurfaceValid
				+ ", renderSurface = " + renderSurface
		);

		releaseVolatileResources("onStop withReason <- " + reasonString);
		synchronized (syncLock) {
			pendingChangedSurface = null;
		}
		
		if (startCounter>0) {
			--startCounter;
		}

		if (bCalledStart)
		{
			GetOverrideGameActivityWhenInitComplete().nativeAppCommand(APP_CMD_STOP);

			bCalledStart = false;
			startCounter = 0;
			onPauseCounter = 0;
		}
	}

	private void ReleaseRenderSurfaces(int contextID, String reasonString)
	{

		if (contextID != 0 && lifecycleContextID != 0 && contextID != lifecycleContextID) {
			Log.e(TAG, "[lifecycleContextID=" + lifecycleContextID + "] contextID:" + contextID + " ABNORMAL Engine::releaseVolatileResources() called. with contextID change = " + contextID + ", reasonString=" + reasonString);
			//return;
		}

		lifecycleContextID = contextID;
		ReleaseRenderSurfaces(reasonString);
	}

	private void ReleaseRenderSurfaces(String reasonString)
	{
		synchronized (syncLock) {
			if (GameActivity.isValidGameActivity()) {
		        Log.i(TAG, "*** Engine::ReleaseRenderSurfaces() : GameActivityForMakeAAR.isValidGameActivity() = " + GameActivityForMakeAAR.isValidGameActivity()
					+ "\n\t, maskEngineEvents= " + Long.toBinaryString(maskEngineEvents)
			        + ", bGameActivityNativeMainIsDone=" + bGameActivityNativeMainIsDone
			        + ", lifecycleContextID:" + lifecycleContextID
			        + ", reasonString:" + reasonString
			        + ", bCalledStart:" + bCalledStart
			        + ", startCounter:" + startCounter
			        + ", bResuming:" + bResuming
			        + ", onPauseCounter:" + onPauseCounter
			        + ", renderSurfaceValid:" + renderSurfaceValid
			        + ", renderSurface:" + renderSurface
			        + ", surfaceCreatedCounter:" + surfaceCreatedCounter
			        + ", pendingChangedSurface:" + pendingChangedSurface

		        );
				        
		        --surfaceCreatedCounter;

		        if (renderView != null) {
			        renderView.Detach();
			        renderView = null;
		        }

				GetOverrideGameActivityWhenInitComplete().nativeSetSurfaceOverride(pendingChangedSurface, renderSurfaceViewPosition[0], renderSurfaceViewPosition[1]);
		        if (renderSurface != null && (onPauseCounter > 0 || startCounter > 0)) {
					GetOverrideGameActivityWhenInitComplete().nativeAppCommand(APP_CMD_TERM_WINDOW);
		        }

				if (renderSurfaceValid && (startCounter > 0 || onPauseCounter > 0)) {
					bResuming = true;
				}
                else {
	                bCalledStart = false;
                }

				renderSurface = pendingChangedSurface;
				renderSurfaceValid = renderSurface != null;
				pendingChangedSurface = null;
			}
		}
	}

	private void releaseVolatileResources(int contextID, String reasonString)
	{
		if (contextID != 0 && lifecycleContextID != 0 && contextID != lifecycleContextID) {
			Log.e(TAG, "[lifecycleContextID=" + lifecycleContextID + "] contextID:" + contextID + " ABNORMAL Engine::releaseVolatileResources() called. with contextID change = " + contextID + ", reasonString=" + reasonString);
			//return;
		}

		lifecycleContextID = contextID;
		releaseVolatileResources(reasonString);
	}

	private void releaseVolatileResources(String reasonString)
	{
		Log.i(TAG, "logOrder:" + logOrder + " * Engine::releaseVolatileResources() - bCalledStart was = " + bCalledStart + ", lifecycleContextID=" + lifecycleContextID + ", reasonString=" + reasonString + ", bResuming was = " + bResuming + ", onPauseCounter=" + onPauseCounter + ", renderSurfaceValid: " + renderSurfaceValid + ", renderSurface = " + renderSurface );

		ReleaseRenderSurfaces(lifecycleContextID, reasonString);
	}

	public void onDestroy(int customContextID, String reasonString)
	{
		++logOrder;
		Log.i(TAG, "[lifecycleContextID=" + lifecycleContextID + "] logOrder:" + logOrder + " * Engine::onDestroy() - onDestroy called. bCalledStart was = " + bCalledStart + ", bResuming was = " + bResuming + ", onPauseCounter=" + onPauseCounter + ", renderSurfaceValid: " + renderSurfaceValid + ", renderSurface = " + renderSurface );

		if (customContextID == 543210)
		{
			// we don't want to call APP_CMD_DESTROY unless we want to completely terminate the unreal engine instance.
			GetOverrideGameActivityWhenInitComplete().nativeAppCommand(APP_CMD_DESTROY);
		}

		maskEngineEvents = 0;
		bHasBeenInit = false;

		lifecycleContextID += 25;
		if (onPauseCounter>0)
		{
			QueueResume("onDestroy withReason <- " + reasonString);
		}
	}

	public void onConfigurationChanged(Configuration newConfig)
	{
		// forward the orientation
		boolean bPortrait = newConfig.orientation == Configuration.ORIENTATION_PORTRAIT;
		GetOverrideGameActivityWhenInitComplete().nativeOnOrientationChanged(newConfig.orientation);
	}

	public void onActivityResult(int requestCode, int resultCode, Intent data)
	{
		++logOrder;
		Log.v(TAG, "logOrder:" + logOrder + " * Engine::onActivityResult() - bCalledStart was = " + bCalledStart + ", bResuming was = " + bResuming + ", onPauseCounter=" + onPauseCounter + ", renderSurfaceValid: " + renderSurfaceValid + ", renderSurface = " + renderSurface );

		// only used by FOnlineSubsystemGooglePlay so we changed signature to pass the ContextWrapper instead of activity here because we no longer want to hold onto any activity references...
		GetOverrideGameActivityWhenInitComplete().nativeOnActivityResult(GameActivity.Get(), requestCode, resultCode, data);
		//		??onResume();
	}

	public boolean onTouchEvent(MotionEvent event)
	{
		int pointerCount = event.getPointerCount();
		if (pointerCount < 1 || !GameActivityForMakeAAR.isValidGameActivity())
		{
			return false;
		}
		int device = event.getDeviceId();

		int action = 0;
		boolean bProcessed = false;
		for (int index = 0; index < pointerCount; index++) {
			int pointerId = event.getPointerId(index);
			int x = (int)event.getX();
			int y = (int)event.getY();
			switch (event.getActionMasked()) {
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					action = TOUCHTYPE_BEGIN;
					break;
				case MotionEvent.ACTION_MOVE:
					action = TOUCHTYPE_MOVE;
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
				case MotionEvent.ACTION_CANCEL:
				case MotionEvent.ACTION_OUTSIDE:
					action = TOUCHTYPE_END;
					break;
				case MotionEvent.ACTION_SCROLL:
				case MotionEvent.ACTION_HOVER_ENTER:
				case MotionEvent.ACTION_HOVER_MOVE:
				case MotionEvent.ACTION_HOVER_EXIT:
					continue;
				default:
					continue;
			}

			if (GetOverrideGameActivity().nativeInputTouch(device, action, pointerId, x, y) == 1) {
				bProcessed = true;
			}
		}

		if (bAllowConsole && (pointerCount >= 4) && (action == TOUCHTYPE_BEGIN)) {
			showConsole();
		}

		return bProcessed;
	}

	public void showConsole()
	{
		if (GameActivityForMakeAAR.isValidGameActivity()) {
			GetOverrideGameActivity().AndroidThunkJava_ShowConsoleWindow(null);
		}
	}

	public static void registerEventCallback(IEventCallback callback)
	{
		if (null == callback) {
			mCallbackRef = new WeakReference<>(IEventCallback.NO_OP);
		} else {
			mCallbackRef = new WeakReference<>(callback);
		}
	}

	public static void AndroidThunkJava_Engine_ReceiveEvent(int event, String param1, int param2, int param3, float param4)
	{
		if (event == EVENTTYPE_INIT) {
			String AARVersion = Engine.Get().getAARVersion();
			String OBBVersion = Engine.Get().getObbVersion(false);

			Log.d(TAG, "*** Engine::ReceiveEvent - INIT - AARVersion=" + AARVersion + ", OBBVersion=" + OBBVersion);
			if (!AARVersion.equals(OBBVersion)) {
				Log.w(TAG, "*** Engine::ReceiveEvent - AARVersion and OBBVersion mismatch!");
			}

			if (pendingPakList != null) {
				// mount any pending pak files with higher priority
				for (String pakFile : pendingPakList) {
					nativeMountPak(pakFile, 1, null);
				}
				pendingPakList = null;
			}
		}
		else if (event == EVENTTYPE_ENGINELOOP_INIT_COMPLETE) {
			Log.d(TAG, "*** Engine::ReceiveEvent - event = " + EVENTTYPE_ENGINELOOP_INIT_COMPLETE + ", param1 =" + param1);
		}
		else if (event == EVENTTYPE_POST_ENGINE_INIT) {
			Log.d(TAG, "*** Engine::ReceiveEvent - event = " + EVENTTYPE_POST_ENGINE_INIT + ", param1 =" + param1);
		}
		else if (event == EVENTTYPE_POST_LOAD_MAP) {
			Log.d(TAG, "*** Engine::ReceiveEvent - event = " + EVENTTYPE_POST_LOAD_MAP + ", param1 =" + param1);
		}
		else if (event == EVENTTYPE_PRE_LOAD_MAP) {
			Log.d(TAG, "*** Engine::ReceiveEvent - event = " + EVENTTYPE_PRE_LOAD_MAP + ", param1 =" + param1);
		}
		else if (event == EVENTTYPE_ACTIVITY_HAS_CHANGED) {
			Log.d(TAG, "*** Engine::ReceiveEvent - event = " + EVENTTYPE_ACTIVITY_HAS_CHANGED + ", param1 =" + param1);
		}
		else  if (event != EVENTTYPE_FRAME_BEGIN && event != EVENTTYPE_FRAME_END)
		{
			Log.d(TAG, "*** Engine::ReceiveEvent - event = " + event + ", param1 =" + param1);
		}

		maskEngineEvents |= 1<<event;

		final IEventCallback callback = mCallbackRef.get();
		if (callback != null) {
			callback.eventCallback(event, param1, param2, param3, param4);
		}
	}

	public void sendEvent(int event, String param1, int param2, int param3, float param4)
	{
		nativeEventReceived(event, param1, param2, param3, param4);
	}

	public void sendData(int dataId, String key, String value)
	{
		nativeDataReceivedString(dataId, key, value);
	}

	public void sendData(int dataId, String key, String[] value)
	{
		nativeDataReceivedStringArray(dataId, key, value);
	}

	public void sendData(int dataId, String key, boolean value)
	{
		nativeDataReceivedBoolean(dataId, key, value);
	}

	public void sendData(int dataId, String key, boolean[] value)
	{
		nativeDataReceivedBooleanArray(dataId, key, value);
	}

	public void sendData(int dataId, String key, byte value)
	{
		nativeDataReceivedByte(dataId, key, value);
	}

	public void sendData(int dataId, String key, byte[] value)
	{
		nativeDataReceivedByteArray(dataId, key, value);
	}

	public void sendData(int dataId, String key, int value)
	{
		nativeDataReceivedInt(dataId, key, value);
	}

	public void sendData(int dataId, String key, int[] value)
	{
		nativeDataReceivedIntArray(dataId, key, value);
	}

	public void sendData(int dataId, String key, Short value)
	{
		nativeDataReceivedShort(dataId, key, value);
	}

	public void sendData(int dataId, String key, Short[] value)
	{
		nativeDataReceivedShortArray(dataId, key, value);
	}

	public void sendData(int dataId, String key, Long value)
	{
		nativeDataReceivedLong(dataId, key, value);
	}

	public void sendData(int dataId, String key, Long[] value)
	{
		nativeDataReceivedLongArray(dataId, key, value);
	}

	public void sendData(int dataId, String key, float value)
	{
		nativeDataReceivedFloat(dataId, key, value);
	}

	public void sendData(int dataId, String key, float[] value)
	{
		nativeDataReceivedFloatArray(dataId, key, value);
	}

	public void sendData(int dataId, String key, Double value)
	{
		nativeDataReceivedDouble(dataId, key, value);
	}

	public void sendData(int dataId, String key, Double[] value)
	{
		nativeDataReceivedDoubleArray(dataId, key, value);
	}

	public boolean mountPak(String pakFile, int order, String mountPoint)
	{
		if (pakFile != null) {
			return nativeMountPak(pakFile, order, mountPoint == null ? "" : mountPoint);
		}
		return false;
	}

	public boolean checkPak(String pakFile)
	{
		if (pakFile != null) {
			return nativeCheckPak(pakFile);
		}
		return false;
	}

	public static native void nativeEventReceived(int event, String param1, int param2, int param3, float param4);
	public static native void nativeDataReceivedString(int dataId, String key, String value);
	public static native void nativeDataReceivedStringArray(int dataId, String key, String[] value);
	public static native void nativeDataReceivedBoolean(int dataId, String key, boolean value);
	public static native void nativeDataReceivedBooleanArray(int dataId, String key, boolean[] value);
	public static native void nativeDataReceivedByte(int dataId, String key, byte value);
	public static native void nativeDataReceivedByteArray(int dataId, String key, byte[] value);
	public static native void nativeDataReceivedInt(int dataId, String key, int value);
	public static native void nativeDataReceivedIntArray(int dataId, String key, int[] value);
	public static native void nativeDataReceivedShort(int dataId, String key, Short value);
	public static native void nativeDataReceivedShortArray(int dataId, String key, Short[] value);
	public static native void nativeDataReceivedLong(int dataId, String key, Long value);
	public static native void nativeDataReceivedLongArray(int dataId, String key, Long[] value);
	public static native void nativeDataReceivedFloat(int dataId, String key, float value);
	public static native void nativeDataReceivedFloatArray(int dataId, String key, float[] value);
	public static native void nativeDataReceivedDouble(int dataId, String key, Double value);
	public static native void nativeDataReceivedDoubleArray(int dataId, String key, Double[] value);
	public static native boolean nativeMountPak(String pakFile, int order, String mountPoint);
	public static native boolean nativeCheckPak(String pakFile);
}