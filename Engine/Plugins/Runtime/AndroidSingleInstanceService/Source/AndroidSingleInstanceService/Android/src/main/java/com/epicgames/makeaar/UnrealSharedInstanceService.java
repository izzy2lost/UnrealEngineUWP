package com.epicgames.makeaar;

import android.app.Activity;
import android.app.Application;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Point;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.provider.Settings;
import android.util.Log;
import android.view.Surface;
import android.view.TextureView;
import android.widget.Toast;

import com.epicgames.makeaar.Engine;
import com.epicgames.makeaar.EngineFactory;
import com.epicgames.makeaar.GameActivityForMakeAAR;
import com.epicgames.makeaar.GameActivitySetupInfo;
import com.epicgames.unreal.SimpleContextWrapper;
import com.epicgames.unreal.GameActivity;

import com.epicgames.makeaar.UnrealMessageType;

import java.lang.ref.PhantomReference;
import java.lang.ref.WeakReference;

public class UnrealSharedInstanceService extends Service {

	private static final String TAG = "UE_ASIS_SharedInstanceService";

	static int activeTaskID = 0;
	/**
	* Target we publish for clients to send messages to IncomingHandler.
	*/
	Messenger mMessenger;
	public String Name()
	{
		return getApplication().getClass().getSimpleName();
	}
	public static String Name(Context context)
	{
		return context.getPackageName() +",context=" + context.toString();
	}

	public UnrealSharedInstanceService() {
	}

	/**
	 * Handler of incoming messages from clients.
	 */
	static class IncomingHandler extends Handler implements Engine.IEventCallback
	{
		private Service applicationContext;
		static private Engine engineInstance;


		public static String defaultProjectModuleName = "UNDEFINED_PackageName";
//		public static String defaultProjectModuleName = "Alpha_POC";
		public static String defaultOBBFileLocation = "";//"Gallery3D"; //"Alpha_POC";
		//	public final static String defaultProjectModuleName = "";
//		public static String defaultCommandLine = "../../../" + BuildConfig.OBB_MODULE_NAME + "/" + BuildConfig.OBB_MODULE_NAME + ".uproject -nosound -nocrashreports";
		public static String defaultCommandLine = "-nosound -nocrashreports -dpcvars=a.UseSwappyForFramePacing=0 -tracehost=127.0.0.1 -cpuprofilertrace -statnamedevents -trace=Bookmark,Frame,CPU,GPU,LoadTime,File";
//		public final static Engine gUnrealEngine = EngineFactory.getInstance(null, defaultOBBFileLocation, defaultProjectModuleName, true);
//		static Engine gUnrealEngine;
		public static Messenger mServiceReply;




		void Release()
		{
			engineInstance.registerEventCallback(null);
			mServiceReply = null;
		}

		final static void SetOBB(String commandLineArgs, String obbModuleName, String obbFileLocation)
		{
			if (commandLineArgs.isEmpty())
				commandLineArgs = "-nosound -nocrashreports";
			if (!obbModuleName.isEmpty()) {
				defaultProjectModuleName = obbModuleName;
				defaultCommandLine = "../../../" + defaultProjectModuleName + "/" + defaultProjectModuleName + ".uproject " + commandLineArgs;
//			if (!obbFileLocation.isEmpty())
				defaultOBBFileLocation = obbFileLocation;
			}

		}

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
		
		IncomingHandler(Service context, Intent intent) {
		    mServiceReply = intent.getParcelableExtra("callback");
			applicationContext = context;

		    if (intent.hasExtra("obbModuleName"))
		    {
			    Bundle data = intent.getExtras();
			    String obbModuleName = data.getString("obbModuleName", "");
			    String obbFileLocation = data.getString("obbFileLocation", "");
			    String commandLineArgs = data.getString("commandLineArgs", "");
			    SetOBB(commandLineArgs, obbModuleName, obbFileLocation);
    
			    Log.i("UESharedInstanceServiceNative", "**\tIncomingHandler(constructor intent) - proc=" + Name(context)
				    + ", obbModuleName=" + obbModuleName
				    + ", obbFileLocation=" + obbFileLocation);
		    }
		    
		    if (engineInstance == null) {
			    engineInstance = EngineFactory.getInstance(null, defaultOBBFileLocation, defaultProjectModuleName, true);
		    }
		    
		    engineInstance.registerEventCallback(this);
		    engineInstance.allowConsole(true);
		}

