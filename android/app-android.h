#pragma once

#include "input/keycodes.h"
#include <jni.h>

static JNIEnv *jniEnvUI;
static jclass jniClass;

jint Java_com_henrikrydgard_libnative_NativeRenderer_displayGetBrightness(JNIEnv * env, jclass cls);
void Java_com_henrikrydgard_libnative_NativeRenderer_displaySetBrightness(JNIEnv * env, jclass cls, jint value);

// Compatability we alias the keycodes
// since native's keycodes are based on
// android keycodes.
typedef enum _keycode_t AndroidKeyCodes;

enum AndroidJoystickAxis {
	// Field descriptor #15 I
	JOYSTICK_AXIS_X = 0,
	JOYSTICK_AXIS_Y = 1,
	JOYSTICK_AXIS_PRESSURE = 2,
	JOYSTICK_AXIS_SIZE = 3,
	JOYSTICK_AXIS_TOUCH_MAJOR = 4,
	JOYSTICK_AXIS_TOUCH_MINOR = 5,
	JOYSTICK_AXIS_TOOL_MAJOR = 6,
	JOYSTICK_AXIS_TOOL_MINOR = 7,
	JOYSTICK_AXIS_ORIENTATION = 8,
	JOYSTICK_AXIS_VSCROLL = 9,
	JOYSTICK_AXIS_HSCROLL = 10,
	JOYSTICK_AXIS_Z = 11,
	JOYSTICK_AXIS_RX = 12,
	JOYSTICK_AXIS_RY = 13,
	JOYSTICK_AXIS_RZ = 14,
	JOYSTICK_AXIS_HAT_X = 15,
	JOYSTICK_AXIS_HAT_Y = 16,
	JOYSTICK_AXIS_LTRIGGER = 17,
	JOYSTICK_AXIS_RTRIGGER = 18,
	JOYSTICK_AXIS_THROTTLE = 19,
	JOYSTICK_AXIS_RUDDER = 20,
	JOYSTICK_AXIS_WHEEL = 21,
	JOYSTICK_AXIS_GAS = 22,
	JOYSTICK_AXIS_BRAKE = 23,
	JOYSTICK_AXIS_DISTANCE = 24,
	JOYSTICK_AXIS_TILT = 25,
};
