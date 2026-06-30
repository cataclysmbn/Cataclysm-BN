package org.libsdl.app

import android.app.Activity
import android.app.AlertDialog
import android.app.Dialog
import android.app.UiModeManager
import android.content.ActivityNotFoundException
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.PorterDuff
import android.graphics.drawable.Drawable
import android.hardware.Sensor
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.LocaleList
import android.os.Message
import android.os.ParcelFileDescriptor
import android.util.DisplayMetrics
import android.util.Log
import android.util.SparseArray
import android.view.Display
import android.view.Gravity
import android.view.InputDevice
import android.view.KeyEvent
import android.view.PointerIcon
import android.view.Surface
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputMethodManager
import android.webkit.MimeTypeMap
import android.widget.Button
import android.widget.LinearLayout
import android.widget.RelativeLayout
import android.widget.TextView
import android.widget.Toast
import android.window.OnBackInvokedCallback
import android.window.OnBackInvokedDispatcher
import java.io.FileNotFoundException
import java.util.Hashtable
import java.util.Locale
import kotlin.math.sqrt

/** SDL Activity */
open class SDLActivity : Activity(), View.OnSystemUiVisibilityChangeListener {
    private var backInvokedCallback: OnBackInvokedCallback? = null

    open fun main() {
        val library = mSingleton!!.getMainSharedObject()
        val function = mSingleton!!.getMainFunction()
        val arguments = mSingleton!!.getArguments()
        Log.v("SDL", "Running main function $function from library $library")
        nativeRunMain(library, function, arguments)
        Log.v("SDL", "Finished main function")
    }

    protected open fun getMainSharedObject(): String {
        val libraries = mSingleton!!.getLibraries()
        val library = if (libraries.isNotEmpty()) "lib${libraries[libraries.size - 1]}.so" else "libmain.so"
        return getContext().applicationInfo.nativeLibraryDir + "/" + library
    }

    protected open fun getMainFunction(): String = "SDL_main"

    protected open fun getLibraries(): Array<String> = arrayOf("SDL3", "main")

    open fun loadLibraries() {
        for (lib in getLibraries()) {
            SDL.loadLibrary(lib, this)
        }
    }

    protected open fun getArguments(): Array<String> = emptyArray()

    protected open fun createSDLSurface(context: Context): SDLSurface = SDLSurface(context)

    override fun onCreate(savedInstanceState: Bundle?) {
        Log.v(TAG, "Manufacturer: ${Build.MANUFACTURER}")
        Log.v(TAG, "Device: ${Build.DEVICE}")
        Log.v(TAG, "Model: ${Build.MODEL}")
        Log.v(TAG, "onCreate()")
        super.onCreate(savedInstanceState)

        if (mSDLMainFinished || mActivityCreated) {
            val allowRecreate = nativeAllowRecreateActivity()
            if (mSDLMainFinished) {
                Log.v(TAG, "SDL main() finished")
            }
            if (allowRecreate) {
                Log.v(TAG, "activity re-created")
            } else {
                Log.v(TAG, "activity finished")
                System.exit(0)
                return
            }
        }

        mActivityCreated = true

        try {
            Thread.currentThread().name = "SDLActivity"
        } catch (error: Exception) {
            Log.v(TAG, "modify thread properties failed $error")
        }

        var errorMsgBrokenLib = ""
        try {
            loadLibraries()
            mBrokenLibraries = false
        } catch (error: UnsatisfiedLinkError) {
            System.err.println(error.message)
            mBrokenLibraries = true
            errorMsgBrokenLib = error.message ?: ""
        } catch (error: Exception) {
            System.err.println(error.message)
            mBrokenLibraries = true
            errorMsgBrokenLib = error.message ?: ""
        }

        if (!mBrokenLibraries) {
            val expectedVersion = "$SDL_MAJOR_VERSION.$SDL_MINOR_VERSION.$SDL_MICRO_VERSION"
            val version = nativeGetVersion()
            if (version != expectedVersion) {
                mBrokenLibraries = true
                errorMsgBrokenLib = "SDL C/Java version mismatch (expected $expectedVersion, got $version)"
            }
        }

        if (mBrokenLibraries) {
            mSingleton = this
            AlertDialog.Builder(this)
                .setMessage(
                    "An error occurred while trying to start the application. Please try again and/or reinstall." +
                        System.getProperty("line.separator") +
                        System.getProperty("line.separator") +
                        "Error: " + errorMsgBrokenLib,
                )
                .setTitle("SDL Error")
                .setPositiveButton("Exit") { _: DialogInterface?, _: Int -> mSingleton?.finish() }
                .setCancelable(false)
                .create()
                .show()
            return
        }

        val runCount = nativeCheckSDLThreadCounter()
        if (runCount != 0) {
            val allowRecreate = nativeAllowRecreateActivity()
            if (allowRecreate) {
                Log.v(TAG, "activity re-created // run_count: $runCount")
            } else {
                Log.v(TAG, "activity finished // run_count: $runCount")
                System.exit(0)
                return
            }
        }

        SDL.setupJNI()
        SDL.initialize()
        mSingleton = this
        SDL.setContext(this)
        mClipboardHandler = SDLClipboardHandler()
        mHIDDeviceManager = HIDDeviceManager.acquire(this)
        mSurface = createSDLSurface(this)
        mLayout = RelativeLayout(this).apply { addView(mSurface) }

        nativeSetNaturalOrientation(getNaturalOrientation())
        mCurrentRotation = getCurrentRotation()
        onNativeRotationChanged(mCurrentRotation)

        try {
            @Suppress("DEPRECATION")
            mCurrentLocale = if (Build.VERSION.SDK_INT < 24) {
                getContext().resources.configuration.locale
            } else {
                getContext().resources.configuration.locales[0]
            }
        } catch (_: Exception) {
        }

        when (getContext().resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
            Configuration.UI_MODE_NIGHT_NO -> onNativeDarkModeChanged(false)
            Configuration.UI_MODE_NIGHT_YES -> onNativeDarkModeChanged(true)
        }

        setContentView(mLayout)
        setWindowStyle(false)
        window.decorView.setOnSystemUiVisibilityChangeListener(this)
        registerBackInvokedCallback()

        val intent = intent
        if (intent?.data != null) {
            val filename = intent.data?.path
            if (filename != null) {
                Log.v(TAG, "Got filename: $filename")
                onNativeDropFile(filename)
            }
        }
    }

