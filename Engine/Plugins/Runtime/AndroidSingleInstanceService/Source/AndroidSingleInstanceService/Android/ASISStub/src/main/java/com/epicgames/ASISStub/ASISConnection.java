package com.epicgames.ASISStub;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.UiThread;
import androidx.lifecycle.MutableLiveData;

import com.epicgames.makeaar.UnrealMessageType;

import java.util.concurrent.atomic.AtomicReference;


public class ASISConnection implements ServiceConnection, TextureView.SurfaceTextureListener, TextureView.OnTouchListener
{
	final static String TAG = "UE-ASISConnection";

	final static String obbModuleName = "Lyra";//""ALPHA_POC";
	//	final static String obbModuleName = BuildConfig.ASISModuleName; //"Lyra";//""ALPHA_POC";
//	final static String servicePackageName = "";
//	final static String servicePackageName = BuildConfig.ASISPackageName;
	final static String servicePackageName = "com.epicgames.Lyra";
	final static String obbFileLocation = ""; //"/data/local/tmp/3d/AndroidPackagingTest.main.obb.png";
//	final static String obbFileLocation = BuildConfig.ASISOBBPath; //"/data/local/tmp/3d/AndroidPackagingTest.main.obb.png";

	final static String commandLineArgs = "-nosound";
	//	final static String commandLineArgs = "-nosound -launchandroidflags=0 -dpcvars='Android.UseGameThreadForNativeCommands=0'";
	final static boolean enablePropagateAlpha = true;
	
	Messenger serviceMessenger = null;
	Messenger serviceReplyMessenger = null;
	MutableLiveData<Boolean> isBoundToService = new MutableLiveData<Boolean>(false);


	AtomicReference<Surface> externalSurfaceData = new AtomicReference<Surface>(null);

	int[] surfacePosition = new int[2];

	TextureView activeTextureView = null;

	final int mTaskID;

	private static final Handler.Callback mConnectionStateCallback = (Message msg) -> {
		final String TAG = "UE-ServiceCallback";

		switch (msg.what) {

			case 0:	//EVENTTYPE_INIT == 0
				Log.v(TAG, "received mConnectionStateCallback for EVENTTYPE_INIT, obj = " + msg.obj);

				break;

			case 1:	//EVENTTYPE_POST_ENGINE_INIT == 1
				Log.v(TAG, "received mConnectionStateCallback for EVENTTYPE_POST_ENGINE_INIT, obj = " + msg.obj);
				break;

			default:
				//Log.e(TAG, "mConnectionStateCallback, Unknown message = " + msg.what);
		}
		return true;
	};
	

	int getTaskId() { return mTaskID; }

	ContextWrapper mContextWrapper;

	ASISConnection(Activity _activity)
	{
//			if (mServiceConnection.getValue() != null)
//			{
//				unbindToUnrealInstanceService("MyServiceConnection constructor!!!");
//				doCleanupForUnbinding("MyServiceConnection constructor!!!");
//
//			}
		assert (_activity != null);

		mContextWrapper = _activity;

		mTaskID = _activity.getTaskId();

//			bindToUnrealInstanceService("MyServiceConnection constructor " + mTaskID);
	}


//		Activity getActivity() {
//			assert (mContextWrapper instanceof Activity);
//			return (Activity) mContextWrapper;
//		}
//
//		Application getApplication()
//		{
//			Application application = null;
//			try {
//				application = getActivity().getApplication();
//			}
//			catch (Exception e)
//			{
//				e.printStackTrace();
//			}
//			finally {
//				return application;
//			}
//		}

	void SendMessage(Message msg) throws RemoteException {
		if (serviceMessenger != null) {
			serviceMessenger.send(msg);
		}
	}

	void SetTextureView(TextureView textureView_)
	{
		textureView_.setAlpha(1.0f);
		textureView_.setOpaque(false);
		textureView_.setOnTouchListener(this);
		textureView_.setSurfaceTextureListener(this);

		Log.d(TAG, "SetTextureView: " + textureView_ + ", isAvailable=" + textureView_.isAvailable() + ", isAttachedToWindow=" + textureView_.isAttachedToWindow());

		if (textureView_.isAvailable() && textureView_.isAttachedToWindow())
		{
			textureView_.getLocationOnScreen(surfacePosition);

			onSurfaceTextureAvailable(textureView_.getSurfaceTexture(), textureView_.getWidth(), textureView_.getHeight());
		}
		activeTextureView = textureView_;
	}

