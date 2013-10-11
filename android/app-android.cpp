// This is generic code that is included in all Android apps that use the
// Native framework by Henrik Rydg�rd (https://github.com/hrydgard/native).

// It calls a set of methods defined in NativeApp.h. These should be implemented
// by your game or app.

#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <stdint.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/basictypes.h"
#include "base/display.h"
#include "base/NativeApp.h"
#include "base/logging.h"
#include "base/timeutil.h"
#include "file/zip_read.h"
#include "input/input_state.h"
#include "audio/mixer.h"
#include "math/math_util.h"
#include "net/resolve.h"
#include "android/native_audio.h"
#include "gfx_es2/gl_state.h"

#include "app-android.h"

static JNIEnv *jniEnvUI;

std::string frameCommand;
std::string frameCommandParam;

std::string systemName;
std::string langRegion;

const bool extraLog = true;

static float left_joystick_x_async;
static float left_joystick_y_async;
static float right_joystick_x_async;
static float right_joystick_y_async;
static float hat_joystick_x_async;
static float hat_joystick_y_async;

int optimalFramesPerBuffer = 0;
int optimalSampleRate = 0;

// Android implementation of callbacks to the Java part of the app
void SystemToast(const char *text) {
	frameCommand = "toast";
	frameCommandParam = text;
}

// TODO: need a Hide or bool show;
void ShowAd(int x, int y, bool center_x) {
	ELOG("TODO! ShowAd!");
}

void ShowKeyboard() {
	frameCommand = "showKeyboard";
	frameCommandParam = "";
}

void Vibrate(int length_ms) {
	frameCommand = "vibrate";
	char temp[32];
	sprintf(temp, "%i", length_ms);
	frameCommandParam = temp;
}

void LaunchBrowser(const char *url) {
	frameCommand = "launchBrowser";
	frameCommandParam = url;
}

void LaunchMarket(const char *url) {
	frameCommand = "launchMarket";
	frameCommandParam = url;
}

void LaunchEmail(const char *email_address) {
	frameCommand = "launchEmail";
	frameCommandParam = email_address;
}

void System_InputBox(const char *title, const char *defaultValue) {
	frameCommand = "inputBox";
	frameCommandParam = title;
}

std::string System_GetProperty(SystemProperty prop) {
	switch (prop) {
	case SYSPROP_NAME:
		return systemName;
	case SYSPROP_LANGREGION:  // "en_US"
		return langRegion;
	default:
		return "";
	}
}

// Remember that all of these need initialization on init! The process
// may be reused when restarting the game. Globals are DANGEROUS.

float dp_xscale = 1;
float dp_yscale = 1;

InputState input_state;

static bool renderer_inited = false;
static bool first_lost = true;
static bool use_opensl_audio = false;
static std::string library_path;

std::string GetJavaString(JNIEnv *env, jstring jstr)
{
	const char *str = env->GetStringUTFChars(jstr, 0);
	std::string cpp_string = std::string(str);
	env->ReleaseStringUTFChars(jstr, str);
	return cpp_string;
}

extern "C" jboolean Java_com_henrikrydgard_libnative_NativeApp_isLandscape(JNIEnv *env, jclass)
{
	std::string app_name, app_nice_name;
	bool landscape;
	NativeGetAppInfo(&app_name, &app_nice_name, &landscape);
	return landscape;
}

