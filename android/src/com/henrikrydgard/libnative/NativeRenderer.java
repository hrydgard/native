package com.henrikrydgard.libnative;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.WindowManager;


public class NativeRenderer implements GLSurfaceView.Renderer {
	private static String TAG = "NativeRenderer";
	public static int brightness;
	
	static NativeActivity mActivity;
	
	NativeRenderer(NativeActivity act) {
		mActivity = act;
	}
	
	
	public void onDrawFrame(GL10 unused /*use GLES20*/) {
        displayRender();
	}

	public void onSurfaceCreated(GL10 unused, EGLConfig config) {
		Log.i(TAG, "onSurfaceCreated - EGL context is new or was lost");
		// Actually, it seems that it is here we should recreate lost GL objects.
		displayInit();
	}
 
	public void onSurfaceChanged(GL10 unused, int width, int height) {
		Log.i(TAG, "onSurfaceChanged");
		displayResize(width, height);
	}
	
	public static void getCurrentBrightness() {
		WindowManager.LayoutParams settings = mActivity.getWindow().getAttributes();
		brightness = Math.round(settings.screenBrightness*100);
	}
	
	public static void setCurrentBrightness(int value) {
		WindowManager.LayoutParams settings = mActivity.getWindow().getAttributes();
        settings.screenBrightness = (float)value/100;
        mActivity.getWindow().setAttributes(settings);
	}
	
	// NATIVE METHODS

	// Note: This also means "device lost" and you should reload
	// all buffered objects. 
	public native void displayInit(); 
	public static native int displayGetBrightness();
	public static native void displaySetBrightness(int value);
	public native void displayResize(int w, int h);
	public native void displayRender();
	
	// called by the C++ code through JNI. Dispatch anything we can't directly handle
	// on the gfx thread to the UI thread.
	public void postCommand(String command, String parameter) {
		final String cmd = command;
		final String param = parameter;
		mActivity.runOnUiThread(new Runnable() {
			public void run() {
				NativeRenderer.this.mActivity.processCommand(cmd, param);
			}
		});
	}
}
