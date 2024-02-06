package com.epicgames.unreal;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import java.lang.ref.PhantomReference;
import java.lang.ref.WeakReference;
import java.util.Arrays;

// valid calls for Context...
// getResources()
// getApplicationContext()
// getPackageManager()

public final class SimpleContextWrapper extends ContextWrapper
{
	final Handler mHandler;
	final private Thread mUiThread;
final public Context mAppActivityContext;

public static Logger Log = new Logger("UE", "SimpleContextWrapper");

public static Activity getActivityOfContext(Context context)
{
	if (context == null)
	{
		return null;
    }

	while (context instanceof ContextWrapper) {
	if (context instanceof Activity) {
        return (Activity)context;
    }
	context = ((ContextWrapper)context).getBaseContext();
}
return null;
	}

	public static Context getBaseOfContext(Context context)
{
	if (context == null)
	{
		return null;
    }
    while (context instanceof ContextWrapper) {

	if (!(((ContextWrapper)context).getBaseContext() instanceof ContextWrapper)) {
        return ((ContextWrapper)context).getBaseContext();
    }
	context = ((ContextWrapper)context).getBaseContext();
}

return null;
	}

	public static ContextWrapper getContextWrapperOfContext(Context context)
{
	if (context == null)
	{
		return null;
	}

	Context searchContext = context;
	while (searchContext instanceof ContextWrapper) {
	if (searchContext instanceof SimpleContextWrapper) {
		return (SimpleContextWrapper)searchContext;
    }
	searchContext = ((ContextWrapper)searchContext).getBaseContext();
}


searchContext = context;
while (searchContext instanceof ContextWrapper) {
	if (searchContext instanceof Activity) {
		return (ContextWrapper)searchContext;
    }
	searchContext = ((ContextWrapper)searchContext).getBaseContext();
}

while (context instanceof ContextWrapper) {
    if (context instanceof ContextWrapper) {
        return (ContextWrapper)context;
    }
	context = ((ContextWrapper)context).getBaseContext();
}

return null;
	}

	public static SimpleContextWrapper getSimpleActivityOfContext(Context context)
{
    if (context == null)
    {
        return null;
    }

	while (context instanceof ContextWrapper) {
    if (context instanceof SimpleContextWrapper) {
        return (SimpleContextWrapper)context;
	}
	context = ((ContextWrapper)context).getBaseContext();
}

return null;
	}


//	@UiThread
	public SimpleContextWrapper(SimpleContextWrapper base)
{
	super(base);
	this.mHandler = base.mHandler;
	this.mUiThread = base.mUiThread;
	this.mAppActivityContext = base.mAppActivityContext;
}

//	@UiThread
public SimpleContextWrapper(Activity base)
{
	super(base);
	SimpleContextWrapper _base = getSimpleActivityOfContext(base);
	Log.verbose("UE - SimpleContextWrapper(Activity base),  base = " + base + ", _base= " + _base + ", Thread.currentThread() = " + Thread.currentThread());

	if (_base != null)
	{
		this.mHandler = _base.mHandler;
		this.mUiThread = _base.mUiThread;
		this.mAppActivityContext = _base;
	}
	else
	{
		this.mHandler = new Handler();
		this.mUiThread = Thread.currentThread();
		this.mAppActivityContext = getActivityOfContext(base);
	}

}

//	@UiThread
public SimpleContextWrapper(Context base)
{
	super(base);

	SimpleContextWrapper _base = getSimpleActivityOfContext(base);
	Log.verbose("UE - SimpleContextWrapper(Context base),  base = " + base + ", _base= " + _base + ", Thread.currentThread() = " + Thread.currentThread());

	if (_base != null)
	{
		this.mHandler = _base.mHandler;
		this.mUiThread = _base.mUiThread;
		this.mAppActivityContext = _base.mAppActivityContext;
	}
	else
	{
		this.mHandler = new Handler();
		this.mUiThread = Thread.currentThread();
		this.mAppActivityContext = getActivityOfContext(base);
	}
}

public final void runOnUiThread(Runnable action)
{

	Activity _activity = mAppActivityContext instanceof Activity ? (Activity)mAppActivityContext : null;
if (_activity == null)
{
	_activity = getActivityOfContext(this);
	if (_activity == null)
    {
		_activity = getActivityOfContext(getBaseOfContext(this));
	}
}

if (_activity != null)
{
	_activity.runOnUiThread(action);
}
else
{
	Log.debug("UE - SimpleContextWrapper, trying to runOnUiThread mAppActivityContext = " + mAppActivityContext + ", mUiThread= " + mUiThread + ", Thread.currentThread() = " + Thread.currentThread());

	if (Thread.currentThread() != mUiThread)
	{
		mHandler.post(action);
	}
	else
	{
		Log.error("UE - SimpleContextWrapper, trying to runOnUiThread is being BYPASSED and action called on Main Thread, mAppActivityContext = " + mAppActivityContext + ", mUiThread= " + mUiThread + ", Thread.currentThread() = " + Thread.currentThread());

		//action.run();
		runOnMainThread(action);

	}
}
	}

	public Window getWindow()
{
	return mAppActivityContext != null ? ((Activity)mAppActivityContext).getWindow() : null;
}

public WindowManager getWindowManager()
{
	return mAppActivityContext != null ? ((Activity)mAppActivityContext).getWindowManager() : (WindowManager)getSystemService(Context.WINDOW_SERVICE);
}

public void addContentView(View view, ViewGroup.LayoutParams params)
{
	if (mAppActivityContext != null)
	{
		((Activity)mAppActivityContext).addContentView(view, params);
	}
}

public boolean equals(Context obj)
{
	return (this.hashCode() == obj.hashCode());
}

public final void runOnMainThread(Runnable action)
{
	if (mHandler == null)
	{
		getMainExecutor().execute(action);
	}
	else
	{
		mHandler.post(action);
	}
}

}