		@Override
		public void eventCallback(int event, String param1, int param2, int param3, float param4)
		{

			if (event != 3 && event != 4) {
				Log.d(TAG, "eventCallback: " + event);
			}

			if (event == Engine.EVENTTYPE_FRAME_BEGIN) {


			}

			if (event == Engine.EVENTTYPE_FRAME_END) {

			}

			if (event == Engine.EVENTTYPE_POST_ENGINE_INIT)
			{


			}

			if (event == Engine.EVENTTYPE_PRE_LOAD_MAP)
			{

			}

			if (event == Engine.EVENTTYPE_POST_LOAD_MAP) {

				//Log.i(TAG, "Configure unlocked FPS");
				//engine.sendConsoleCommand("rhi.SyncInterval=1"); // 0 = unlocked, 1 = 60 fps, 2 = 30 fps
				//engine.sendConsoleCommand("r.Vsync=0");
				//engine.sendConsoleCommand("r.GTSyncType=1"); // will cause the game thread to sync with the RHI thread (rather than the rendering thread), which caps the upper limit of input latency to something more sensible



			} else if (event == Engine.EVENTTYPE_ACTION) {

				Log.d(TAG, "eventCallback: " + event);
			}

			if (mServiceReply != null) {
				try {
					Message replyMsg = Message.obtain();
					replyMsg.what = event;
					if (param1 != null && !param1.isEmpty())
					{
//						Log.d(TAG, "writing parceable eventCallback: " + event + ", param1=" + param1);
						replyMsg.getData().putString("param1", param1);

//						replyMsg.obj = new UEReplyEvent(event, param1, param2, param3, param4);
					}
					replyMsg.arg1 = param2;
					replyMsg.arg2 = param3;

					if (param4 != 0.0f)
					{
//						Log.d(TAG, "writing parceable eventCallback: " + event + ", param4=" + param4);
						replyMsg.getData().putFloat("param4", param4);
					}
					mServiceReply.send(replyMsg);
				} catch (RemoteException e) {
					e.printStackTrace();
				}
			}
		}

