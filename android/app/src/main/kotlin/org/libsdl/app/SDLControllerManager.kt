package org.libsdl.app

import android.content.Context
import android.graphics.Color
import android.hardware.lights.Light
import android.hardware.lights.LightsManager
import android.hardware.lights.LightsRequest
import android.hardware.lights.LightState
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import java.util.Comparator
import kotlin.math.roundToInt

class SDLControllerManager {
    companion object {
        @JvmStatic external fun nativeSetupJNI()

        @JvmStatic external fun nativeAddJoystick(
            device_id: Int,
            name: String,
            desc: String,
            vendor_id: Int,
            product_id: Int,
            button_mask: Int,
            naxes: Int,
            axis_mask: Int,
            nhats: Int,
            can_rumble: Boolean,
            has_rgb_led: Boolean,
        )

        @JvmStatic external fun nativeRemoveJoystick(device_id: Int)
        @JvmStatic external fun nativeAddHaptic(device_id: Int, name: String)
        @JvmStatic external fun nativeRemoveHaptic(device_id: Int)
        @JvmStatic external fun onNativePadDown(device_id: Int, keycode: Int): Boolean
        @JvmStatic external fun onNativePadUp(device_id: Int, keycode: Int): Boolean
        @JvmStatic external fun onNativeJoy(device_id: Int, axis: Int, value: Float)
        @JvmStatic external fun onNativeHat(device_id: Int, hat_id: Int, x: Int, y: Int)

        @JvmField var mJoystickHandler: SDLJoystickHandler? = null
        @JvmField var mHapticHandler: SDLHapticHandler? = null

        private const val TAG = "SDLControllerManager"

        @JvmStatic fun initialize() {
            if (mJoystickHandler == null) {
                mJoystickHandler = SDLJoystickHandler()
            }

            if (mHapticHandler == null) {
                mHapticHandler = if (Build.VERSION.SDK_INT >= 31) {
                    SDLHapticHandler_API31()
                } else if (Build.VERSION.SDK_INT >= 26) {
                    SDLHapticHandler_API26()
                } else {
                    SDLHapticHandler()
                }
            }
        }

        // Joystick glue code, just a series of stubs that redirect to the SDLJoystickHandler instance
        @JvmStatic fun handleJoystickMotionEvent(event: MotionEvent): Boolean =
            requireNotNull(mJoystickHandler).handleMotionEvent(event)

        /**
         * This method is called by SDL using JNI.
         */
        @JvmStatic fun pollInputDevices() {
            requireNotNull(mJoystickHandler).pollInputDevices()
        }

        /**
         * This method is called by SDL using JNI.
         */
        @JvmStatic fun joystickSetLED(device_id: Int, red: Int, green: Int, blue: Int) {
            requireNotNull(mJoystickHandler).setLED(device_id, red, green, blue)
        }

        /**
         * This method is called by SDL using JNI.
         */
        @JvmStatic fun pollHapticDevices() {
            requireNotNull(mHapticHandler).pollHapticDevices()
        }

        /**
         * This method is called by SDL using JNI.
         */
        @JvmStatic fun hapticRun(device_id: Int, intensity: Float, length: Int) {
            requireNotNull(mHapticHandler).run(device_id, intensity, length)
        }

        /**
         * This method is called by SDL using JNI.
         */
        @JvmStatic fun hapticRumble(
            device_id: Int,
            low_frequency_intensity: Float,
            high_frequency_intensity: Float,
            length: Int,
        ) {
            requireNotNull(mHapticHandler).rumble(device_id, low_frequency_intensity, high_frequency_intensity, length)
        }

        /**
         * This method is called by SDL using JNI.
         */
        @JvmStatic fun hapticStop(device_id: Int) {
            requireNotNull(mHapticHandler).stop(device_id)
        }

        // Check if a given device is considered a possible SDL joystick
        @JvmStatic fun isDeviceSDLJoystick(deviceId: Int): Boolean {
            val device = InputDevice.getDevice(deviceId)
            // We cannot use InputDevice.isVirtual before API 16, so let's accept
            // only nonnegative device ids (VIRTUAL_KEYBOARD equals -1)
            if (device == null || deviceId < 0) {
                return false
            }
            val sources = device.sources

            /* This is called for every button press, so let's not spam the logs */
            /*
            if ((sources & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
                Log.v(TAG, "Input device " + device.getName() + " has class joystick.");
            }
            if ((sources & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD) {
                Log.v(TAG, "Input device " + device.getName() + " is a dpad.");
            }
            if ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
                Log.v(TAG, "Input device " + device.getName() + " is a gamepad.");
            }
            */

            return (sources and InputDevice.SOURCE_CLASS_JOYSTICK) != 0 ||
                (sources and InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD ||
                (sources and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
        }
    }
}

/* Actual joystick functionality available for API >= 19 devices */
open class SDLJoystickHandler {
    class SDLJoystick {
        var device_id = 0
        lateinit var name: String
        lateinit var desc: String
        lateinit var axes: ArrayList<InputDevice.MotionRange>
        lateinit var hats: ArrayList<InputDevice.MotionRange>
        lateinit var lights: ArrayList<Light>
        var lightsSession: LightsManager.LightsSession? = null
    }

    class RangeComparator : Comparator<InputDevice.MotionRange> {
        override fun compare(arg0: InputDevice.MotionRange, arg1: InputDevice.MotionRange): Int {
            // Some controllers, like the Moga Pro 2, return AXIS_GAS (22) for right trigger and AXIS_BRAKE (23) for left trigger - swap them so they're sorted in the right order for SDL
            var arg0Axis = arg0.axis
            var arg1Axis = arg1.axis
            if (arg0Axis == MotionEvent.AXIS_GAS) {
                arg0Axis = MotionEvent.AXIS_BRAKE
            } else if (arg0Axis == MotionEvent.AXIS_BRAKE) {
                arg0Axis = MotionEvent.AXIS_GAS
            }
            if (arg1Axis == MotionEvent.AXIS_GAS) {
                arg1Axis = MotionEvent.AXIS_BRAKE
            } else if (arg1Axis == MotionEvent.AXIS_BRAKE) {
                arg1Axis = MotionEvent.AXIS_GAS
            }

            // Make sure the AXIS_Z is sorted between AXIS_RY and AXIS_RZ.
            // This is because the usual pairing are:
            // - AXIS_X + AXIS_Y (left stick).
            // - AXIS_RX, AXIS_RY (sometimes the right stick, sometimes triggers).
            // - AXIS_Z, AXIS_RZ (sometimes the right stick, sometimes triggers).
            // This sorts the axes in the above order, which tends to be correct
            // for Xbox-ish game pads that have the right stick on RX/RY and the
            // triggers on Z/RZ.
            //
            // Gamepads that don't have AXIS_Z/AXIS_RZ but use
            // AXIS_LTRIGGER/AXIS_RTRIGGER are unaffected by this.
            //
            // References:
            // - https://developer.android.com/develop/ui/views/touch-and-input/game-controllers/controller-input
            // - https://www.kernel.org/doc/html/latest/input/gamepad.html
            if (arg0Axis == MotionEvent.AXIS_Z) {
                arg0Axis = MotionEvent.AXIS_RZ - 1
            } else if (arg0Axis > MotionEvent.AXIS_Z && arg0Axis < MotionEvent.AXIS_RZ) {
                --arg0Axis
            }
            if (arg1Axis == MotionEvent.AXIS_Z) {
                arg1Axis = MotionEvent.AXIS_RZ - 1
            } else if (arg1Axis > MotionEvent.AXIS_Z && arg1Axis < MotionEvent.AXIS_RZ) {
                --arg1Axis
            }

            return arg0Axis - arg1Axis
        }
    }

    private val mJoysticks = ArrayList<SDLJoystick>()

    /**
     * Handles adding and removing of input devices.
     */
    @Synchronized fun pollInputDevices() {
        val deviceIds = InputDevice.getDeviceIds()

        for (device_id in deviceIds) {
            if (SDLControllerManager.isDeviceSDLJoystick(device_id)) {
                var joystick = getJoystick(device_id)
                if (joystick == null) {
                    val joystickDevice = InputDevice.getDevice(device_id) ?: continue
                    joystick = SDLJoystick()
                    joystick.device_id = device_id
                    joystick.name = joystickDevice.name
                    joystick.desc = getJoystickDescriptor(joystickDevice)
                    joystick.axes = ArrayList()
                    joystick.hats = ArrayList()
                    joystick.lights = ArrayList()

                    val ranges = ArrayList(joystickDevice.motionRanges)
                    ranges.sortWith(RangeComparator())
                    for (range in ranges) {
                        if ((range.source and InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
                            if (range.axis == MotionEvent.AXIS_HAT_X || range.axis == MotionEvent.AXIS_HAT_Y) {
                                joystick.hats.add(range)
                            } else {
                                joystick.axes.add(range)
                            }
                        }
                    }

                    var can_rumble = false
                    var has_rgb_led = false
                    if (Build.VERSION.SDK_INT >= 31) {
                        val vibratorManager = joystickDevice.vibratorManager
                        val vibrators = vibratorManager.vibratorIds
                        if (vibrators.isNotEmpty()) {
                            can_rumble = true
                        }
                        val lightsManager = joystickDevice.lightsManager
                        val lights = lightsManager.lights
                        for (light in lights) {
                            if (light.hasRgbControl()) {
                                joystick.lights.add(light)
                            }
                        }
                        if (joystick.lights.isNotEmpty()) {
                            joystick.lightsSession = lightsManager.openSession()
                            has_rgb_led = true
                        }
                    }

                    mJoysticks.add(joystick)
                    SDLControllerManager.nativeAddJoystick(
                        joystick.device_id,
                        joystick.name,
                        joystick.desc,
                        getVendorId(joystickDevice),
                        getProductId(joystickDevice),
                        getButtonMask(joystickDevice),
                        joystick.axes.size,
                        getAxisMask(joystick.axes),
                        joystick.hats.size / 2,
                        can_rumble,
                        has_rgb_led,
                    )
                }
            }
        }

        /* Check removed devices */
        var removedDevices: ArrayList<Int>? = null
        for (joystick in mJoysticks) {
            val device_id = joystick.device_id
            var i = 0
            while (i < deviceIds.size) {
                if (device_id == deviceIds[i]) {
                    break
                }
                i++
            }
            if (i == deviceIds.size) {
                if (removedDevices == null) {
                    removedDevices = ArrayList()
                }
                removedDevices.add(device_id)
            }
        }

        if (removedDevices != null) {
            for (device_id in removedDevices) {
                SDLControllerManager.nativeRemoveJoystick(device_id)
                var i = 0
                while (i < mJoysticks.size) {
                    if (mJoysticks[i].device_id == device_id) {
                        if (Build.VERSION.SDK_INT >= 31) {
                            if (mJoysticks[i].lightsSession != null) {
                                try {
                                    mJoysticks[i].lightsSession?.close()
                                } catch (_: Exception) {
                                    // Session may already be unregistered when device disconnects
                                }
                                mJoysticks[i].lightsSession = null
                            }
                        }
                        mJoysticks.removeAt(i)
                        break
                    }
                    i++
                }
            }
        }
    }

    @Synchronized protected fun getJoystick(device_id: Int): SDLJoystick? {
        for (joystick in mJoysticks) {
            if (joystick.device_id == device_id) {
                return joystick
            }
        }
        return null
    }

    /**
     * Handles given MotionEvent.
     * @param event the event to be handled.
     * @return if given event was processed.
     */
    fun handleMotionEvent(event: MotionEvent): Boolean {
        val actionPointerIndex = event.actionIndex
        val action = event.actionMasked
        if (action == MotionEvent.ACTION_MOVE) {
            val joystick = getJoystick(event.deviceId)
            if (joystick != null) {
                for (i in 0 until joystick.axes.size) {
                    val range = joystick.axes[i]
                    /* Normalize the value to -1...1 */
                    val value = (event.getAxisValue(range.axis, actionPointerIndex) - range.min) / range.range * 2.0f - 1.0f
                    SDLControllerManager.onNativeJoy(joystick.device_id, i, value)
                }
                for (i in 0 until joystick.hats.size / 2) {
                    val hatX = event.getAxisValue(joystick.hats[2 * i].axis, actionPointerIndex).roundToInt()
                    val hatY = event.getAxisValue(joystick.hats[2 * i + 1].axis, actionPointerIndex).roundToInt()
                    SDLControllerManager.onNativeHat(joystick.device_id, i, hatX, hatY)
                }
            }
        }
        return true
    }

    fun getJoystickDescriptor(joystickDevice: InputDevice): String {
        val desc = joystickDevice.descriptor
        if (desc != null && desc.isNotEmpty()) {
            return desc
        }
        return joystickDevice.name
    }

    fun getProductId(joystickDevice: InputDevice): Int = joystickDevice.productId

    fun getVendorId(joystickDevice: InputDevice): Int = joystickDevice.vendorId

    fun getAxisMask(ranges: List<InputDevice.MotionRange>): Int {
        // For compatibility, keep computing the axis mask like before,
        // only really distinguishing 2, 4 and 6 axes.
        var axis_mask = 0
        if (ranges.size >= 2) {
            // ((1 << SDL_GAMEPAD_AXIS_LEFTX) | (1 << SDL_GAMEPAD_AXIS_LEFTY))
            axis_mask = axis_mask or 0x0003
        }
        if (ranges.size >= 4) {
            // ((1 << SDL_GAMEPAD_AXIS_RIGHTX) | (1 << SDL_GAMEPAD_AXIS_RIGHTY))
            axis_mask = axis_mask or 0x000c
        }
        if (ranges.size >= 6) {
            // ((1 << SDL_GAMEPAD_AXIS_LEFT_TRIGGER) | (1 << SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))
            axis_mask = axis_mask or 0x0030
        }
        // Also add an indicator bit for whether the sorting order has changed.
        // This serves to disable outdated gamecontrollerdb.txt mappings.
        var have_z = false
        var have_past_z_before_rz = false
        for (range in ranges) {
            val axis = range.axis
            if (axis == MotionEvent.AXIS_Z) {
                have_z = true
            } else if (axis > MotionEvent.AXIS_Z && axis < MotionEvent.AXIS_RZ) {
                have_past_z_before_rz = true
            }
        }
        if (have_z && have_past_z_before_rz) {
            // If both these exist, the compare() function changed sorting order.
            // Set a bit to indicate this fact.
            axis_mask = axis_mask or 0x8000
        }
        return axis_mask
    }

    fun getButtonMask(joystickDevice: InputDevice): Int {
        var button_mask = 0
        val keys = intArrayOf(
            KeyEvent.KEYCODE_BUTTON_A,
            KeyEvent.KEYCODE_BUTTON_B,
            KeyEvent.KEYCODE_BUTTON_X,
            KeyEvent.KEYCODE_BUTTON_Y,
            KeyEvent.KEYCODE_BACK,
            KeyEvent.KEYCODE_MENU,
            KeyEvent.KEYCODE_BUTTON_MODE,
            KeyEvent.KEYCODE_BUTTON_START,
            KeyEvent.KEYCODE_BUTTON_THUMBL,
            KeyEvent.KEYCODE_BUTTON_THUMBR,
            KeyEvent.KEYCODE_BUTTON_L1,
            KeyEvent.KEYCODE_BUTTON_R1,
            KeyEvent.KEYCODE_DPAD_UP,
            KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_DPAD_LEFT,
            KeyEvent.KEYCODE_DPAD_RIGHT,
            KeyEvent.KEYCODE_BUTTON_SELECT,
            KeyEvent.KEYCODE_DPAD_CENTER,

            // These don't map into any SDL controller buttons directly
            KeyEvent.KEYCODE_BUTTON_L2,
            KeyEvent.KEYCODE_BUTTON_R2,
            KeyEvent.KEYCODE_BUTTON_C,
            KeyEvent.KEYCODE_BUTTON_Z,
            KeyEvent.KEYCODE_BUTTON_1,
            KeyEvent.KEYCODE_BUTTON_2,
            KeyEvent.KEYCODE_BUTTON_3,
            KeyEvent.KEYCODE_BUTTON_4,
            KeyEvent.KEYCODE_BUTTON_5,
            KeyEvent.KEYCODE_BUTTON_6,
            KeyEvent.KEYCODE_BUTTON_7,
            KeyEvent.KEYCODE_BUTTON_8,
            KeyEvent.KEYCODE_BUTTON_9,
            KeyEvent.KEYCODE_BUTTON_10,
            KeyEvent.KEYCODE_BUTTON_11,
            KeyEvent.KEYCODE_BUTTON_12,
            KeyEvent.KEYCODE_BUTTON_13,
            KeyEvent.KEYCODE_BUTTON_14,
            KeyEvent.KEYCODE_BUTTON_15,
            KeyEvent.KEYCODE_BUTTON_16,
        )
        val masks = intArrayOf(
            (1 shl 0), // A -> A
            (1 shl 1), // B -> B
            (1 shl 2), // X -> X
            (1 shl 3), // Y -> Y
            (1 shl 4), // BACK -> BACK
            (1 shl 6), // MENU -> START
            (1 shl 5), // MODE -> GUIDE
            (1 shl 6), // START -> START
            (1 shl 7), // THUMBL -> LEFTSTICK
            (1 shl 8), // THUMBR -> RIGHTSTICK
            (1 shl 9), // L1 -> LEFTSHOULDER
            (1 shl 10), // R1 -> RIGHTSHOULDER
            (1 shl 11), // DPAD_UP -> DPAD_UP
            (1 shl 12), // DPAD_DOWN -> DPAD_DOWN
            (1 shl 13), // DPAD_LEFT -> DPAD_LEFT
            (1 shl 14), // DPAD_RIGHT -> DPAD_RIGHT
            (1 shl 4), // SELECT -> BACK
            (1 shl 0), // DPAD_CENTER -> A
            (1 shl 15), // L2 -> ??
            (1 shl 16), // R2 -> ??
            (1 shl 17), // C -> ??
            (1 shl 18), // Z -> ??
            (1 shl 20), // 1 -> ??
            (1 shl 21), // 2 -> ??
            (1 shl 22), // 3 -> ??
            (1 shl 23), // 4 -> ??
            (1 shl 24), // 5 -> ??
            (1 shl 25), // 6 -> ??
            (1 shl 26), // 7 -> ??
            (1 shl 27), // 8 -> ??
            (1 shl 28), // 9 -> ??
            (1 shl 29), // 10 -> ??
            (1 shl 30), // 11 -> ??
            (1 shl 31), // 12 -> ??
            // We're out of room...
            -1, // 13 -> ??
            -1, // 14 -> ??
            -1, // 15 -> ??
            -1, // 16 -> ??
        )
        val has_keys = joystickDevice.hasKeys(*keys)
        for (i in keys.indices) {
            if (has_keys[i]) {
                button_mask = button_mask or masks[i]
            }
        }
        return button_mask
    }

    fun setLED(device_id: Int, red: Int, green: Int, blue: Int) {
        if (Build.VERSION.SDK_INT < 31) {
            return
        }
        val joystick = getJoystick(device_id)
        if (joystick == null || joystick.lights.isEmpty()) {
            return
        }
        val lightsRequest = LightsRequest.Builder()
        val lightState = LightState.Builder().setColor(Color.rgb(red, green, blue)).build()
        for (light in joystick.lights) {
            if (light.hasRgbControl()) {
                lightsRequest.addLight(light, lightState)
            }
        }
        joystick.lightsSession?.requestLights(lightsRequest.build())
    }
}

open class SDLHapticHandler_API31 : SDLHapticHandler() {
    override fun run(device_id: Int, intensity: Float, length: Int) {
        val haptic = getHaptic(device_id)
        if (haptic != null) {
            vibrate(haptic.vib, intensity, length)
        }
    }

    override fun rumble(device_id: Int, low_frequency_intensity: Float, high_frequency_intensity: Float, length: Int) {
        val device = InputDevice.getDevice(device_id) ?: return

        if (Build.VERSION.SDK_INT < 31) {
            /* Silence 'lint' warning */
            return
        }

        val manager = device.vibratorManager
        val vibrators = manager.vibratorIds
        if (vibrators.size >= 2) {
            vibrate(manager.getVibrator(vibrators[0]), low_frequency_intensity, length)
            vibrate(manager.getVibrator(vibrators[1]), high_frequency_intensity, length)
        } else if (vibrators.size == 1) {
            val intensity = low_frequency_intensity * 0.6f + high_frequency_intensity * 0.4f
            vibrate(manager.getVibrator(vibrators[0]), intensity, length)
        }
    }

    private fun vibrate(vibrator: Vibrator, intensity: Float, length: Int) {
        if (Build.VERSION.SDK_INT < 31) {
            /* Silence 'lint' warning */
            return
        }

        if (intensity == 0.0f) {
            vibrator.cancel()
            return
        }

        var value = (intensity * 255).roundToInt()
        if (value > 255) {
            value = 255
        }
        if (value < 1) {
            vibrator.cancel()
            return
        }
        try {
            vibrator.vibrate(VibrationEffect.createOneShot(length.toLong(), value))
        } catch (_: Exception) {
            // Fall back to the generic method, which uses DEFAULT_AMPLITUDE, but works even if
            // something went horribly wrong with the Android 8.0 APIs.
            @Suppress("DEPRECATION")
            vibrator.vibrate(length.toLong())
        }
    }
}

open class SDLHapticHandler_API26 : SDLHapticHandler() {
    override fun run(device_id: Int, intensity: Float, length: Int) {
        if (Build.VERSION.SDK_INT < 26) {
            /* Silence 'lint' warning */
            return
        }

        val haptic = getHaptic(device_id)
        if (haptic != null) {
            if (intensity == 0.0f) {
                stop(device_id)
                return
            }

            var vibeValue = (intensity * 255).roundToInt()

            if (vibeValue > 255) {
                vibeValue = 255
            }
            if (vibeValue < 1) {
                stop(device_id)
                return
            }
            try {
                haptic.vib.vibrate(VibrationEffect.createOneShot(length.toLong(), vibeValue))
            } catch (_: Exception) {
                // Fall back to the generic method, which uses DEFAULT_AMPLITUDE, but works even if
                // something went horribly wrong with the Android 8.0 APIs.
                @Suppress("DEPRECATION")
                haptic.vib.vibrate(length.toLong())
            }
        }
    }
}

open class SDLHapticHandler {
    class SDLHaptic {
        var device_id = 0
        lateinit var name: String
        lateinit var vib: Vibrator
    }

    private val mHaptics = ArrayList<SDLHaptic>()

    open fun run(device_id: Int, intensity: Float, length: Int) {
        val haptic = getHaptic(device_id)
        if (haptic != null) {
            @Suppress("DEPRECATION")
            haptic.vib.vibrate(length.toLong())
        }
    }

    open fun rumble(device_id: Int, low_frequency_intensity: Float, high_frequency_intensity: Float, length: Int) {
        // Not supported in older APIs
    }

    fun stop(device_id: Int) {
        val haptic = getHaptic(device_id)
        if (haptic != null) {
            haptic.vib.cancel()
        }
    }

    @Synchronized fun pollHapticDevices() {
        val deviceId_VIBRATOR_SERVICE = 999999
        var hasVibratorService = false

        /* Check VIBRATOR_SERVICE */
        @Suppress("DEPRECATION")
        val vib = requireNotNull(SDL.getContext()).getSystemService(Context.VIBRATOR_SERVICE) as Vibrator?
        if (vib != null) {
            hasVibratorService = vib.hasVibrator()

            if (hasVibratorService) {
                var haptic = getHaptic(deviceId_VIBRATOR_SERVICE)
                if (haptic == null) {
                    haptic = SDLHaptic()
                    haptic.device_id = deviceId_VIBRATOR_SERVICE
                    haptic.name = "VIBRATOR_SERVICE"
                    haptic.vib = vib
                    mHaptics.add(haptic)
                    SDLControllerManager.nativeAddHaptic(haptic.device_id, haptic.name)
                }
            }
        }

        /* Check removed devices */
        var removedDevices: ArrayList<Int>? = null
        for (haptic in mHaptics) {
            val device_id = haptic.device_id
            if (device_id != deviceId_VIBRATOR_SERVICE || !hasVibratorService) {
                if (removedDevices == null) {
                    removedDevices = ArrayList()
                }
                removedDevices.add(device_id)
            } // else: don't remove the vibrator if it is still present
        }

        if (removedDevices != null) {
            for (device_id in removedDevices) {
                SDLControllerManager.nativeRemoveHaptic(device_id)
                var i = 0
                while (i < mHaptics.size) {
                    if (mHaptics[i].device_id == device_id) {
                        mHaptics.removeAt(i)
                        break
                    }
                    i++
                }
            }
        }
    }

    @Synchronized protected fun getHaptic(device_id: Int): SDLHaptic? {
        for (haptic in mHaptics) {
            if (haptic.device_id == device_id) {
                return haptic
            }
        }
        return null
    }
}

open class SDLGenericMotionListener_API14 : View.OnGenericMotionListener {
    // Generic Motion (mouse hover, joystick...) events go here
    override fun onGenericMotion(v: View, event: MotionEvent): Boolean {
        if (event.source == InputDevice.SOURCE_JOYSTICK) {
            return SDLControllerManager.handleJoystickMotionEvent(event)
        }

        var x: Float
        var y: Float
        val action = event.actionMasked
        val pointerCount = event.pointerCount
        var consumed = false

        for (i in 0 until pointerCount) {
            val toolType = event.getToolType(i)

            if (toolType == MotionEvent.TOOL_TYPE_MOUSE) {
                when (action) {
                    MotionEvent.ACTION_SCROLL -> {
                        x = event.getAxisValue(MotionEvent.AXIS_HSCROLL, i)
                        y = event.getAxisValue(MotionEvent.AXIS_VSCROLL, i)
                        SDLActivity.onNativeMouse(0, action, x, y, false)
                        consumed = true
                    }

                    MotionEvent.ACTION_HOVER_MOVE -> {
                        x = getEventX(event, i)
                        y = getEventY(event, i)

                        SDLActivity.onNativeMouse(0, action, x, y, checkRelativeEvent(event))
                        consumed = true
                    }

                    else -> {
                    }
                }
            } else if (toolType == MotionEvent.TOOL_TYPE_STYLUS || toolType == MotionEvent.TOOL_TYPE_ERASER) {
                when (action) {
                    MotionEvent.ACTION_HOVER_ENTER,
                    MotionEvent.ACTION_HOVER_MOVE,
                    MotionEvent.ACTION_HOVER_EXIT,
                    -> {
                        x = event.getX(i)
                        y = event.getY(i)
                        var p = event.getPressure(i)
                        if (p > 1.0f) {
                            // may be larger than 1.0f on some devices
                            // see the documentation of getPressure(i)
                            p = 1.0f
                        }

                        // BUTTON_STYLUS_PRIMARY is 2^5, so shift by 4, and apply SDL_PEN_INPUT_DOWN/SDL_PEN_INPUT_ERASER_TIP
                        var buttons = (event.buttonState shr 4) or (1 shl if (toolType == MotionEvent.TOOL_TYPE_STYLUS) 0 else 30)
                        if ((event.buttonState and MotionEvent.BUTTON_TERTIARY) != 0) {
                            buttons = buttons or 0x08
                        }

                        SDLActivity.onNativePen(event.getPointerId(i), getPenDeviceType(event.device), buttons, action, x, y, p)
                        consumed = true
                    }
                }
            }
        }

        return consumed
    }

    open fun supportsRelativeMouse(): Boolean = false

    open fun inRelativeMode(): Boolean = false

    open fun setRelativeMouseEnabled(enabled: Boolean): Boolean = false

    open fun reclaimRelativeMouseModeIfNeeded() {
    }

    open fun checkRelativeEvent(event: MotionEvent): Boolean = inRelativeMode()

    open fun getEventX(event: MotionEvent, pointerIndex: Int): Float = event.getX(pointerIndex)

    open fun getEventY(event: MotionEvent, pointerIndex: Int): Float = event.getY(pointerIndex)

    open fun getPenDeviceType(penDevice: InputDevice?): Int = SDL_PEN_DEVICE_TYPE_UNKNOWN

    companion object {
        protected const val SDL_PEN_DEVICE_TYPE_UNKNOWN = 0
        protected const val SDL_PEN_DEVICE_TYPE_DIRECT = 1
        protected const val SDL_PEN_DEVICE_TYPE_INDIRECT = 2
    }
}

open class SDLGenericMotionListener_API24 : SDLGenericMotionListener_API14() {
    // Generic Motion (mouse hover, joystick...) events go here

    private var mRelativeModeEnabled = false

    override fun supportsRelativeMouse(): Boolean = true

    override fun inRelativeMode(): Boolean = mRelativeModeEnabled

    override fun setRelativeMouseEnabled(enabled: Boolean): Boolean {
        mRelativeModeEnabled = enabled
        return true
    }

    override fun getEventX(event: MotionEvent, pointerIndex: Int): Float {
        if (Build.VERSION.SDK_INT < 24) {
            /* Silence 'lint' warning */
            return 0f
        }

        return if (mRelativeModeEnabled && event.getToolType(pointerIndex) == MotionEvent.TOOL_TYPE_MOUSE) {
            event.getAxisValue(MotionEvent.AXIS_RELATIVE_X, pointerIndex)
        } else {
            event.getX(pointerIndex)
        }
    }

    override fun getEventY(event: MotionEvent, pointerIndex: Int): Float {
        if (Build.VERSION.SDK_INT < 24) {
            /* Silence 'lint' warning */
            return 0f
        }

        return if (mRelativeModeEnabled && event.getToolType(pointerIndex) == MotionEvent.TOOL_TYPE_MOUSE) {
            event.getAxisValue(MotionEvent.AXIS_RELATIVE_Y, pointerIndex)
        } else {
            event.getY(pointerIndex)
        }
    }
}

open class SDLGenericMotionListener_API26 : SDLGenericMotionListener_API24() {
    // Generic Motion (mouse hover, joystick...) events go here
    private var mRelativeModeEnabled = false

    override fun supportsRelativeMouse(): Boolean = !SDLActivity.isDeXMode() || Build.VERSION.SDK_INT >= 27

    override fun inRelativeMode(): Boolean = mRelativeModeEnabled

    override fun setRelativeMouseEnabled(enabled: Boolean): Boolean {
        if (Build.VERSION.SDK_INT < 26) {
            /* Silence 'lint' warning */
            return false
        }

        return if (!SDLActivity.isDeXMode() || Build.VERSION.SDK_INT >= 27) {
            if (enabled) {
                SDLActivity.getContentView().requestPointerCapture()
            } else {
                SDLActivity.getContentView().releasePointerCapture()
            }
            mRelativeModeEnabled = enabled
            true
        } else {
            false
        }
    }

    override fun reclaimRelativeMouseModeIfNeeded() {
        if (Build.VERSION.SDK_INT < 26) {
            /* Silence 'lint' warning */
            return
        }

        if (mRelativeModeEnabled && !SDLActivity.isDeXMode()) {
            SDLActivity.getContentView().requestPointerCapture()
        }
    }

    override fun checkRelativeEvent(event: MotionEvent): Boolean {
        if (Build.VERSION.SDK_INT < 26) {
            /* Silence 'lint' warning */
            return false
        }
        return event.source == InputDevice.SOURCE_MOUSE_RELATIVE
    }

    override fun getEventX(event: MotionEvent, pointerIndex: Int): Float {
        // Relative mouse in capture mode will only have relative for X/Y
        return event.getX(pointerIndex)
    }

    override fun getEventY(event: MotionEvent, pointerIndex: Int): Float {
        // Relative mouse in capture mode will only have relative for X/Y
        return event.getY(pointerIndex)
    }
}

class SDLGenericMotionListener_API29 : SDLGenericMotionListener_API26() {
    override fun getPenDeviceType(penDevice: InputDevice?): Int {
        if (penDevice == null) {
            return SDL_PEN_DEVICE_TYPE_UNKNOWN
        }

        return if (penDevice.isExternal) SDL_PEN_DEVICE_TYPE_INDIRECT else SDL_PEN_DEVICE_TYPE_DIRECT
    }
}
