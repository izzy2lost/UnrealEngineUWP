package com.epicgames.ASISStub;

import androidx.annotation.NonNull;
import androidx.annotation.UiThread;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.lifecycle.MutableLiveData;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.ServiceConnection;
import android.content.res.Configuration;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.content.Intent;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.RelativeLayout;

import com.epicgames.makeaar.UnrealMessageType;

import java.lang.ref.WeakReference;
import java.util.concurrent.atomic.AtomicReference;


public class UseServiceActivity extends Activity implements View.OnClickListener
{

	Messenger mMessenger;
//	private static WeakReference<View.OnClickListener> mCallbackRef = new WeakReference<>(IEventCallback.NO_OP);



	//final MutableLiveData<MyServiceConnection> mServiceConnection = new MutableLiveData<MyServiceConnection>(new MyServiceConnection());

	ASISConnection mServiceConnection = null; //new MyServiceConnection();
	
	ASISConnection GetServiceConnection(String caller)
	{

		Log.w(TAG, "GetServiceConnection(" + caller + ") : mServiceConnection = " + mServiceConnection);

		if (mServiceConnection != null)
		{
			if (!mServiceConnection.isBoundToService.getValue())
			{
				mServiceConnection.bindToUnrealInstanceService("GetServiceConnection rebind");
			}
			return mServiceConnection;
		}
		else
		{
			CreateServiceConnection(this, "GetServiceConnection");
		}
		
		assert (mServiceConnection != null);

		return mServiceConnection;
	}

	ASISConnection CreateServiceConnection(Activity activity, String caller)
	{

		Log.w(TAG, "CreateServiceConnection(" + caller + ") : mServiceConnection = " + mServiceConnection + ", activity=" + activity);

		if (mServiceConnection != null)
		{
			if (!mServiceConnection.isBoundToService.getValue())
			{
				mServiceConnection.bindToUnrealInstanceService("CreateServiceConnection rebind");
			}
			return mServiceConnection;
		}
		
//		if (mServiceConnection != null && mServiceConnection.isBoundToService.getValue())
//		{
//			return mServiceConnection;
////			mServiceConnection.logContextDetails("CreateServiceConnection destroy prev (" + activity + ")");
////			DeposeServiceConnection();
//		}

		if (mServiceConnection == null)
		{
			//mServiceConnection.logContextDetails("CreateServiceConnection create new (" + activity + ")");
			mServiceConnection = new ASISConnection(activity);
			mServiceConnection.bindToUnrealInstanceService("CreateServiceConnection");
		}

		return mServiceConnection;
	}

	void DeposeServiceConnection(String caller)
	{

		Log.w(TAG, "DeposeServiceConnection, mServiceConnection = " + mServiceConnection + ", caller=" + caller);

		if (mServiceConnection != null && mServiceConnection.isBoundToService.getValue()) {
			mServiceConnection.unbindToUnrealInstanceService(caller + "->DeposeServiceConnection");
			mServiceConnection.doCleanupForUnbinding(caller + "->DeposeServiceConnection");
		}

		mServiceConnection = null;
	}



	static Integer next_theme = AppCompatDelegate.MODE_NIGHT_NO;

	// declaring objects of Button class
	// Surface renderSurface;

	final static String TAG = "UE-UseServiceActivity";


	
	private Button activateView1Button, activateView2Button, activateView3Button, restartActivityButton, themeChangeButton, resumeButton;
	private Button pauseButton, stopButton, bindServiceButton, unbindServiceButton, memReportButton;

	void ActivateTextureView(int viewIndex)
	{
		TextureView textureView_ = null;
		switch (viewIndex)
		{
			case 1:
				textureView_ = (TextureView) findViewById( R.id.textureView );
				break;
			case 2:
				textureView_ = (TextureView) findViewById( R.id.renderView );
				break;
			case 3:
				textureView_ = (TextureView) findViewById( R.id.renderView2 );
				break;
			default:
				assert false;
		}

//		try {
//			GetServiceConnection("ActivateTextureView cleanup").detachSurfaceFromService(getTaskId(), null);
//		} catch (Exception e) {
//			e.printStackTrace();
//		}

		GetServiceConnection("ActivateTextureView").SetTextureView(textureView_);
	}


	@UiThread
	void stopService(int attachId, Handler.Callback callback)
	{
		Intent intent = new Intent();
		intent.setClassName(ASISConnection.servicePackageName, "com.epicgames.makeaar.UnrealSharedInstanceService");
		getApplication().stopService(intent);
	}

	@Override
	public void onContentChanged() {
		Log.v(TAG, "UseServiceActivity: onContentChanged");
		super.onContentChanged();
	}