	public boolean onTouch(View v, MotionEvent event) {
		// TODO Auto-generated method stub

		Log.d(TAG, "Touch: " + event.getAction() + " X: " + event.getX() + " Y: " + event.getY());

		Message msg = Message.obtain(null, UnrealMessageType.TouchEvent.ordinal());
		msg.getData().putInt("taskId", getTaskId());
		msg.getData().putParcelable("touch", event);

		try {
			SendMessage(msg);
		} catch (RemoteException e) {
			e.printStackTrace();
		}

		return true;
	}

	public void ConsoleCommand(Context context, String consoleCommand)
	{
		Log.i(TAG, "ConsoleCommand() - consoleCommand = " + consoleCommand);

		// Create and send a message to the service, using a supported 'what' value
		Message msg = Message.obtain(null, UnrealMessageType.SendConsoleCommand.ordinal());
		msg.getData().putString("consoleCommand", consoleCommand);

		try {
			SendMessage(msg);
		} catch (RemoteException e) {
			e.printStackTrace();
		}
	}

	void SendData(int event, String param1, int param2, int param3, float param4)
	{
		// Create and send a message to the service, using a supported 'what' value
		Message msg = Message.obtain(null, UnrealMessageType.SendData.ordinal());
		msg.getData().putInt("taskId", getTaskId());
		msg.getData().putInt("event", event);
		msg.getData().putString("param1", param1);
		msg.getData().putInt("param2", param2);
		msg.getData().putInt("param3", param3);
		msg.getData().putFloat("param4", param4);

		try {
			SendMessage(msg);
		} catch (RemoteException e) {
			e.printStackTrace();
		}
	}

	void bindToUnrealInstanceService(String caller)
	{
		logContextDetails("bindToUnrealInstanceService("+ caller + ") -> ");
		if(serviceReplyMessenger == null)
		{
			serviceReplyMessenger = new Messenger(new Handler(mConnectionStateCallback));
		}

		Intent intent = new Intent();
		intent.setClassName(servicePackageName, "com.epicgames.makeaar.UnrealSharedInstanceService");

		intent.putExtra("obbModuleName", obbModuleName);
		intent.putExtra("obbFileLocation", obbFileLocation);
		intent.putExtra("commandLineArgs", commandLineArgs);
		intent.putExtra("enablePropagateAlpha", enablePropagateAlpha);

		if (serviceReplyMessenger != null) {
			intent.putExtra("callback", serviceReplyMessenger);
		}

		//ServiceConnection connection = GetServiceConnection("bindToUnrealInstanceService, caller=" + caller);

		Log.d(TAG, "bindToUnrealInstanceService intent=" + intent + ", mServiceConnection=" + this);
		mContextWrapper.bindService(intent, this, Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT | Context.BIND_DEBUG_UNBIND | Context.BIND_ADJUST_WITH_ACTIVITY);
//			getApplication().bindService(intent, this, Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT | Context.BIND_ADJUST_WITH_ACTIVITY);
	}

	void unbindToUnrealInstanceService(String caller)
	{
		logContextDetails("unbindToUnrealInstanceService("+ caller + ") -> ");

		if (isBoundToService.getValue()) {
			Log.v(TAG, "unbindToUnrealInstanceService");

			mContextWrapper.unbindService(this);
		}
	}


	private void logContextDetails(String caller)
	{
		Log.w(TAG, caller + ": isBoundToService=" +isBoundToService.getValue()
			+ ", taskID=" +getTaskId()
			+ ", serviceMessenger=" +serviceMessenger
			+ ", serviceReplyMessenger=" +serviceReplyMessenger
			+ ", externalSurfaceData=" +externalSurfaceData
		);
	}

	public void doCleanupForUnbinding(String caller) {
		logContextDetails("doCleanupForUnbinding(" + caller + ")");
		
		if (activeTextureView != null) {
			activeTextureView.setSurfaceTextureListener(null);
			activeTextureView = null;
		}

		if (externalSurfaceData.get() != null)
		{
			externalSurfaceData.get().release();
			externalSurfaceData.set(null);
		}

		serviceMessenger = null;
		serviceReplyMessenger = null;
		isBoundToService.setValue( false );
	}

	@Override
	public void onServiceConnected(ComponentName name, IBinder serviceBinder) {
		Log.v(TAG, "onServiceConnected name=" + name + ", serviceBinder=" + serviceBinder);
		serviceMessenger = new Messenger(serviceBinder);
		//serviceReplyMessenger = new Messenger(new Handler(mConnectionStateCallback));
		isBoundToService.setValue( true );
//			TextureView textureView_ = (TextureView) findViewById( R.id.renderView );
//
//			textureView_ = (TextureView) getActivity().findViewById( R.id.renderView );
//			SetTextureView(textureView_);
	}