    protected open fun pauseNativeThread() {
        mNextNativeState = NativeState.PAUSED
        mIsResumedCalled = false
        if (mBrokenLibraries) {
            return
        }
        handleNativeState()
    }

    protected open fun resumeNativeThread() {
        mNextNativeState = NativeState.RESUMED
        mIsResumedCalled = true
        if (mBrokenLibraries) {
            return
        }
        handleNativeState()
    }

    override fun onPause() {
        Log.v(TAG, "onPause()")
        super.onPause()
        mHIDDeviceManager?.setFrozen(true)
        if (!mHasMultiWindow) {
            pauseNativeThread()
        }
    }

    override fun onResume() {
        Log.v(TAG, "onResume()")
        super.onResume()
        mHIDDeviceManager?.setFrozen(false)
        if (!mHasMultiWindow) {
            resumeNativeThread()
        }
    }

    override fun onStop() {
        Log.v(TAG, "onStop()")
        super.onStop()
        if (mHasMultiWindow) {
            pauseNativeThread()
        }
    }

    override fun onStart() {
        Log.v(TAG, "onStart()")
        super.onStart()
        if (mHasMultiWindow) {
            resumeNativeThread()
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        Log.v(TAG, "onWindowFocusChanged(): $hasFocus")
        if (mBrokenLibraries) {
            return
        }
        mHasFocus = hasFocus
        if (hasFocus) {
            mNextNativeState = NativeState.RESUMED
            getMotionListener().reclaimRelativeMouseModeIfNeeded()
            handleNativeState()
            nativeFocusChanged(true)
        } else {
            nativeFocusChanged(false)
            if (!mHasMultiWindow) {
                mNextNativeState = NativeState.PAUSED
                handleNativeState()
            }
        }
    }

    override fun onTrimMemory(level: Int) {
        Log.v(TAG, "onTrimMemory()")
        super.onTrimMemory(level)
        if (mBrokenLibraries) {
            return
        }
        nativeLowMemory()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        Log.v(TAG, "onConfigurationChanged()")
        super.onConfigurationChanged(newConfig)
        if (mBrokenLibraries) {
            return
        }
        @Suppress("DEPRECATION")
        if (mCurrentLocale == null || mCurrentLocale != newConfig.locale) {
            mCurrentLocale = newConfig.locale
            onNativeLocaleChanged()
        }
        when (newConfig.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
            Configuration.UI_MODE_NIGHT_NO -> onNativeDarkModeChanged(false)
            Configuration.UI_MODE_NIGHT_YES -> onNativeDarkModeChanged(true)
        }
    }

    override fun onDestroy() {
        Log.v(TAG, "onDestroy()")
        mHIDDeviceManager?.let { HIDDeviceManager.release(it) }
        mHIDDeviceManager = null
        SDLAudioManager.release(this)
        if (mBrokenLibraries) {
            super.onDestroy()
            return
        }
        mSDLThread?.let { thread ->
            nativeSendQuit()
            try {
                thread.join(1000)
            } catch (error: Exception) {
                Log.v(TAG, "Problem stopping SDLThread: $error")
            }
        }
        unregisterBackInvokedCallback()
        nativeQuit()
        super.onDestroy()
    }

    private fun handleBackButton(): Boolean {
        if (!nativeGetHintBoolean("SDL_ANDROID_TRAP_BACK_BUTTON", false)) {
            return false
        }
        onNativeKeyDown(KeyEvent.KEYCODE_BACK)
        onNativeKeyUp(KeyEvent.KEYCODE_BACK)
        return true
    }

    private fun registerBackInvokedCallback() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU || backInvokedCallback != null) {
            return
        }
        backInvokedCallback = OnBackInvokedCallback {
            if (!handleBackButton() && !isFinishing) {
                super@SDLActivity.onBackPressed()
            }
        }
        onBackInvokedDispatcher.registerOnBackInvokedCallback(OnBackInvokedDispatcher.PRIORITY_DEFAULT, backInvokedCallback!!)
    }

    private fun unregisterBackInvokedCallback() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU || backInvokedCallback == null) {
            return
        }
        onBackInvokedDispatcher.unregisterOnBackInvokedCallback(backInvokedCallback!!)
        backInvokedCallback = null
    }

    override fun onBackPressed() {
        if (handleBackButton()) {
            return
        }
        if (!isFinishing) {
            super.onBackPressed()
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        val state = mFileDialogState
        if (state != null && state.requestCode == requestCode) {
            val filelist = if (data != null) {
                val singleFileUri = data.data
                if (singleFileUri == null) {
                    val clipData = data.clipData!!
                    Array(clipData.itemCount) { i -> clipData.getItemAt(i).uri.toString() }
                } else {
                    arrayOf(singleFileUri.toString())
                }
            } else {
                emptyArray()
            }
            onNativeFileDialog(requestCode, filelist, -1)
            mFileDialogState = null
        }
    }

    fun pressBackButton() {
        runOnUiThread {
            if (!isFinishing) {
                superOnBackPressed()
            }
        }
    }

    fun superOnBackPressed() {
        super.onBackPressed()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (mBrokenLibraries) {
            return false
        }
        val keyCode = event.keyCode
        if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN ||
            keyCode == KeyEvent.KEYCODE_VOLUME_UP ||
            keyCode == KeyEvent.KEYCODE_CAMERA ||
            keyCode == KeyEvent.KEYCODE_ZOOM_IN ||
            keyCode == KeyEvent.KEYCODE_ZOOM_OUT
        ) {
            return false
        }
        mDispatchingKeyEvent = true
        val result = super.dispatchKeyEvent(event)
        mDispatchingKeyEvent = false
        return result
    }

    protected open fun onUnhandledMessage(command: Int, param: Any?): Boolean = false

    var commandHandler: Handler = SDLCommandHandler()

    protected open fun sendCommand(command: Int, data: Any?): Boolean {
        val msg = commandHandler.obtainMessage()
        msg.arg1 = command
        msg.obj = data
        val result = commandHandler.sendMessage(msg)
        if (command == COMMAND_CHANGE_WINDOW_STYLE) {
            var shouldWait = false
            if (data is Int) {
                @Suppress("DEPRECATION")
                val display = (getSystemService(Context.WINDOW_SERVICE) as WindowManager).defaultDisplay
                val realMetrics = DisplayMetrics()
                @Suppress("DEPRECATION")
                display.getRealMetrics(realMetrics)
                val fullscreenLayout = realMetrics.widthPixels == mSurface!!.width && realMetrics.heightPixels == mSurface!!.height
                shouldWait = if (data == 1) !fullscreenLayout else fullscreenLayout
            }
            val context = getContext()
            if (shouldWait) {
                synchronized(context) {
                    try {
                        (context as java.lang.Object).wait(500)
                    } catch (error: InterruptedException) {
                        error.printStackTrace()
                    }
                }
            }
        }
        return result
    }

    open fun setOrientationBis(w: Int, h: Int, resizable: Boolean, hint: String?) {
        val orientationHint = hint ?: ""
        var orientationLandscape = -1
        var orientationPortrait = -1
        if (w <= 1 || h <= 1) {
            return
        }
        if (orientationHint.contains("LandscapeRight") && orientationHint.contains("LandscapeLeft")) {
            orientationLandscape = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
        } else if (orientationHint.contains("LandscapeLeft")) {
            orientationLandscape = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
        } else if (orientationHint.contains("LandscapeRight")) {
            orientationLandscape = ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
        }
        val containsPortrait = orientationHint.contains("Portrait ") || orientationHint.endsWith("Portrait")
        if (containsPortrait && orientationHint.contains("PortraitUpsideDown")) {
            orientationPortrait = ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
        } else if (containsPortrait) {
            orientationPortrait = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
        } else if (orientationHint.contains("PortraitUpsideDown")) {
            orientationPortrait = ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT
        }
        val isLandscapeAllowed = orientationLandscape != -1
        val isPortraitAllowed = orientationPortrait != -1
        val req = if (!isPortraitAllowed && !isLandscapeAllowed) {
            if (resizable) ActivityInfo.SCREEN_ORIENTATION_FULL_USER else if (w > h) ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE else ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
        } else if (resizable) {
            if (isPortraitAllowed && isLandscapeAllowed) ActivityInfo.SCREEN_ORIENTATION_FULL_USER else if (isLandscapeAllowed) orientationLandscape else orientationPortrait
        } else if (isPortraitAllowed && isLandscapeAllowed) {
            if (w > h) orientationLandscape else orientationPortrait
        } else {
            if (isLandscapeAllowed) orientationLandscape else orientationPortrait
        }
        Log.v(TAG, "setOrientation() requestedOrientation=$req width=$w height=$h resizable=$resizable hint=$orientationHint")
        mSingleton!!.requestedOrientation = req
    }

    val messageboxSelection = IntArray(1)

    fun messageboxShowMessageBox(
        flags: Int,
        title: String,
        message: String,
        buttonFlags: IntArray,
        buttonIds: IntArray,
        buttonTexts: Array<String>,
        colors: IntArray?,
    ): Int {
        messageboxSelection[0] = -1
        if (buttonFlags.size != buttonIds.size && buttonIds.size != buttonTexts.size) {
            return -1
        }
        val args = Bundle().apply {
            putInt("flags", flags)
            putString("title", title)
            putString("message", message)
            putIntArray("buttonFlags", buttonFlags)
            putIntArray("buttonIds", buttonIds)
            putStringArray("buttonTexts", buttonTexts)
            putIntArray("colors", colors)
        }
        runOnUiThread { messageboxCreateAndShow(args) }
        synchronized(messageboxSelection) {
            try {
                (messageboxSelection as java.lang.Object).wait()
            } catch (error: InterruptedException) {
                error.printStackTrace()
                return -1
            }
        }
        return messageboxSelection[0]
    }

    protected open fun messageboxCreateAndShow(args: Bundle) {
        val colors = args.getIntArray("colors")
        val backgroundColor: Int
        val textColor: Int
        val buttonBorderColor: Int
        val buttonBackgroundColor: Int
        val buttonSelectedColor: Int
        if (colors != null) {
            var i = -1
            backgroundColor = colors[++i]
            textColor = colors[++i]
            buttonBorderColor = colors[++i]
            buttonBackgroundColor = colors[++i]
            buttonSelectedColor = colors[++i]
        } else {
            backgroundColor = Color.TRANSPARENT
            textColor = Color.TRANSPARENT
            buttonBorderColor = Color.TRANSPARENT
            buttonBackgroundColor = Color.TRANSPARENT
            buttonSelectedColor = Color.TRANSPARENT
        }
        val dialog = AlertDialog.Builder(this).create()
        dialog.setTitle(args.getString("title"))
        dialog.setCancelable(false)
        dialog.setOnDismissListener {
            synchronized(messageboxSelection) {
                (messageboxSelection as java.lang.Object).notify()
            }
        }
        val message = TextView(this).apply {
            gravity = Gravity.CENTER
            text = args.getString("message")
            if (textColor != Color.TRANSPARENT) {
                setTextColor(textColor)
            }
        }
        val buttonFlags = args.getIntArray("buttonFlags")!!
        val buttonIds = args.getIntArray("buttonIds")!!
        val buttonTexts = args.getStringArray("buttonTexts")!!
        val mapping = SparseArray<Button>()
        val buttons = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
        }
        for (i in buttonTexts.indices) {
            val button = Button(this)
            val id = buttonIds[i]
            button.setOnClickListener {
                messageboxSelection[0] = id
                dialog.dismiss()
            }
            if (buttonFlags[i] != 0) {
                if ((buttonFlags[i] and 0x00000001) != 0) {
                    mapping.put(KeyEvent.KEYCODE_ENTER, button)
                }
                if ((buttonFlags[i] and 0x00000002) != 0) {
                    mapping.put(KeyEvent.KEYCODE_ESCAPE, button)
                }
            }
            button.text = buttonTexts[i]
            if (textColor != Color.TRANSPARENT) {
                button.setTextColor(textColor)
            }
            if (buttonBorderColor != Color.TRANSPARENT) {
                // TODO set color for border of messagebox button
            }
            if (buttonBackgroundColor != Color.TRANSPARENT) {
                val drawable: Drawable? = button.background
                if (drawable == null) {
                    button.setBackgroundColor(buttonBackgroundColor)
                } else {
                    drawable.setColorFilter(buttonBackgroundColor, PorterDuff.Mode.MULTIPLY)
                }
            }
            if (buttonSelectedColor != Color.TRANSPARENT) {
                // TODO set color for selected messagebox button
            }
            buttons.addView(button)
        }
        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(message)
            addView(buttons)
            if (backgroundColor != Color.TRANSPARENT) {
                setBackgroundColor(backgroundColor)
            }
        }
        dialog.setView(content)
        dialog.setOnKeyListener(DialogInterface.OnKeyListener { _: DialogInterface?, keyCode: Int, event: KeyEvent ->
            val button = mapping.get(keyCode)
            if (button != null) {
                if (event.action == KeyEvent.ACTION_UP) {
                    button.performClick()
                }
                true
            } else {
                false
            }
        })
        dialog.show()
    }

    private val rehideSystemUi = Runnable {
        val flags = View.SYSTEM_UI_FLAG_FULLSCREEN or
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.INVISIBLE
        window.decorView.systemUiVisibility = flags
    }

    override fun onSystemUiVisibilityChange(visibility: Int) {
        if (mFullscreenModeActive && ((visibility and View.SYSTEM_UI_FLAG_FULLSCREEN) == 0 || (visibility and View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0)) {
            val handler = window.decorView.handler
            if (handler != null) {
                handler.removeCallbacks(rehideSystemUi)
                handler.postDelayed(rehideSystemUi, 2000)
            }
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        val result = grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED
        nativePermissionResult(requestCode, result)
    }

    enum class NativeState { INIT, RESUMED, PAUSED }

    protected class SDLCommandHandler : Handler() {
        override fun handleMessage(msg: Message) {
            val context = getContext()
            when (msg.arg1) {
                COMMAND_CHANGE_TITLE -> {
                    if (context is Activity) {
                        context.title = msg.obj as String
                    } else {
                        Log.e(TAG, "error handling message, getContext() returned no Activity")
                    }
                }
                COMMAND_CHANGE_WINDOW_STYLE -> {
                    if (context is Activity) {
                        val window = context.window
                        if (msg.obj is Int && msg.obj as Int != 0) {
                            val flags = View.SYSTEM_UI_FLAG_FULLSCREEN or
                                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.INVISIBLE
                            window.decorView.systemUiVisibility = flags
                            window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
                            window.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN)
                            mFullscreenModeActive = true
                        } else {
                            val flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_VISIBLE
                            window.decorView.systemUiVisibility = flags
                            window.addFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN)
                            window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
                            mFullscreenModeActive = false
                        }
                        if (Build.VERSION.SDK_INT >= 30) {
                            window.attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
                        }
                        if (Build.VERSION.SDK_INT >= 30 && Build.VERSION.SDK_INT < 35) {
                            onNativeInsetsChanged(0, 0, 0, 0)
                        }
                    } else {
                        Log.e(TAG, "error handling message, getContext() returned no Activity")
                    }
                }
                COMMAND_TEXTEDIT_HIDE -> {
                    val textEdit = mTextEdit
                    if (textEdit != null) {
                        textEdit.layoutParams = RelativeLayout.LayoutParams(0, 0)
                        val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                        imm.hideSoftInputFromWindow(textEdit.windowToken, 0)
                        onNativeScreenKeyboardHidden()
                        mSurface?.requestFocus()
                    }
                }
                COMMAND_SET_KEEP_SCREEN_ON -> {
                    if (context is Activity) {
                        if (msg.obj is Int && msg.obj as Int != 0) {
                            context.window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        } else {
                            context.window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        }
                    }
                }
                else -> {
                    if (context is SDLActivity && !context.onUnhandledMessage(msg.arg1, msg.obj)) {
                        Log.e(TAG, "error handling message, command is ${msg.arg1}")
                    }
                }
            }
        }
    }

    class ShowTextInputTask(var input_type: Int, var x: Int, var y: Int, var w: Int, var h: Int) : Runnable {
        init {
            if (w <= 0) {
                w = 1
            }
            if (h + HEIGHT_PADDING <= 0) {
                h = 1 - HEIGHT_PADDING
            }
        }

        override fun run() {
            val params = RelativeLayout.LayoutParams(w, h + HEIGHT_PADDING).apply {
                leftMargin = x
                topMargin = y
            }
            if (mTextEdit == null) {
                mTextEdit = SDLDummyEdit(getContext())
                mLayout!!.addView(mTextEdit, params)
            } else {
                mTextEdit!!.layoutParams = params
            }
            mTextEdit.setInputType(input_type)
            mTextEdit!!.visibility = View.VISIBLE
            mTextEdit!!.requestFocus()
            val imm = getContext().getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.showSoftInput(mTextEdit, 0)
            if (imm.isAcceptingText) {
                onNativeScreenKeyboardShown()
            }
        }

        companion object {
            const val HEIGHT_PADDING = 15
        }
    }

    class SDLFileDialogState {
        var requestCode = 0
        var multipleChoice = false
    }

    companion object {
        private const val TAG = "SDL"
        private const val SDL_MAJOR_VERSION = 3
        private const val SDL_MINOR_VERSION = 4
        private const val SDL_MICRO_VERSION = 2

        @JvmField var mIsResumedCalled = false
        @JvmField var mHasFocus = true
        @JvmField val mHasMultiWindow = Build.VERSION.SDK_INT >= 24

        private const val SDL_SYSTEM_CURSOR_ARROW = 0
        private const val SDL_SYSTEM_CURSOR_IBEAM = 1
        private const val SDL_SYSTEM_CURSOR_WAIT = 2
        private const val SDL_SYSTEM_CURSOR_CROSSHAIR = 3
        private const val SDL_SYSTEM_CURSOR_WAITARROW = 4
        private const val SDL_SYSTEM_CURSOR_SIZENWSE = 5
        private const val SDL_SYSTEM_CURSOR_SIZENESW = 6
        private const val SDL_SYSTEM_CURSOR_SIZEWE = 7
        private const val SDL_SYSTEM_CURSOR_SIZENS = 8
        private const val SDL_SYSTEM_CURSOR_SIZEALL = 9
        private const val SDL_SYSTEM_CURSOR_NO = 10
        private const val SDL_SYSTEM_CURSOR_HAND = 11
        private const val SDL_SYSTEM_CURSOR_WINDOW_TOPLEFT = 12
        private const val SDL_SYSTEM_CURSOR_WINDOW_TOP = 13
        private const val SDL_SYSTEM_CURSOR_WINDOW_TOPRIGHT = 14
        private const val SDL_SYSTEM_CURSOR_WINDOW_RIGHT = 15
        private const val SDL_SYSTEM_CURSOR_WINDOW_BOTTOMRIGHT = 16
        private const val SDL_SYSTEM_CURSOR_WINDOW_BOTTOM = 17
        private const val SDL_SYSTEM_CURSOR_WINDOW_BOTTOMLEFT = 18
        private const val SDL_SYSTEM_CURSOR_WINDOW_LEFT = 19

        const val SDL_ORIENTATION_UNKNOWN = 0
        const val SDL_ORIENTATION_LANDSCAPE = 1
        const val SDL_ORIENTATION_LANDSCAPE_FLIPPED = 2
        const val SDL_ORIENTATION_PORTRAIT = 3
        const val SDL_ORIENTATION_PORTRAIT_FLIPPED = 4

        @JvmField var mCurrentRotation = 0
        @JvmField var mCurrentLocale: Locale? = null
        @JvmField var mNextNativeState = NativeState.INIT
        @JvmField var mCurrentNativeState = NativeState.INIT
        @JvmField var mBrokenLibraries = true
        @JvmField var mSingleton: SDLActivity = nullValue()
        @JvmField var mSurface: SDLSurface = nullValue()
        @JvmField internal var mTextEdit: SDLDummyEdit = nullValue()
        @JvmField var mLayout: ViewGroup = nullValue()
        @JvmField var mClipboardHandler: SDLClipboardHandler = nullValue()
        @JvmField var mCursors = Hashtable<Int, PointerIcon>()
        @JvmField var mLastCursorID = 0
        @JvmField var mMotionListener: SDLGenericMotionListener_API14 = nullValue()
        @JvmField var mHIDDeviceManager: HIDDeviceManager? = null
        @JvmField var mSDLThread: Thread? = null

        @Suppress("UNCHECKED_CAST")
        private fun <T> nullValue(): T = null as T
        @JvmField var mSDLMainFinished = false
        @JvmField var mActivityCreated = false
        private var mFileDialogState: SDLFileDialogState? = null
        @JvmField var mDispatchingKeyEvent = false

        const val COMMAND_CHANGE_TITLE = 1
        const val COMMAND_CHANGE_WINDOW_STYLE = 2
        const val COMMAND_TEXTEDIT_HIDE = 3
        const val COMMAND_SET_KEEP_SCREEN_ON = 5
        const val COMMAND_USER = 0x8000
        @JvmField var mFullscreenModeActive = false

        @JvmStatic fun getMotionListener(): SDLGenericMotionListener_API14 {
            if (mMotionListener == null) {
                mMotionListener = when {
                    Build.VERSION.SDK_INT >= 29 -> SDLGenericMotionListener_API29()
                    Build.VERSION.SDK_INT >= 26 -> SDLGenericMotionListener_API26()
                    Build.VERSION.SDK_INT >= 24 -> SDLGenericMotionListener_API24()
                    else -> SDLGenericMotionListener_API14()
                }
            }
            return mMotionListener!!
        }

        @JvmStatic fun initialize() {
            mSingleton = nullValue()
            mSurface = nullValue()
            mTextEdit = nullValue()
            mLayout = nullValue()
            mClipboardHandler = nullValue()
            mCursors = Hashtable()
            mLastCursorID = 0
            mSDLThread = null
            mIsResumedCalled = false
            mHasFocus = true
            mNextNativeState = NativeState.INIT
            mCurrentNativeState = NativeState.INIT
        }

        @JvmStatic fun getNaturalOrientation(): Int {
            var result = SDL_ORIENTATION_UNKNOWN
            val activity = getContext()
            @Suppress("DEPRECATION")
            val display: Display = activity.windowManager.defaultDisplay
            val config = activity.resources.configuration
            @Suppress("DEPRECATION")
            val rotation = display.rotation
            result = if (((rotation == Surface.ROTATION_0 || rotation == Surface.ROTATION_180) && config.orientation == Configuration.ORIENTATION_LANDSCAPE) ||
                ((rotation == Surface.ROTATION_90 || rotation == Surface.ROTATION_270) && config.orientation == Configuration.ORIENTATION_PORTRAIT)
            ) {
                SDL_ORIENTATION_LANDSCAPE
            } else {
                SDL_ORIENTATION_PORTRAIT
            }
            return result
        }

        @JvmStatic fun getCurrentRotation(): Int {
            val activity = getContext()
            @Suppress("DEPRECATION")
            return when (activity.windowManager.defaultDisplay.rotation) {
                Surface.ROTATION_90 -> 90
                Surface.ROTATION_180 -> 180
                Surface.ROTATION_270 -> 270
                else -> 0
            }
        }

        @JvmStatic fun dispatchingKeyEvent(): Boolean = mDispatchingKeyEvent

        @JvmStatic fun handleNativeState() {
            if (mNextNativeState == mCurrentNativeState) {
                return
            }
            if (mNextNativeState == NativeState.INIT) {
                mCurrentNativeState = mNextNativeState
                return
            }
            if (mNextNativeState == NativeState.PAUSED) {
                if (mSDLThread != null) {
                    nativePause()
                }
                mSurface.handlePause()
                mCurrentNativeState = mNextNativeState
                return
            }
            if (mNextNativeState == NativeState.RESUMED) {
                val surface = mSurface
                if (isSurfaceReady(surface) && (mHasFocus || mHasMultiWindow) && mIsResumedCalled) {
                    if (mSDLThread == null) {
                        mSDLThread = Thread(SDLMain(), "SDLThread")
                        enableSurfaceSensor(surface, Sensor.TYPE_ACCELEROMETER, true)
                        mSDLThread!!.start()
                    } else {
                        nativeResume()
                    }
                    surface.handleResume()
                    mCurrentNativeState = mNextNativeState
                }
            }
        }

        private fun isSurfaceReady(surface: SDLSurface): Boolean = try {
            val field = SDLSurface::class.java.getDeclaredField("mIsSurfaceReady")
            field.isAccessible = true
            field.getBoolean(surface)
        } catch (_: Exception) {
            false
        }

        private fun enableSurfaceSensor(surface: SDLSurface, sensorType: Int, enabled: Boolean) {
            val method = SDLSurface::class.java.getDeclaredMethod("enableSensor", Int::class.javaPrimitiveType, Boolean::class.javaPrimitiveType)
            method.isAccessible = true
            method.invoke(surface, sensorType, enabled)
        }

        @JvmStatic external fun nativeGetVersion(): String
        @JvmStatic external fun nativeSetupJNI()
        @JvmStatic external fun nativeInitMainThread()
        @JvmStatic external fun nativeCleanupMainThread()
        @JvmStatic external fun nativeRunMain(library: String, function: String, arguments: Any): Int
        @JvmStatic external fun nativeLowMemory()
        @JvmStatic external fun nativeSendQuit()
        @JvmStatic external fun nativeQuit()
        @JvmStatic external fun nativePause()
        @JvmStatic external fun nativeResume()
        @JvmStatic external fun nativeFocusChanged(hasFocus: Boolean)
        @JvmStatic external fun onNativeDropFile(filename: String)
        @JvmStatic external fun nativeSetScreenResolution(surfaceWidth: Int, surfaceHeight: Int, deviceWidth: Int, deviceHeight: Int, density: Float, rate: Float)
        @JvmStatic external fun onNativeResize()
        @JvmStatic external fun onNativeKeyDown(keycode: Int)
        @JvmStatic external fun onNativeKeyUp(keycode: Int)
        @JvmStatic external fun onNativeSoftReturnKey(): Boolean
        @JvmStatic external fun onNativeKeyboardFocusLost()
        @JvmStatic external fun onNativeMouse(button: Int, action: Int, x: Float, y: Float, relative: Boolean)
        @JvmStatic external fun onNativeTouch(touchDevId: Int, pointerFingerId: Int, action: Int, x: Float, y: Float, p: Float)
        @JvmStatic external fun onNativePen(penId: Int, device_type: Int, button: Int, action: Int, x: Float, y: Float, p: Float)
        @JvmStatic external fun onNativeAccel(x: Float, y: Float, z: Float)
        @JvmStatic external fun onNativeClipboardChanged()
        @JvmStatic external fun onNativeSurfaceCreated()
        @JvmStatic external fun onNativeSurfaceChanged()
        @JvmStatic external fun onNativeSurfaceDestroyed()
        @JvmStatic external fun onNativeScreenKeyboardShown()
        @JvmStatic external fun onNativeScreenKeyboardHidden()
        @JvmStatic external fun nativeGetHint(name: String): String
        @JvmStatic external fun nativeGetHintBoolean(name: String, default_value: Boolean): Boolean
        @JvmStatic external fun nativeSetenv(name: String, value: String)
        @JvmStatic external fun nativeSetNaturalOrientation(orientation: Int)
        @JvmStatic external fun onNativeRotationChanged(rotation: Int)
        @JvmStatic external fun onNativeInsetsChanged(left: Int, right: Int, top: Int, bottom: Int)
        @JvmStatic external fun nativeAddTouch(touchId: Int, name: String)
        @JvmStatic external fun nativePermissionResult(requestCode: Int, result: Boolean)
        @JvmStatic external fun onNativeLocaleChanged()
        @JvmStatic external fun onNativeDarkModeChanged(enabled: Boolean)
        @JvmStatic external fun nativeAllowRecreateActivity(): Boolean
        @JvmStatic external fun nativeCheckSDLThreadCounter(): Int
        @JvmStatic external fun onNativeFileDialog(requestCode: Int, filelist: Array<String>, filter: Int)
        @JvmStatic external fun onNativePinchStart()
        @JvmStatic external fun onNativePinchUpdate(scale: Float)
        @JvmStatic external fun onNativePinchEnd()

        @JvmStatic fun setActivityTitle(title: String): Boolean = mSingleton!!.sendCommand(COMMAND_CHANGE_TITLE, title)
        @JvmStatic fun setWindowStyle(fullscreen: Boolean) { mSingleton!!.sendCommand(COMMAND_CHANGE_WINDOW_STYLE, if (fullscreen) 1 else 0) }
        @JvmStatic fun setOrientation(w: Int, h: Int, resizable: Boolean, hint: String?) { mSingleton.setOrientationBis(w, h, resizable, hint) }
        @JvmStatic fun minimizeWindow() {
            val singleton = mSingleton ?: return
            val startMain = Intent(Intent.ACTION_MAIN).apply {
                addCategory(Intent.CATEGORY_HOME)
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            }
            singleton.startActivity(startMain)
        }
        @JvmStatic fun shouldMinimizeOnFocusLoss(): Boolean = false
        @JvmStatic fun supportsRelativeMouse(): Boolean {
            if (Build.VERSION.SDK_INT < 27 && isDeXMode()) {
                return false
            }
            return getMotionListener().supportsRelativeMouse()
        }
        @JvmStatic fun setRelativeMouseEnabled(enabled: Boolean): Boolean = if (enabled && !supportsRelativeMouse()) false else getMotionListener().setRelativeMouseEnabled(enabled)
        @JvmStatic fun sendMessage(command: Int, param: Int): Boolean = mSingleton.sendCommand(command, param)
        @JvmStatic fun getContext(): Activity = SDL.getContext()!!
        @JvmStatic fun isAndroidTV(): Boolean {
            val uiModeManager = getContext().getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
            if (uiModeManager.currentModeType == Configuration.UI_MODE_TYPE_TELEVISION) return true
            if (Build.MANUFACTURER == "MINIX" && Build.MODEL == "NEO-U1") return true
            if (Build.MANUFACTURER == "Amlogic" && (Build.MODEL.startsWith("TV") || Build.MODEL == "X96-W" || Build.MODEL == "A95X-R1")) return true
            return false
        }
        @JvmStatic fun isVRHeadset(): Boolean = (Build.MANUFACTURER == "Oculus" && Build.MODEL.startsWith("Quest")) || Build.MANUFACTURER == "Pico"
        @JvmStatic fun getDiagonal(): Double {
            val metrics = DisplayMetrics()
            @Suppress("DEPRECATION")
            getContext().windowManager.defaultDisplay.getMetrics(metrics)
            val widthInches = metrics.widthPixels / metrics.xdpi.toDouble()
            val heightInches = metrics.heightPixels / metrics.ydpi.toDouble()
            return sqrt(widthInches * widthInches + heightInches * heightInches)
        }
        @JvmStatic fun isTablet(): Boolean = getDiagonal() >= 7.0
        @JvmStatic fun isChromebook(): Boolean {
            val packageManager = getContext().packageManager
            if (packageManager.hasSystemFeature("org.chromium.arc") || packageManager.hasSystemFeature("org.chromium.arc.device_management")) return true
            return Build.MODEL != null && Build.MODEL.startsWith("sdk_gpc_")
        }
        @JvmStatic fun isDeXMode(): Boolean {
            if (Build.VERSION.SDK_INT < 24) return false
            return try {
                val config = getContext().resources.configuration
                val configClass: Class<*> = config.javaClass
                configClass.getField("SEM_DESKTOP_MODE_ENABLED").getInt(configClass) == configClass.getField("semDesktopModeEnabled").getInt(config)
            } catch (_: Exception) {
                false
            }
        }
        @JvmStatic fun getManifestEnvironmentVariables(): Boolean {
            try {
                val applicationInfo = getContext().packageManager.getApplicationInfo(getContext().packageName, PackageManager.GET_META_DATA)
                val bundle = applicationInfo.metaData ?: return false
                val prefix = "SDL_ENV."
                for (key in bundle.keySet()) {
                    if (key.startsWith(prefix)) {
                        nativeSetenv(key.substring(prefix.length), bundle[key].toString())
                    }
                }
                return true
            } catch (error: Exception) {
                Log.v(TAG, "exception $error")
            }
            return false
        }
        @JvmStatic fun getContentView(): View = mLayout!!
        @JvmStatic fun showTextInput(input_type: Int, x: Int, y: Int, w: Int, h: Int): Boolean = mSingleton!!.commandHandler.post(ShowTextInputTask(input_type, x, y, w, h))
        @JvmStatic fun isTextInputEvent(event: KeyEvent): Boolean = !event.isCtrlPressed && (event.isPrintingKey || event.keyCode == KeyEvent.KEYCODE_SPACE)
        @JvmStatic fun handleKeyEvent(v: View?, keyCode: Int, event: KeyEvent, ic: InputConnection?): Boolean {
            val deviceId = event.deviceId
            var source = event.source
            if (source == InputDevice.SOURCE_UNKNOWN) {
                val device = InputDevice.getDevice(deviceId)
                if (device != null) source = device.sources
            }
            if (SDLControllerManager.isDeviceSDLJoystick(deviceId)) {
                if (event.action == KeyEvent.ACTION_DOWN) {
                    if (SDLControllerManager.onNativePadDown(deviceId, keyCode)) return true
                } else if (event.action == KeyEvent.ACTION_UP) {
                    if (SDLControllerManager.onNativePadUp(deviceId, keyCode)) return true
                }
            }
            if ((source and InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE && !isVRHeadset()) {
                if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_FORWARD) {
                    when (event.action) {
                        KeyEvent.ACTION_DOWN, KeyEvent.ACTION_UP -> return true
                    }
                }
            }
            if (event.action == KeyEvent.ACTION_DOWN) {
                onNativeKeyDown(keyCode)
                if (isTextInputEvent(event)) {
                    val text = event.unicodeChar.toChar().toString()
                    if (ic != null) ic.commitText(text, 1) else SDLInputConnection.nativeCommitText(text, 1)
                }
                return true
            } else if (event.action == KeyEvent.ACTION_UP) {
                onNativeKeyUp(keyCode)
                return true
            }
            return false
        }
        @JvmStatic fun getNativeSurface(): Surface? = mSurface?.getNativeSurface()
        @JvmStatic fun initTouch() {
            for (id in InputDevice.getDeviceIds()) {
                val device = InputDevice.getDevice(id)
                if (device != null && ((device.sources and InputDevice.SOURCE_TOUCHSCREEN) == InputDevice.SOURCE_TOUCHSCREEN || device.isVirtual)) {
                    nativeAddTouch(device.id, device.name)
                }
            }
        }
        @JvmStatic fun manualBackButton() { mSingleton!!.pressBackButton() }
        @JvmStatic fun clipboardHasText(): Boolean = mClipboardHandler!!.clipboardHasText()
        @JvmStatic fun clipboardGetText(): String? = mClipboardHandler!!.clipboardGetText()
        @JvmStatic fun clipboardSetText(string: String) { mClipboardHandler!!.clipboardSetText(string) }
        @JvmStatic fun createCustomCursor(colors: IntArray, width: Int, height: Int, hotSpotX: Int, hotSpotY: Int): Int {
            val bitmap = Bitmap.createBitmap(colors, width, height, Bitmap.Config.ARGB_8888)
            ++mLastCursorID
            if (Build.VERSION.SDK_INT >= 24) {
                try {
                    mCursors[mLastCursorID] = PointerIcon.create(bitmap, hotSpotX.toFloat(), hotSpotY.toFloat())
                } catch (_: Exception) {
                    return 0
                }
            } else {
                return 0
            }
            return mLastCursorID
        }
        @JvmStatic fun destroyCustomCursor(cursorID: Int) {
            if (Build.VERSION.SDK_INT >= 24) {
                try {
                    mCursors.remove(cursorID)
                } catch (_: Exception) {
                }
            }
        }
        @JvmStatic fun setCustomCursor(cursorID: Int): Boolean {
            if (Build.VERSION.SDK_INT >= 24) {
                try {
                    mSurface!!.pointerIcon = mCursors[cursorID]
                } catch (_: Exception) {
                    return false
                }
            } else {
                return false
            }
            return true
        }
        @JvmStatic fun setSystemCursor(cursorID: Int): Boolean {
            val cursorType = when (cursorID) {
                SDL_SYSTEM_CURSOR_ARROW -> 1000
                SDL_SYSTEM_CURSOR_IBEAM -> 1008
                SDL_SYSTEM_CURSOR_WAIT, SDL_SYSTEM_CURSOR_WAITARROW -> 1004
                SDL_SYSTEM_CURSOR_CROSSHAIR -> 1007
                SDL_SYSTEM_CURSOR_SIZENWSE, SDL_SYSTEM_CURSOR_WINDOW_TOPLEFT, SDL_SYSTEM_CURSOR_WINDOW_BOTTOMRIGHT -> 1017
                SDL_SYSTEM_CURSOR_SIZENESW, SDL_SYSTEM_CURSOR_WINDOW_TOPRIGHT, SDL_SYSTEM_CURSOR_WINDOW_BOTTOMLEFT -> 1016
                SDL_SYSTEM_CURSOR_SIZEWE, SDL_SYSTEM_CURSOR_WINDOW_RIGHT, SDL_SYSTEM_CURSOR_WINDOW_LEFT -> 1014
                SDL_SYSTEM_CURSOR_SIZENS, SDL_SYSTEM_CURSOR_WINDOW_TOP, SDL_SYSTEM_CURSOR_WINDOW_BOTTOM -> 1015
                SDL_SYSTEM_CURSOR_SIZEALL -> 1020
                SDL_SYSTEM_CURSOR_NO -> 1012
                SDL_SYSTEM_CURSOR_HAND -> 1002
                else -> 0
            }
            if (Build.VERSION.SDK_INT >= 24) {
                try {
                    mSurface!!.pointerIcon = PointerIcon.getSystemIcon(getContext(), cursorType)
                } catch (_: Exception) {
                    return false
                }
            }
            return true
        }
        @JvmStatic fun requestPermission(permission: String, requestCode: Int) {
            if (Build.VERSION.SDK_INT < 23) {
                nativePermissionResult(requestCode, true)
                return
            }
            val activity = getContext()
            if (activity.checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                activity.requestPermissions(arrayOf(permission), requestCode)
            } else {
                nativePermissionResult(requestCode, true)
            }
        }
        @JvmStatic fun openURL(url: String): Boolean = try {
            val intent = Intent(Intent.ACTION_VIEW).apply {
                data = Uri.parse(url)
                addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY or Intent.FLAG_ACTIVITY_MULTIPLE_TASK or Intent.FLAG_ACTIVITY_NEW_DOCUMENT)
            }
            mSingleton!!.startActivity(intent)
            true
        } catch (_: Exception) {
            false
        }
        @JvmStatic fun showToast(message: String, duration: Int, gravity: Int, xOffset: Int, yOffset: Int): Boolean {
            val singleton = mSingleton ?: return false
            return try {
                singleton.runOnUiThread {
                    try {
                        val toast = Toast.makeText(singleton, message, duration)
                        if (gravity >= 0) toast.setGravity(gravity, xOffset, yOffset)
                        toast.show()
                    } catch (error: Exception) {
                        Log.e(TAG, error.message ?: "")
                    }
                }
                true
            } catch (_: Exception) {
                false
            }
        }
        @JvmStatic @Throws(Exception::class) fun openFileDescriptor(uri: String, mode: String): Int {
            val singleton = mSingleton ?: return -1
            return try {
                val pfd: ParcelFileDescriptor? = singleton.contentResolver.openFileDescriptor(Uri.parse(uri), mode)
                pfd?.detachFd() ?: -1
            } catch (error: FileNotFoundException) {
                error.printStackTrace()
                -1
            }
        }
        @JvmStatic fun showFileDialog(filters: Array<String>?, allowMultiple: Boolean, forWrite: Boolean, requestCode: Int): Boolean {
            val singleton = mSingleton ?: return false
            val multiple = if (forWrite) false else allowMultiple
            val mimes = ArrayList<String>()
            val mimeTypeMap = MimeTypeMap.getSingleton()
            if (filters != null) {
                for (pattern in filters) {
                    val extensions = pattern.split(";")
                    if (extensions.size == 1 && extensions[0] == "*") {
                        mimes.add("*/*")
                    } else {
                        for (ext in extensions) {
                            val mime = mimeTypeMap.getMimeTypeFromExtension(ext)
                            if (mime != null) mimes.add(mime)
                        }
                    }
                }
            }
            val intent = Intent(if (forWrite) Intent.ACTION_CREATE_DOCUMENT else Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                putExtra(Intent.EXTRA_ALLOW_MULTIPLE, multiple)
                when (mimes.size) {
                    0 -> type = "*/*"
                    1 -> type = mimes[0]
                    else -> {
                        type = "*/*"
                        putExtra(Intent.EXTRA_MIME_TYPES, mimes.toArray(arrayOf<String>()))
                    }
                }
            }
            try {
                singleton.startActivityForResult(intent, requestCode)
            } catch (error: ActivityNotFoundException) {
                Log.e(TAG, "Unable to open file dialog.", error)
                return false
            }
            mFileDialogState = SDLFileDialogState().apply {
                this.requestCode = requestCode
                multipleChoice = multiple
            }
            return true
        }
        @JvmStatic fun getPreferredLocales(): String {
            var result = ""
            if (Build.VERSION.SDK_INT >= 24) {
                val locales = LocaleList.getAdjustedDefault()
                for (i in 0 until locales.size()) {
                    if (i != 0) result += ","
                    result += formatLocale(locales[i])
                }
            } else if (mCurrentLocale != null) {
                result = formatLocale(mCurrentLocale!!)
            }
            return result
        }
        @JvmStatic fun formatLocale(locale: Locale): String {
            val lang = when (locale.language) {
                "in" -> "id"
                "" -> "und"
                else -> locale.language
            }
            return if (locale.country == "") lang else lang + "_" + locale.country
        }
    }
}