	@Override
	protected void onResume() {
		Log.v(TAG, "UseServiceActivity: onResume");
		super.onResume();

		//GetServiceConnection("onResume").resumeService(getTaskId());
	}

	@Override
	protected void onPause() {
		Log.v(TAG, "UseServiceActivity: onPause");
		super.onPause();
		GetServiceConnection("onPause").detachSurfaceFromService(getTaskId(), null);
	}

	@Override
	protected void onStart() {
		Log.v(TAG, "UseServiceActivity: onStart");
		super.onStart();

//		CreateServiceConnection( this,"UseServiceActivity::onStart()");
	}

	@Override
	protected void onStop() {
		Log.v(TAG, "UseServiceActivity: onStop");
		super.onStop();
//		DeposeServiceConnection();
	}

	@Override
	protected void onDestroy() {
		Log.v(TAG, "UseServiceActivity: onDestroy");
		super.onDestroy();

		DeposeServiceConnection("onDestroy");
	}

	@Override
	protected void onRestart() {
		Log.v(TAG, "UseServiceActivity onRestart called! hasBeenCreated = " + hasBeenCreated);
		super.onRestart();
	}

	
	@Override
	protected void onCreate(Bundle savedInstanceState) {

		new RelativeLayout(this);
		
//		getWindow().getTransitionManager().setTransition();
//		WindowManager.LayoutParams params = getWindow().getAttributes();
//		params.type = params.TYPE_APPLICATION_ATTACHED_DIALOG;
//		params.flags = params.flags | Window.FEATURE_NO_TITLE;

//!		getWindow();
//!		getWindowManager();
//!		getIntent();
//!		runOnUiThread();
		
//		getResources();
//		getExternalFilesDir();
//		getFilesDir();
//		getSystemService();
//		getPackageName();
//		getPackageResourcePath();
//		getMainExecutor();
//		getDisplay();
//		getPackageManager();
//		getApplicationContext();


		
		super.onCreate( savedInstanceState );
		AppCompatDelegate.setDefaultNightMode(next_theme);
		getTheme().applyStyle(R.style.Theme_UnrealEngine_NoActionBar, true);
		
		setContentView( R.layout.interface_use_service_activity );



		activateView1Button 	= (Button) findViewById( R.id.activateView1Button);
		activateView2Button 	= (Button) findViewById( R.id.activateView2Button);
		activateView3Button 	= (Button) findViewById( R.id.activateView3Button);

		pauseButton 			= (Button) findViewById( R.id.buttonPause);
		stopButton 				= (Button) findViewById( R.id.buttonStopService);
		bindServiceButton 		= (Button) findViewById( R.id.buttonBindService);
		unbindServiceButton 	= (Button) findViewById( R.id.buttonUnbindService);
		restartActivityButton 	= (Button) findViewById( R.id.restartActivityButton);
		memReportButton 		= (Button) findViewById( R.id.buttonMemReport);
		themeChangeButton 		= (Button) findViewById( R.id.changeThemeButton);
		resumeButton 			= (Button) findViewById( R.id.resumeButton);


		activateView1Button.setOnClickListener( this );
		activateView2Button.setOnClickListener( this );
		activateView3Button.setOnClickListener( this );

		pauseButton.setOnClickListener( this );
		stopButton.setOnClickListener( this );
		bindServiceButton.setOnClickListener( this );
		unbindServiceButton.setOnClickListener( this );
		themeChangeButton.setOnClickListener( this );
		resumeButton.setOnClickListener( this );


		Log.v(TAG, "UseServiceActivity onCreate called! hasBeenCreated = " + hasBeenCreated);

		if (false == hasBeenCreated) {
			Debug.getMemoryInfo(baseMemoryInfo);
			Debug.getMemoryInfo(prevMemoryInfo);
			hasBeenCreated = true;
		}

		CreateServiceConnection( this,"UseServiceActivity::onCreate()");
	}