// For the Back button to work right.
extern "C" jboolean Java_com_henrikrydgard_libnative_NativeApp_isAtTopLevel(JNIEnv *env, jclass) {
	bool isAtTop = NativeIsAtTopLevel();
	if (extraLog) {
		ILOG("isAtTopLevel %i", (int)isAtTop);
	}
	return isAtTop;
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_audioConfig
	(JNIEnv *env, jclass, jint optimalFPB, jint optimalSR) {
	optimalFramesPerBuffer = optimalFPB;
	optimalSampleRate = optimalSR;
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_init
	(JNIEnv *env, jclass, jint dpi, jstring jdevicetype, jstring jlangRegion, jstring japkpath,
	 jstring jdataDir, jstring jexternalDir, jstring jlibraryDir, jstring jinstallID, jboolean juseNativeAudio) {
	jniEnvUI = env;

	ILOG("NativeApp.init() -- begin");

	memset(&input_state, 0, sizeof(input_state));
	renderer_inited = false;
	first_lost = true;

	g_buttonTracker.Reset();

	left_joystick_x_async = 0;
	left_joystick_y_async = 0;
	right_joystick_x_async = 0;
	right_joystick_y_async = 0;
	hat_joystick_x_async = 0;
	hat_joystick_y_async = 0;

	std::string apkPath = GetJavaString(env, japkpath);
	ILOG("NativeApp::Init: APK path: %s", apkPath.c_str());
	VFSRegister("", new ZipAssetReader(apkPath.c_str(), "assets/"));

	systemName = GetJavaString(env, jdevicetype);
	langRegion = GetJavaString(env, jlangRegion);

	std::string externalDir = GetJavaString(env, jexternalDir);
	std::string user_data_path = GetJavaString(env, jdataDir) + "/";
	library_path = GetJavaString(env, jlibraryDir) + "/";
	std::string installID = GetJavaString(env, jinstallID);

	ILOG("NativeApp.init(): External storage path: %s", externalDir.c_str());

	std::string app_name;
	std::string app_nice_name;
	bool landscape;

	net::Init();

	g_dpi = dpi;
	g_dpi_scale = 240.0f / (float)g_dpi;

	NativeGetAppInfo(&app_name, &app_nice_name, &landscape);

	const char *argv[2] = {app_name.c_str(), 0};
	NativeInit(1, argv, user_data_path.c_str(), externalDir.c_str(), installID.c_str());

	use_opensl_audio = juseNativeAudio;
	ILOG("NativeApp.init() -- end");
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_audioInit(JNIEnv *, jclass) {
	ILOG("NativeApp.audioInit() -- begin");
	if (use_opensl_audio) {
		// TODO: PPSSPP doesn't support 48khz yet so let's not use that yet.
		ILOG("Using OpenSL audio! frames/buffer: %i   optimal sr: %i   actual sr: 44100", optimalFramesPerBuffer, optimalSampleRate);
		optimalSampleRate = 44100;
		AndroidAudio_Init(&NativeMix, library_path, optimalFramesPerBuffer, optimalSampleRate);
	}
	ILOG("NativeApp.audioInit() -- end");
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_audioShutdown(JNIEnv *, jclass) {
	ILOG("NativeApp.audioShutdown() -- begin");
	if (use_opensl_audio) {
		AndroidAudio_Shutdown();
	}
	ILOG("NativeApp.audioShutdown() -- end");
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_resume(JNIEnv *, jclass) {
	ILOG("NativeApp.resume() - resuming audio");
	if (use_opensl_audio) {
		AndroidAudio_Resume();
	}
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_pause(JNIEnv *, jclass) {
	ILOG("NativeApp.pause() - begin");
	if (use_opensl_audio) {
		AndroidAudio_Pause();
	}
	ILOG("NativeApp.pause() - end");
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_shutdown(JNIEnv *, jclass) {
	ILOG("NativeApp.shutdown() -- begin");
	NativeShutdown();
	ILOG("VFSShutdown.");
	VFSShutdown();
	net::Shutdown();
	ILOG("NativeApp.shutdown() -- end");
}

static jmethodID postCommand;

extern "C" void Java_com_henrikrydgard_libnative_NativeRenderer_displayInit(JNIEnv * env, jobject obj) {
	ILOG("NativeApp.displayInit()");
	if (!renderer_inited) {
		// We default to 240 dpi and all UI code is written to assume it. (DENSITY_HIGH, like Nexus S).
		// Note that we don't compute dp_xscale and dp_yscale until later! This is so that NativeGetAppInfo
		// can change the dp resolution if it feels like it.
		dp_xres = pixel_xres * g_dpi_scale;
		dp_yres = pixel_yres * g_dpi_scale;

		ILOG("Calling NativeInitGraphics(): dpi = %i, dp_xres = %i, dp_yres = %i", g_dpi, dp_xres, dp_yres);
		NativeInitGraphics();
		ILOG("NativeInitGraphics() completed");

		dp_xscale = (float)dp_xres / pixel_xres;
		dp_yscale = (float)dp_yres / pixel_yres;
		renderer_inited = true;
	} else {
		ILOG("Calling NativeDeviceLost();");
		NativeDeviceLost();
		ILOG("NativeDeviceLost completed.;");
	}
	ILOG("(Re)-fetching method ID to postCommand...");
	jclass cls = env->GetObjectClass(obj);
	postCommand = env->GetMethodID(cls, "postCommand", "(Ljava/lang/String;Ljava/lang/String;)V");
	ILOG("MethodID: %i", (int)postCommand);
}

extern "C" void Java_com_henrikrydgard_libnative_NativeRenderer_displayResize(JNIEnv *, jobject clazz, jint w, jint h) {
	ILOG("NativeApp.displayResize(%i, %i)", w, h);
	// TODO: Move some of the logic from displayInit here?
	pixel_xres = w;
	pixel_yres = h;
	dp_xres = pixel_xres * g_dpi_scale;
	dp_yres = pixel_yres * g_dpi_scale;
	dp_xscale = (float)dp_xres / pixel_xres;
	dp_yscale = (float)dp_yres / pixel_yres;
}

extern "C" void Java_com_henrikrydgard_libnative_NativeRenderer_displayRender(JNIEnv *env, jobject obj) {
	// Too spammy
	// ILOG("NativeApp.displayRender()");
	if (renderer_inited) {
		// TODO: Look into if these locks are a perf loss
		{
			lock_guard guard(input_state.lock);

			input_state.pad_lstick_x = left_joystick_x_async;
			input_state.pad_lstick_y = left_joystick_y_async;
			input_state.pad_rstick_x = right_joystick_x_async;
			input_state.pad_rstick_y = right_joystick_y_async;

			UpdateInputState(&input_state);
		}
		NativeUpdate(input_state);

		{
			lock_guard guard(input_state.lock);
			EndInputState(&input_state);
		}

		NativeRender();
		time_update();
	} else {
		ELOG("BAD: Ended up in nativeRender even though app has quit.%s", "");
		// Shouldn't really get here. Let's draw magenta.
		glstate.depthWrite.set(GL_TRUE);
		glstate.colorMask.set(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(1.0, 0.0, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}
	
	if (!frameCommand.empty()) {
		ILOG("frameCommand %s %s", frameCommand.c_str(), frameCommandParam.c_str());

		jstring cmd = env->NewStringUTF(frameCommand.c_str());
		jstring param = env->NewStringUTF(frameCommandParam.c_str());
		env->CallVoidMethod(obj, postCommand, cmd, param);
		
		frameCommand = "";
		frameCommandParam = "";
	}
}

extern "C" void Java_com_henrikrydgard_libnative_NativeRenderer_displayShutdown(JNIEnv *env, jobject obj) {
	if (renderer_inited) {
		ILOG("Calling NativeDeviceLost();");
		NativeDeviceLost();
		ILOG("NativeDeviceLost completed.;");
		NativeShutdownGraphics();
		renderer_inited = false;
		NativeMessageReceived("recreateviews", "");
	}
}


// This path is not used if OpenSL ES is available.
extern "C" jint Java_com_henrikrydgard_libnative_NativeApp_audioRender(JNIEnv*	env, jclass clazz, jshortArray array) {
	// Too spammy
	// ILOG("NativeApp.audioRender");

	// The audio thread can pretty safely enable Flush-to-Zero mode on the FPU.
	EnableFZ();

	int buf_size = env->GetArrayLength(array);
	if (buf_size) {
		short *data = env->GetShortArrayElements(array, 0);
		int samples = buf_size / 2;
		samples = NativeMix(data, samples);
		if (samples != 0) {
			env->ReleaseShortArrayElements(array, data, 0);
			return samples * 2;
		} else {
			env->ReleaseShortArrayElements(array, data, JNI_ABORT);
			return 0;
		}
	}
	return 0;
}

extern "C" void JNICALL Java_com_henrikrydgard_libnative_NativeApp_touch
	(JNIEnv *, jclass, float x, float y, int code, int pointerId) {
	// ELOG("Touch Enter %i", pointerId);

	float scaledX = (int)(x * dp_xscale);	// why the (int) cast?
	float scaledY = (int)(y * dp_yscale);

	TouchInput touch;
	touch.id = pointerId;
	touch.x = scaledX;
	touch.y = scaledY;
	if (code == 1) {
		input_state.pointer_down[pointerId] = true;
		touch.flags = TOUCH_DOWN;
	} else if (code == 2) {
		input_state.pointer_down[pointerId] = false;
		touch.flags = TOUCH_UP;
	} else {
		touch.flags = TOUCH_MOVE;
	}
	NativeTouch(touch);


	lock_guard guard(input_state.lock);

	if (pointerId >= MAX_POINTERS) {
		ELOG("Too many pointers: %i", pointerId);
		return;	// We ignore 8+ pointers entirely.
	}
	input_state.pointer_x[pointerId] = scaledX;
	input_state.pointer_y[pointerId] = scaledY;
	input_state.mouse_valid = true;

	// ELOG("Touch Exit %i", pointerId);
}


extern "C" void Java_com_henrikrydgard_libnative_NativeApp_keyDown(JNIEnv *, jclass, jint deviceId, jint key) {
	KeyInput keyInput;
	keyInput.deviceId = deviceId;
	keyInput.keyCode = key;
	keyInput.flags = KEY_DOWN;
	NativeKey(keyInput);
	g_buttonTracker.Process(keyInput);
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_keyUp(JNIEnv *, jclass, jint deviceId, jint key) {
	KeyInput keyInput;
	keyInput.deviceId = deviceId;
	keyInput.keyCode = key;
	keyInput.flags = KEY_UP;
	NativeKey(keyInput);
	g_buttonTracker.Process(keyInput);
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_beginJoystickEvent(
	JNIEnv *env, jclass) {
	// mutex lock?
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_joystickAxis(
		JNIEnv *env, jclass, jint deviceId, jint axisId, jfloat value) {
	switch (axisId) {
	case JOYSTICK_AXIS_X:
		left_joystick_x_async = value;
		break;
	case JOYSTICK_AXIS_Y:
		left_joystick_y_async = -value;
		break;
	case JOYSTICK_AXIS_Z:
		right_joystick_x_async = value;
		break;
	case JOYSTICK_AXIS_RZ:
		right_joystick_y_async = -value;
		break;
	case JOYSTICK_AXIS_HAT_X:
		hat_joystick_x_async = value;
		break;
	case JOYSTICK_AXIS_HAT_Y:
		hat_joystick_y_async = -value;
		break;
	}

	// Hat is just stupid. Force translate it to dpad events.  TODO
	
	AxisInput axis;
	axis.axisId = axisId;
	axis.deviceId = deviceId;
	axis.value = value;
	NativeAxis(axis);
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_endJoystickEvent(
	JNIEnv *env, jclass) {
	// mutex unlock?
}


extern "C" void Java_com_henrikrydgard_libnative_NativeApp_mouseWheelEvent(
	JNIEnv *env, jclass, jint stick, jfloat x, jfloat y) {
	// TODO
}

extern "C" void JNICALL Java_com_henrikrydgard_libnative_NativeApp_accelerometer(JNIEnv *, jclass, float x, float y, float z) {
	// Theoretically this needs locking but I doubt it matters. Worst case, the X
	// from one "sensor frame" will be used together with Y from the next.
	// Should look into quantization though, for compressed movement storage.
	input_state.accelerometer_valid = true;
	input_state.acc.x = x;
	input_state.acc.y = y;
	input_state.acc.z = z;

	AxisInput axis;
	axis.deviceId = DEVICE_ID_ACCELEROMETER;
	axis.flags = 0;
	
	axis.axisId = JOYSTICK_AXIS_ACCELEROMETER_X;
	axis.value = x;
	NativeAxis(axis);

	axis.axisId = JOYSTICK_AXIS_ACCELEROMETER_Y;
	axis.value = y;
	NativeAxis(axis);

	axis.axisId = JOYSTICK_AXIS_ACCELEROMETER_Z;
	axis.value = z;
	NativeAxis(axis);
}

extern "C" void Java_com_henrikrydgard_libnative_NativeApp_sendMessage(JNIEnv *env, jclass, jstring message, jstring param) {
	jboolean isCopy;
	std::string msg = GetJavaString(env, message);
	std::string prm = GetJavaString(env, param);
	ILOG("Message received: %s %s", msg.c_str(), prm.c_str());
	NativeMessageReceived(msg.c_str(), prm.c_str());
}