/** Simple runnable to start the SDL application */
class SDLMain : Runnable {
    override fun run() {
        try {
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_DISPLAY)
        } catch (error: Exception) {
            Log.v("SDL", "modify thread properties failed $error")
        }
        SDLActivity.nativeInitMainThread()
        SDLActivity.mSingleton!!.main()
        SDLActivity.nativeCleanupMainThread()
        val singleton = SDLActivity.mSingleton
        if (singleton != null && !singleton.isFinishing) {
            SDLActivity.mSDLThread = null
            SDLActivity.mSDLMainFinished = true
            singleton.finish()
        }
    }
}

class SDLClipboardHandler : ClipboardManager.OnPrimaryClipChangedListener {
    protected val clipMgr: ClipboardManager = SDL.getContext()!!.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    init {
        clipMgr.addPrimaryClipChangedListener(this)
    }

    fun clipboardHasText(): Boolean = if (Build.VERSION.SDK_INT >= 28) clipMgr.hasPrimaryClip() else {
        @Suppress("DEPRECATION")
        clipMgr.hasText()
    }

    fun clipboardGetText(): String? {
        val clip = clipMgr.primaryClip
        if (clip != null) {
            val item = clip.getItemAt(0)
            val text = item?.text
            if (text != null) {
                return text.toString()
            }
        }
        return null
    }

    fun clipboardSetText(string: String) {
        clipMgr.removePrimaryClipChangedListener(this)
        if (string.isEmpty()) {
            if (Build.VERSION.SDK_INT >= 28) {
                clipMgr.clearPrimaryClip()
            } else {
                clipMgr.setPrimaryClip(ClipData.newPlainText(null, ""))
            }
        } else {
            clipMgr.setPrimaryClip(ClipData.newPlainText(null, string))
        }
        clipMgr.addPrimaryClipChangedListener(this)
    }

    override fun onPrimaryClipChanged() {
        SDLActivity.onNativeClipboardChanged()
    }
}
