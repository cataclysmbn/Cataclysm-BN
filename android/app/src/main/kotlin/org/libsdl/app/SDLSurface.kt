package org.libsdl.app

import android.content.Context
import android.content.pm.ActivityInfo
import android.graphics.Insets
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Build
import android.util.DisplayMetrics
import android.util.Log
import android.view.Display
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.PointerIcon
import android.view.ScaleGestureDetector
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowInsets
import android.view.WindowManager
import kotlin.math.max
import kotlin.math.min

/** SDLSurface. This is what SDL draws on. */
class SDLSurface(context: Context) : SurfaceView(context), SurfaceHolder.Callback,
    View.OnApplyWindowInsetsListener, View.OnKeyListener, View.OnTouchListener,
    SensorEventListener, ScaleGestureDetector.OnScaleGestureListener {

    protected var mSensorManager: SensorManager
    protected var mDisplay: Display
    protected var mWidth = 1.0f
    protected var mHeight = 1.0f
    protected var mIsSurfaceReady = false
    private val scaleGestureDetector: ScaleGestureDetector

    init {
        holder.addCallback(this)
        scaleGestureDetector = ScaleGestureDetector(context, this)
        isFocusable = true
        isFocusableInTouchMode = true
        requestFocus()
        setOnApplyWindowInsetsListener(this)
        setOnKeyListener(this)
        setOnTouchListener(this)
        @Suppress("DEPRECATION")
        mDisplay = (context.getSystemService(Context.WINDOW_SERVICE) as WindowManager).defaultDisplay
        mSensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
        setOnGenericMotionListener(SDLActivity.getMotionListener())
    }

    fun handlePause() {
        enableSensor(Sensor.TYPE_ACCELEROMETER, false)
    }

    fun handleResume() {
        isFocusable = true
        isFocusableInTouchMode = true
        requestFocus()
        setOnApplyWindowInsetsListener(this)
        setOnKeyListener(this)
        setOnTouchListener(this)
        enableSensor(Sensor.TYPE_ACCELEROMETER, true)
    }

    fun getNativeSurface(): Surface = holder.surface

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.v("SDL", "surfaceCreated()")
        SDLActivity.onNativeSurfaceCreated()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.v("SDL", "surfaceDestroyed()")
        SDLActivity.mNextNativeState = SDLActivity.NativeState.PAUSED
        SDLActivity.handleNativeState()
        mIsSurfaceReady = false
        SDLActivity.onNativeSurfaceDestroyed()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.v("SDL", "surfaceChanged()")
        val singleton = SDLActivity.mSingleton ?: return

        mWidth = width.toFloat()
        mHeight = height.toFloat()
        var deviceWidth = width
        var deviceHeight = height
        var density = 1.0f
        try {
            val realMetrics = DisplayMetrics()
            @Suppress("DEPRECATION")
            mDisplay.getRealMetrics(realMetrics)
            deviceWidth = realMetrics.widthPixels
            deviceHeight = realMetrics.heightPixels
            density = realMetrics.densityDpi.toFloat() / 160.0f
        } catch (_: Exception) {
        }

        val activityContext = SDLActivity.getContext()
        synchronized(activityContext) {
            (activityContext as java.lang.Object).notifyAll()
        }

        Log.v("SDL", "Window size: ${width}x$height")
        Log.v("SDL", "Device size: ${deviceWidth}x$deviceHeight")
        SDLActivity.nativeSetScreenResolution(width, height, deviceWidth, deviceHeight, density, mDisplay.refreshRate)
        SDLActivity.onNativeResize()

        var skip = false
        val requestedOrientation = singleton.requestedOrientation
        if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT ||
            requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
        ) {
            if (mWidth > mHeight) {
                skip = true
            }
        } else if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE ||
            requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        ) {
            if (mWidth < mHeight) {
                skip = true
            }
        }

        if (skip) {
            val minDimension = min(mWidth, mHeight).toDouble()
            val maxDimension = max(mWidth, mHeight).toDouble()
            if (maxDimension / minDimension < 1.20) {
                Log.v("SDL", "Don't skip on such aspect-ratio. Could be a square resolution.")
                skip = false
            }
        }

        if (skip && Build.VERSION.SDK_INT >= 24) {
            skip = false
        }

        if (skip) {
            Log.v("SDL", "Skip .. Surface is not ready.")
            mIsSurfaceReady = false
            return
        }

        SDLActivity.onNativeSurfaceChanged()
        mIsSurfaceReady = true
        SDLActivity.mNextNativeState = SDLActivity.NativeState.RESUMED
        SDLActivity.handleNativeState()
    }

    override fun onApplyWindowInsets(v: View, insets: WindowInsets): WindowInsets {
        if (Build.VERSION.SDK_INT >= 30) {
            val combined: Insets = insets.getInsets(
                WindowInsets.Type.systemBars() or
                    WindowInsets.Type.systemGestures() or
                    WindowInsets.Type.mandatorySystemGestures() or
                    WindowInsets.Type.tappableElement() or
                    WindowInsets.Type.displayCutout(),
            )
            SDLActivity.onNativeInsetsChanged(combined.left, combined.right, combined.top, combined.bottom)
        }
        return insets
    }

    override fun onKey(v: View, keyCode: Int, event: KeyEvent): Boolean =
        SDLActivity.handleKeyEvent(v, keyCode, event, null)

    private fun getNormalizedX(x: Float): Float = if (mWidth <= 1) 0.5f else x / (mWidth - 1)
    private fun getNormalizedY(y: Float): Float = if (mHeight <= 1) 0.5f else y / (mHeight - 1)

    override fun onTouch(v: View, event: MotionEvent): Boolean {
        val touchDevId = event.deviceId
        val pointerCount = event.pointerCount
        val action = event.actionMasked
        var i = if (action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_POINTER_DOWN) event.actionIndex else 0
        do {
            val toolType = event.getToolType(i)
            if (toolType == MotionEvent.TOOL_TYPE_MOUSE) {
                val motionListener = SDLActivity.getMotionListener()
                val x = motionListener.getEventX(event, i)
                val y = motionListener.getEventY(event, i)
                SDLActivity.onNativeMouse(event.buttonState, action, x, y, motionListener.inRelativeMode())
            } else if (toolType == MotionEvent.TOOL_TYPE_STYLUS || toolType == MotionEvent.TOOL_TYPE_ERASER) {
                val pointerId = event.getPointerId(i)
                val x = event.getX(i)
                val y = event.getY(i)
                val pressure = min(event.getPressure(i), 1.0f)
                var buttonState = (event.buttonState shr 4) or (1 shl if (toolType == MotionEvent.TOOL_TYPE_STYLUS) 0 else 30)
                if ((event.buttonState and MotionEvent.BUTTON_TERTIARY) != 0) {
                    buttonState = buttonState or 0x08
                }
                SDLActivity.onNativePen(
                    pointerId,
                    SDLActivity.getMotionListener().getPenDeviceType(event.device),
                    buttonState,
                    action,
                    x,
                    y,
                    pressure,
                )
            } else {
                val pointerId = event.getPointerId(i)
                val x = getNormalizedX(event.getX(i))
                val y = getNormalizedY(event.getY(i))
                val pressure = min(event.getPressure(i), 1.0f)
                SDLActivity.onNativeTouch(touchDevId, pointerId, action, x, y, pressure)
            }
            if (action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_POINTER_DOWN) {
                break
            }
            i++
        } while (i < pointerCount)
        scaleGestureDetector.onTouchEvent(event)
        return true
    }

    protected fun enableSensor(sensorType: Int, enabled: Boolean) {
        if (enabled) {
            mSensorManager.registerListener(
                this,
                mSensorManager.getDefaultSensor(sensorType),
                SensorManager.SENSOR_DELAY_GAME,
                null,
            )
        } else {
            mSensorManager.unregisterListener(this, mSensorManager.getDefaultSensor(sensorType))
        }
    }

    override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {
        // TODO
    }

    override fun onSensorChanged(event: SensorEvent) {
        if (event.sensor.type == Sensor.TYPE_ACCELEROMETER) {
            val (x, y, newRotation) = when (mDisplay.rotation) {
                Surface.ROTATION_90 -> Triple(-event.values[1], event.values[0], 90)
                Surface.ROTATION_180 -> Triple(-event.values[0], -event.values[1], 180)
                Surface.ROTATION_270 -> Triple(event.values[1], -event.values[0], 270)
                else -> Triple(event.values[0], event.values[1], 0)
            }
            if (newRotation != SDLActivity.mCurrentRotation) {
                SDLActivity.mCurrentRotation = newRotation
                SDLActivity.onNativeRotationChanged(newRotation)
            }
            SDLActivity.onNativeAccel(-x / SensorManager.GRAVITY_EARTH, y / SensorManager.GRAVITY_EARTH, event.values[2] / SensorManager.GRAVITY_EARTH)
        }
    }

    override fun onResolvePointerIcon(event: MotionEvent, pointerIndex: Int): PointerIcon? = try {
        super.onResolvePointerIcon(event, pointerIndex)
    } catch (_: NullPointerException) {
        null
    }

    override fun onCapturedPointerEvent(event: MotionEvent): Boolean {
        var action = event.actionMasked
        val pointerCount = event.pointerCount
        for (i in 0 until pointerCount) {
            when (action) {
                MotionEvent.ACTION_SCROLL -> {
                    SDLActivity.onNativeMouse(0, action, event.getAxisValue(MotionEvent.AXIS_HSCROLL, i), event.getAxisValue(MotionEvent.AXIS_VSCROLL, i), false)
                    return true
                }
                MotionEvent.ACTION_HOVER_MOVE, MotionEvent.ACTION_MOVE -> {
                    SDLActivity.onNativeMouse(0, action, event.getX(i), event.getY(i), true)
                    return true
                }
                MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.ACTION_BUTTON_RELEASE -> {
                    action = if (action == MotionEvent.ACTION_BUTTON_PRESS) MotionEvent.ACTION_DOWN else MotionEvent.ACTION_UP
                    SDLActivity.onNativeMouse(event.buttonState, action, event.getX(i), event.getY(i), true)
                    return true
                }
            }
        }
        return false
    }

    override fun onScale(detector: ScaleGestureDetector): Boolean {
        SDLActivity.onNativePinchUpdate(detector.scaleFactor)
        return true
    }

    override fun onScaleBegin(detector: ScaleGestureDetector): Boolean {
        SDLActivity.onNativePinchStart()
        return true
    }

    override fun onScaleEnd(detector: ScaleGestureDetector) {
        SDLActivity.onNativePinchEnd()
    }
}
