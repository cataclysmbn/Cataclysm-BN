package org.libsdl.app

import android.app.Activity
import android.content.Context
import java.lang.reflect.Method

/** SDL library initialization */
object SDL {
    @JvmField
    var mContext: Activity? = null

    /** This function should be called first and sets up the native code so it can call into the Java classes. */
    @JvmStatic
    fun setupJNI() {
        SDLActivity.nativeSetupJNI()
        SDLAudioManager.nativeSetupJNI()
        SDLControllerManager.nativeSetupJNI()
    }

    /** This function should be called each time the activity is started. */
    @JvmStatic
    fun initialize() {
        setContext(null)
        SDLActivity.initialize()
        SDLAudioManager.initialize()
        SDLControllerManager.initialize()
    }

    /** This function stores the current activity (SDL or not). */
    @JvmStatic
    fun setContext(context: Activity?) {
        SDLAudioManager.setContext(context)
        mContext = context
    }

    @JvmStatic
    fun getContext(): Activity? = mContext

    @JvmStatic
    @Throws(UnsatisfiedLinkError::class, SecurityException::class, NullPointerException::class)
    fun loadLibrary(libraryName: String?) {
        loadLibrary(libraryName, mContext)
    }

    @JvmStatic
    @Throws(UnsatisfiedLinkError::class, SecurityException::class, NullPointerException::class)
    fun loadLibrary(libraryName: String?, context: Context?) {
        if (libraryName == null) {
            throw NullPointerException("No library name provided.")
        }

        try {
            val classLoader = requireNotNull(context).classLoader
            val relinkClass = classLoader.loadClass("com.getkeepsafe.relinker.ReLinker")
            val relinkListenerClass = classLoader.loadClass("com.getkeepsafe.relinker.ReLinker\$LoadListener")
            val contextClass = classLoader.loadClass("android.content.Context")
            val stringClass = classLoader.loadClass("java.lang.String")
            val forceMethod: Method = relinkClass.getDeclaredMethod("force")
            val relinkInstance = forceMethod.invoke(null)
            val relinkInstanceClass: Class<*> = relinkInstance.javaClass
            val loadMethod = relinkInstanceClass.getDeclaredMethod(
                "loadLibrary",
                contextClass,
                stringClass,
                stringClass,
                relinkListenerClass,
            )
            loadMethod.invoke(relinkInstance, context, libraryName, null, null)
        } catch (error: Throwable) {
            try {
                System.loadLibrary(libraryName)
            } catch (ule: UnsatisfiedLinkError) {
                throw ule
            } catch (se: SecurityException) {
                throw se
            }
        }
    }
}
