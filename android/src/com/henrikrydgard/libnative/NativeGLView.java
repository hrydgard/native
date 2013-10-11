package com.henrikrydgard.libnative;

// Touch- and sensor-enabled GLSurfaceView.
// Supports simple multitouch and pressure.

import android.app.Activity;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.opengl.GLSurfaceView;
import android.os.Handler;
// import android.os.Build;
// import android.util.Log;
import android.util.Log;
import android.view.MotionEvent;
import com.bda.controller.*;

public class NativeGLView extends GLSurfaceView implements SensorEventListener, ControllerListener {
	private static String TAG = "NativeGLView";
	private SensorManager mSensorManager;
	private Sensor mAccelerometer;
	
	// Moga controller
	private Controller mController = null;
	boolean isMogaPro = false;
	
	public NativeGLView(NativeActivity activity) {
		super(activity);

		/*
		if (Build.VERSION.SDK_INT >= 11) {
			try {
				Method method_setPreserveEGLContextOnPause = GLSurfaceView.class.getMethod(
						"setPreserveEGLContextOnPause", new Class[] { Boolean.class });
				Log.i(TAG, "Invoking setPreserveEGLContextOnPause");
				method_setPreserveEGLContextOnPause.invoke(this, true);
			} catch (NoSuchMethodException e) {
				e.printStackTrace();
			} catch (IllegalArgumentException e) {
				e.printStackTrace();
			} catch (IllegalAccessException e) {
				e.printStackTrace();
			} catch (InvocationTargetException e) {
				e.printStackTrace();
			}
		}*/
		
		mSensorManager = (SensorManager)activity.getSystemService(Activity.SENSOR_SERVICE);
		mAccelerometer = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
		
		Log.i(TAG, "Initializing MOGA");
		mController = Controller.getInstance(activity);
		if (mController.init()) {
			Log.i(TAG, "MOGA initialized");
			mController.setListener(this, new Handler());
			if (mController.getState(Controller.STATE_CURRENT_PRODUCT_VERSION) == Controller.ACTION_VERSION_MOGAPRO) {
				Log.i(TAG, "MOGA pro detected");
				isMogaPro = true;
			}
		} else {
			Log.i(TAG, "MOGA failed to initialize");
		}
	}

	public boolean onTouchEvent(final MotionEvent ev) {
		for (int i = 0; i < ev.getPointerCount(); i++) {
			int pid = ev.getPointerId(i);
			int code = 0;
			
			final int action = ev.getActionMasked();
			
			switch (action) {
			case MotionEvent.ACTION_DOWN:
			case MotionEvent.ACTION_POINTER_DOWN:
				if (ev.getActionIndex() == i)
					code = 1;
				break;
			case MotionEvent.ACTION_UP:
			case MotionEvent.ACTION_POINTER_UP:
				if (ev.getActionIndex() == i)
					code = 2;
				break;
			case MotionEvent.ACTION_MOVE:
				code = 3;
				break;
			default:
				break;
			}
			if (code != 0) {
				float x = ev.getX(i);
				float y = ev.getY(i);
				NativeApp.touch(x, y, code, pid);
			}
		}
		return true;
	} 

	// Sensor management
	public void onAccuracyChanged(Sensor sensor, int arg1) {
	}

	public void onSensorChanged(SensorEvent event) {
		if (event.sensor.getType() != Sensor.TYPE_ACCELEROMETER) {
			return;
		}
		// Can also look at event.timestamp for accuracy magic
		NativeApp.accelerometer(event.values[0], event.values[1], event.values[2]);
	}
	
	@Override
	public void onPause() {
		super.onPause();
		mSensorManager.unregisterListener(this);
		if (mController != null) {
			mController.onPause();
		}
	}
	 
	@Override
	public void onResume() {
		super.onResume();
		mSensorManager.registerListener(this, mAccelerometer, SensorManager.SENSOR_DELAY_GAME);
		if (mController != null) {
			mController.onResume();
			
			// According to the docs, the Moga's state can be inconsistent here.
			// We should do a one time poll. TODO
		}
	}
	