		@Override
		public void handleMessage(Message msg) {


			if (msg.what == UnrealMessageType.Hello.ordinal()) {
				Toast.makeText(applicationContext, "hello!", Toast.LENGTH_SHORT).show();
			}
			else if (msg.what == UnrealMessageType.AttachExternalSurface.ordinal())
			{
				Bundle data = msg.getData();
				Surface externalSurface = data.getParcelable("surface");
				int taskId = data.getInt("taskId", 0);
				boolean bDoStart = data.getBoolean("start", true);
				boolean bDoResume = data.getBoolean("resume", true);

				Log.i(TAG, "handleMessage(MSG_ATTACH_EXTERNAL_SURFACE) - proc=" + Name(this.applicationContext)
					+ ", taskId=" + taskId
					+ ", activeTaskID=" + activeTaskID
					+ ", externalSurface=" + externalSurface
					+ ", applicationContext=" + applicationContext);

				int[] emptyViewSize = {0,0};
				int[] viewSize = emptyViewSize;
				int[] viewPos = emptyViewSize;

				if (data.containsKey("viewSize"))
				{
					viewSize = data.getIntArray("viewSize");
				}
				if (data.containsKey("viewPosition")) {
					viewPos = data.getIntArray("viewPosition");
				}
				boolean overridePropagateAlpha = data.containsKey("enablePropagateAlpha");
				boolean enablePropagateAlpha = data.getBoolean("enablePropagateAlpha", false);

				engineInstance.registerEventCallback(this);

				//if (bDoStart)
				{
					//engineInstance.QueueStart("UnrealMessageType.AttachExternalSurface, taskId=" + taskId); //777
				}

				if (externalSurface != null && externalSurface.isValid() && engineInstance != null) {
					if (engineInstance.IsInitialized() == false) {
						SimpleContextWrapper simpleActivity = new SimpleContextWrapper(applicationContext);

						GameActivitySetupInfo gameActivitySetupInfo = new GameActivitySetupInfo(externalSurface, simpleActivity, defaultOBBFileLocation, enablePropagateAlpha);
						gameActivitySetupInfo.ViewPosition = viewPos;
						gameActivitySetupInfo.ViewSize = viewSize;

						bDoResume &= engineInstance.Init(simpleActivity, gameActivitySetupInfo, defaultCommandLine, defaultProjectModuleName, new String[]{});
					} else {
						if (overridePropagateAlpha) {
							final int DepthBufferPreference = 32;
							final int PropagateAlpha = enablePropagateAlpha ? 1 : 0;
							boolean bPortrait = applicationContext.getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;

							GameActivity.Get().nativeSetWindowInfo(bPortrait, DepthBufferPreference, PropagateAlpha);
						}

						bDoResume &= engineInstance.AttachExternalRenderSurface(taskId, externalSurface, viewPos, viewSize);
					}

					
					//if (bDoResume)
					{
						engineInstance.QueueResume("UnrealMessageType.AttachExternalSurface, taskId=" + taskId);
					}

					activeTaskID = taskId;

					if (msg.replyTo != null) {
						Log.w(TAG, "Sending message for engine init back to message replyTo obj=" + engineInstance);
						Message replyMsg = Message.obtain();
						replyMsg.what = Engine.EVENTTYPE_POST_ENGINE_INIT;
						msg.arg1 = activeTaskID;
//							msg.obj = localBinder;
//							Bundle rdata = new Bundle();
//							rdata.putBundle("key", engineInstance);
//							msg.setData(); = engineInstance;

//						    try {
//							    mServiceReply.send(replyMsg);
//						    } catch (RemoteException e) {
//							    e.printStackTrace();
//						    }
					}
					else {
						Log.w(TAG, "Sending message for engine init using localBinder.handler=" + this);

//						    Message replyMsg = this.obtainMessage(Engine.EVENTTYPE_POST_ENGINE_INIT, engineInstance);
//						    this.sendMessage(replyMsg);
					}
				}
			}
			else if (msg.what == UnrealMessageType.DetachExternalSurface.ordinal())
			{
				Bundle data = msg.getData();

				int taskId = data.getInt("taskId", 0);
				boolean bDoStop = data.getBoolean("stop", true);
				boolean bDoPause = data.getBoolean("pause", true);

				Log.i(TAG, "handleMessage(MSG_DETACH_EXTERNAL_SURFACE) - proc=" + Name(applicationContext)
					+ ", taskId=" + taskId
					+ ", activeTaskID=" + activeTaskID
					+ ", applicationContext=" + applicationContext);

				//if (taskId != 0 && taskId == activeTaskID )
				{

					//if (bDoPause)
					{
						engineInstance.onPause(engineInstance.getCurrentContextID(), "UnrealMessageType.DetachExternalSurface, taskId=" + taskId);
					}

					//if (bDoStop)
					{
						engineInstance.onStop(engineInstance.getCurrentContextID(), "UnrealMessageType.DetachExternalSurface, taskId=" + taskId);
					}

					//engineInstance.sendConsoleCommand("t.maxfps 0.001");
					
				}
				activeTaskID = taskId;

				//mServiceReply = null;
				//engineInstance.registerEventCallback(null);
				//applicationContext.stopSelf();

			}
			else if (msg.what == UnrealMessageType.StopService.ordinal())
			{
				Bundle data = msg.getData();
				int taskId = data.getInt("taskId", 0);

				Log.i(TAG, "handleMessage(MSG_STOP_SERVICE) - proc=" + Name(applicationContext)
					+ ", taskId=" + taskId
					+ ", activeTaskID=" + activeTaskID
					+ ", applicationContext=" + applicationContext);

				engineInstance.QueuePause("UnrealMessageType.StopService, taskId=" + taskId);
				engineInstance.QueueStop("UnrealMessageType.StopService, taskId=" + taskId);
				//engineInstance.onPause(engineInstance.getCurrentContextID(), "UnrealMessageType.StopService, taskId=" + taskId);
				//engineInstance.onStop(engineInstance.getCurrentContextID(), "UnrealMessageType.StopService, taskId=" + taskId);

				activeTaskID = taskId;

				mServiceReply = null;
				engineInstance.registerEventCallback(null);
				applicationContext.stopSelf();
			}
			else if (msg.what == UnrealMessageType.ResumeService.ordinal())
			{
				Bundle data = msg.getData();
				int taskId = data.getInt("taskId", 0);
				boolean bResume = data.getBoolean("resume", true);
				boolean bCMDMainInit = data.getBoolean("cmd_mainInit", false);

				Log.i(TAG, "handleMessage(MSG_RESUME_SERVICE) - proc=" + Name(applicationContext)
					+ ", taskId=" + taskId
					+ ", activeTaskID=" + activeTaskID
					+ ", applicationContext=" + applicationContext);

				engineInstance.registerEventCallback(this);

				if (bResume)
				{
					engineInstance.QueueResume("UnrealMessageType.ResumeService, taskId=" + taskId);
				}
				else {

					if (bCMDMainInit)
						GameActivity.Get().nativeResumeMainInit();
				}

			}
			else if (msg.what == UnrealMessageType.TouchEvent.ordinal())
			{
				engineInstance.onTouchEvent(msg.getData().getParcelable("touch"));
			}
			else if (msg.what == UnrealMessageType.SendConsoleCommand.ordinal())
			{
				Bundle data = msg.getData();
				String consoleCommand = data.getString("consoleCommand");

				Log.i(TAG, "handleMessage(MSG_ISSUE_CONSOLE_COMMAND) - proc=" + Name(applicationContext)
						+ ", consoleCommand=" + consoleCommand
						+ ", activeTaskID=" + activeTaskID
						+ ", applicationContext=" + applicationContext);

				engineInstance.sendConsoleCommand(consoleCommand);
			}
			else
			{
				super.handleMessage(msg);
			}
		}