	@Override
	public void onServiceDisconnected(ComponentName name) {

		Log.w(TAG, "onServiceDisconnected name=" + name);

		doCleanupForUnbinding("onServiceDisconnected name=" + name);
	}

	@Override
	public void onBindingDied(ComponentName name) {

		Log.w(TAG, "onBindingDied name=" + name);

		doCleanupForUnbinding("onBindingDied name=" + name);

		ServiceConnection.super.onBindingDied(name);
	}

	@Override
	public void onNullBinding(ComponentName name) {

		Log.w(TAG, "onNullBinding name=" + name);

		//doCleanupForBinding("onNullBinding");

		ServiceConnection.super.onNullBinding(name);
	}

	@Override
	public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {

		logContextDetails("onSurfaceTextureAvailable(" + surfaceTexture + ")");

		int[] surfacePosition = this.surfacePosition;
		int[] surfaceSize = new int[]{width, height};

		if (externalSurfaceData.get() != null) {
			externalSurfaceData.get().release();
		}

		externalSurfaceData.set(new Surface(surfaceTexture));

		attachSurfaceToService(getTaskId(), externalSurfaceData.get(), surfacePosition, surfaceSize);
	}

	@Override
	public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
		logContextDetails("onSurfaceTextureSizeChanged(" + surfaceTexture + ")");
	}

	// Invoked when the specified SurfaceTexture is about to be destroyed.
	// If returns true, no rendering should happen inside the surface texture after this method is invoked.
	// If returns false, the client needs to call SurfaceTexture.release().
	// Most applications should return true.
	@Override
	public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {

		synchronized (this) {
			logContextDetails("onSurfaceTextureDestroyed(" + surfaceTexture + ")");
			detachSurfaceFromService(getTaskId(), null);

			if (externalSurfaceData.get() != null) {
				externalSurfaceData.get().release();
				externalSurfaceData.set(null);
			}

			if (activeTextureView != null) {
				activeTextureView.setSurfaceTextureListener(null);
			}
			return true;
		}
	}

	@Override
	public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
	}

	@UiThread
	void attachSurfaceToService(int attachId, Surface externalSurface, int[] viewPosition, int[] viewSize)
	{
		synchronized (this) {
			if (externalSurface == null) {
				Log.w(TAG, "attachSurfaceToService, externalSurface is null");
				return;
			}

			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
				Log.v(TAG, "attachSurfaceToService() - " + "proc = " + Application.getProcessName() + ", externalSurface = " + externalSurface + ", attachId = " + attachId);
			}

			Message msg = Message.obtain(null, UnrealMessageType.AttachExternalSurface.ordinal());
			Bundle data = msg.getData();
			data.putInt("attachId", attachId);
			data.putBoolean("enablePropagateAlpha", true);
			data.putBoolean("start", true);
			data.putBoolean("resume", true);

			data.putParcelable("surface", externalSurface);

			if (viewPosition.length == 2) {
				data.putIntArray("viewPosition", viewPosition);
			}

			if (viewSize.length == 2) {
				data.putIntArray("viewSize", viewSize);

			}

			try {
				SendMessage(msg);
			} catch (RemoteException e) {
				e.printStackTrace();
			}
		}
	}

	@UiThread
	void detachSurfaceFromService(int attachId, Handler.Callback callback) 
	{

		synchronized (this) {
			Message msg = Message.obtain(
				null,
				UnrealMessageType.DetachExternalSurface.ordinal()
			);

			msg.getData().putInt("attachId", attachId);
			msg.getData().putBoolean("pause", true);
			msg.getData().putBoolean("stop", true);

			if (callback != null) {

				Handler handler = new Handler(callback);
				Messenger replyMessenger = new Messenger(handler);

				msg.replyTo = replyMessenger;
			}

			Log.i(TAG, "detachSurfaceFromService() - " + "proc = ${Application.getProcessName()}");

			try {
				SendMessage(msg);
			} catch (RemoteException e) {
				e.printStackTrace();
			}
		}
	}

	@UiThread
	void resumeService(int attachId)
	{
		synchronized (this) {
			if (isBoundToService.getValue()) {
				Message msg = Message.obtain(null, UnrealMessageType.ResumeService.ordinal());
				Bundle data = msg.getData();
				data.putInt("attachId", attachId);

				try {
					SendMessage(msg);
				} catch (RemoteException e) {
					e.printStackTrace();
				}
			}
		}
	}
}