	public void onDestroy() {
		if (mController != null) {
			mController.exit();	
		}
	}
	
	// MOGA Controller - from ControllerListener
	@Override
	public void onKeyEvent(KeyEvent event) {
		// The Moga left stick doubles as a D-pad. This creates mapping conflicts so let's turn it off.
		// Unfortunately this breaks menu navigation in PPSSPP currently but meh.
		// This is different on Moga Pro though.
		
		if (!isMogaPro) {
			switch (event.getKeyCode()) {
			case KeyEvent.KEYCODE_DPAD_DOWN:
			case KeyEvent.KEYCODE_DPAD_UP:
			case KeyEvent.KEYCODE_DPAD_LEFT:
			case KeyEvent.KEYCODE_DPAD_RIGHT:
				return;
			default:
				break;
			}
		}

		switch (event.getAction()) {
		case KeyEvent.ACTION_DOWN:
			NativeApp.keyDown(NativeApp.DEVICE_ID_PAD_0, event.getKeyCode());
			break;
		case KeyEvent.ACTION_UP:
			NativeApp.keyUp(NativeApp.DEVICE_ID_PAD_0, event.getKeyCode());
			break;
		}
	}

	// MOGA Controller - from ControllerListener
	@Override
	public void onMotionEvent(com.bda.controller.MotionEvent event) {
		NativeApp.joystickAxis(NativeApp.DEVICE_ID_PAD_0, com.bda.controller.MotionEvent.AXIS_X, event.getAxisValue(com.bda.controller.MotionEvent.AXIS_X));
		NativeApp.joystickAxis(NativeApp.DEVICE_ID_PAD_0, com.bda.controller.MotionEvent.AXIS_Y, event.getAxisValue(com.bda.controller.MotionEvent.AXIS_Y));
		NativeApp.joystickAxis(NativeApp.DEVICE_ID_PAD_0, com.bda.controller.MotionEvent.AXIS_Z, event.getAxisValue(com.bda.controller.MotionEvent.AXIS_Z));
		NativeApp.joystickAxis(NativeApp.DEVICE_ID_PAD_0, com.bda.controller.MotionEvent.AXIS_RZ, event.getAxisValue(com.bda.controller.MotionEvent.AXIS_RZ));
		NativeApp.joystickAxis(NativeApp.DEVICE_ID_PAD_0, com.bda.controller.MotionEvent.AXIS_LTRIGGER, event.getAxisValue(com.bda.controller.MotionEvent.AXIS_LTRIGGER));
		NativeApp.joystickAxis(NativeApp.DEVICE_ID_PAD_0, com.bda.controller.MotionEvent.AXIS_RTRIGGER, event.getAxisValue(com.bda.controller.MotionEvent.AXIS_RTRIGGER));
	}

	// MOGA Controller - from ControllerListener
	@Override
	public void onStateEvent(StateEvent state) {
		switch (state.getState()) {
		case StateEvent.STATE_CONNECTION:
			switch (state.getAction()) {
			case StateEvent.ACTION_CONNECTED:
				Log.i(TAG, "Moga Connected");
				break;
			case StateEvent.ACTION_CONNECTING:
				Log.i(TAG, "Moga Connecting...");
				break;
			case StateEvent.ACTION_DISCONNECTED:
				Log.i(TAG, "Moga Disconnected (or simply Not connected)");
				break;
			}
			break;
			
		case StateEvent.STATE_POWER_LOW:
			switch (state.getAction()) {
			case StateEvent.ACTION_TRUE:
				Log.i(TAG, "Moga Power Low");
				break;
			case StateEvent.ACTION_FALSE:
				Log.i(TAG, "Moga Power OK");
				break;
			}
			break;

		default:
			break;
		}
		
	}
}