		@Override
		protected void finalize() throws Throwable {
			super.finalize();
			mServiceReply = null;
			engineInstance.registerEventCallback(null);
		}
	}


	
	@Override
    public void onCreate() {
        super.onCreate();
		Log.i(TAG, "onCreate(UnrealSharedInstanceService) called. context = " + this 
			+ ", activeTaskID=" + activeTaskID 
		);
	}


    // execution of service will start
    // on calling this method
	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {

		Log.i(TAG, "UnrealSharedInstanceService::onStartCommand called. context = " + this + ", startID=" + startId );

		// returns the status
		// of the program
		return START_STICKY;
    }
	

    // execution of the service will
    // stop on calling this method
	//
    @Override
    public void onDestroy() {
        super.onDestroy();

		Log.w(TAG, "onDestroy(UnrealAndroidInstanceService) - proc = " + Name(this)
			+ ", mMessenger = " + mMessenger
		);

		if (mMessenger != null) {
			try {
				mMessenger.send(Message.obtain(null, UnrealMessageType.DetachExternalSurface.ordinal()));
			} catch (RemoteException e) {
				e.printStackTrace();
			}
		}
		mMessenger = null;
    }

    @Override
    public IBinder onBind(Intent intent) {
		int taskId = intent.getIntExtra("taskId", 0);

		Log.i(TAG, "onBind(intent) - proc=" + Name(this)
			+ ", taskId=" + taskId
			+ ", activeTaskID=" + activeTaskID
			+ ", intent=" + intent);


		Toast.makeText(getApplicationContext(), "binding taskId=" + taskId, Toast.LENGTH_SHORT).show();
		mMessenger = new Messenger(new IncomingHandler(this, intent));
		return mMessenger.getBinder();
    }
	@Override
	public boolean onUnbind(Intent intent) {
		int taskId = intent.getIntExtra("taskId", 0);
		Log.i(TAG, "onUnbind(intent) - proc=" + Name(this)
			+ ", taskId=" + taskId
			+ ", activeTaskID=" + activeTaskID
			+ ", intent=" + intent);
		return super.onUnbind(intent);
	}
}