	public void onClick(View view)
	{
		final boolean waitForBind = false;

		if (activateView1Button.equals(view)) {
			ActivateTextureView(1);
		} else if (activateView2Button.equals(view)) {
			ActivateTextureView(2);
		} else if (activateView3Button.equals(view)) {
			ActivateTextureView(3);
		} else if (pauseButton.equals(view)) {

			try {
				GetServiceConnection("Pause Button").detachSurfaceFromService(getTaskId(), null);
			} catch (Exception e) {
				e.printStackTrace();
			}

		} else if (stopButton.equals(view)) {
			stopService(getTaskId(), null);
		} else if (bindServiceButton.equals(view)) {

			try {
				CreateServiceConnection( this,"Bind Service Button").bindToUnrealInstanceService("onClick");
			} catch (Exception e) {
				e.printStackTrace();
			}

		} else if (unbindServiceButton.equals(view)) {

			try {
				DeposeServiceConnection("unbindServiceButton");
				//GetServiceConnection("Unbind Service Button").unbindToUnrealInstanceService("onClick");
			} catch (Exception e) {
				e.printStackTrace();
			}

		} else if (restartActivityButton.equals(view)) {

			processRestartActivity();
		} else if (memReportButton.equals(view)) {
			ReportDebugMemoryInfo();
		} else if (themeChangeButton.equals(view)) {
			handleThemeChange();
		} else if (resumeButton.equals(view)) {

			try {
				GetServiceConnection("Resume Button").resumeService(getTaskId());
			} catch (Exception e) {
				e.printStackTrace();
			}
		}

	}



//    @Override
//  public boolean onTouchEvent(MotionEvent e) {
//      return ueInterface.MessageServiceToHandleTouch(e);
//    }


	//private final UEInterfaceHelpers ueInterface = new UEInterfaceHelpers(this);

	int cycleThemeCounter = 0;
	static boolean hasBeenCreated = false;
	static Debug.MemoryInfo memoryInfo = new Debug.MemoryInfo();
	static Debug.MemoryInfo prevMemoryInfo = new Debug.MemoryInfo();
	static Debug.MemoryInfo baseMemoryInfo = new Debug.MemoryInfo();
	static int tryCount = 0;
	void ReportDebugMemoryInfo()
	{
//        if (prevMemoryInfo != null)
//        {
//            Log.i(TAG, "Debug.memoryInfo prev= " + prevMemoryInfo.getMemoryStats());
//
//    }
		Runtime.getRuntime().gc();

		Debug.getMemoryInfo(memoryInfo);
		Log.v(TAG, "Debug.memoryInfo:\n\tthis= " + memoryInfo.getMemoryStats()
				+ "\n\tprev= " + prevMemoryInfo.getMemoryStats()
				+ "\n\tbase= " + baseMemoryInfo.getMemoryStats()
		);
		Debug.getMemoryInfo(prevMemoryInfo);

	}

	Handler.Callback onDetachSurfaceHandler = new Handler.Callback() {
		@Override
		public boolean handleMessage(Message msg) {
			synchronized (mServiceConnection) {
				Log.v(TAG, "onDetachSurfaceHandler::handleMessage() msg=" + msg);

				recreate();
				return true;
			}
		}
	};
	
	private Handler handler = new Handler();

	private void processRestartActivity() {

		mServiceConnection.detachSurfaceFromService(getTaskId(), onDetachSurfaceHandler);
		//recreate();
		
//		Runnable RestartTrigger = new Runnable() {
//			@Override
//			public void run() {
//
//				mServiceConnection.detachSurfaceFromService(getTaskId(), onDetachSurfaceHandler);
//			}
//		};
//
//		handler.post(RestartTrigger);
	}

	void doThemeChange(Integer value)
	{
		Log.v(TAG, "doing a touch event theme change: new uiTheme = ${value}");
//        prev_uiMode = next_theme;
		next_theme = value;
//        AppCompatDelegate.setDefaultNightMode(value);
		if (mServiceConnection != null)
		{
			mServiceConnection.detachSurfaceFromService(getTaskId(), onDetachSurfaceHandler);
		}
		else {
			recreate();
		}
	}

	@Override
	public void onConfigurationChanged(@NonNull Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
	}

	void handleThemeChange()
	{
		//GetServiceConnection().bindToUnrealInstanceService("handleThemeChange");
		//resumeService(getTaskId(), null);

		doThemeChange(next_theme == AppCompatDelegate.MODE_NIGHT_NO ? AppCompatDelegate.MODE_NIGHT_YES : AppCompatDelegate.MODE_NIGHT_NO);


		tryCount = 110;
//            ueInterface.bindToUnrealInstanceService(this, textureView_, BuildConfig.OBB_FILE_NAME, BuildConfig.OBB_MODULE_NAME, "-nosound -nocrashreports -opengl -featureleveles31", true, mConnectionStateCallback);
//            ReportDebugMemoryInfo();


//            ueInterface.runOnMainThread(new Runnable()
		runOnUiThread(new Runnable()
		{
			@Override
			public void run() {
				Context context = getApplicationContext();
				synchronized (context) {

					try {
						while ( (++tryCount) < 15) {
							Log.v(TAG, "mem pressure test iter = " + tryCount);


							doThemeChange(next_theme == AppCompatDelegate.MODE_NIGHT_NO ? AppCompatDelegate.MODE_NIGHT_YES : AppCompatDelegate.MODE_NIGHT_NO);

						}
					} catch (Exception e) {
						e.printStackTrace();
					}

				}
			}
		});
	}


}
