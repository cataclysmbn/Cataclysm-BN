package org.libsdl.app

import android.content.Context
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Build
import android.util.Log

internal object SDLAudioManager {
    private const val TAG = "SDLAudio"

    @JvmField
    var mContext: Context? = null

    private var mAudioDeviceCallback: AudioDeviceCallback? = null

    @JvmStatic
    fun initialize() {
        mAudioDeviceCallback = null
        if (Build.VERSION.SDK_INT >= 24) {
            mAudioDeviceCallback = object : AudioDeviceCallback() {
                override fun onAudioDevicesAdded(addedDevices: Array<AudioDeviceInfo>) {
                    for (deviceInfo in addedDevices) {
                        nativeAddAudioDevice(deviceInfo.isSink, deviceInfo.productName.toString(), deviceInfo.id)
                    }
                }

                override fun onAudioDevicesRemoved(removedDevices: Array<AudioDeviceInfo>) {
                    for (deviceInfo in removedDevices) {
                        nativeRemoveAudioDevice(deviceInfo.isSink, deviceInfo.id)
                    }
                }
            }
        }
    }

    @JvmStatic
    fun setContext(context: Context?) {
        mContext = context
    }

    @JvmStatic
    fun release(context: Context?) {
        // no-op atm
    }

    @JvmStatic
    private fun getInputAudioDeviceInfo(deviceId: Int): AudioDeviceInfo? {
        if (Build.VERSION.SDK_INT >= 24) {
            val audioManager = requireNotNull(mContext).getSystemService(Context.AUDIO_SERVICE) as AudioManager
            for (deviceInfo in audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)) {
                if (deviceInfo.id == deviceId) {
                    return deviceInfo
                }
            }
        }
        return null
    }

    @JvmStatic
    private fun getPlaybackAudioDeviceInfo(deviceId: Int): AudioDeviceInfo? {
        if (Build.VERSION.SDK_INT >= 24) {
            val audioManager = requireNotNull(mContext).getSystemService(Context.AUDIO_SERVICE) as AudioManager
            for (deviceInfo in audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
                if (deviceInfo.id == deviceId) {
                    return deviceInfo
                }
            }
        }
        return null
    }

    @JvmStatic
    fun registerAudioDeviceCallback() {
        if (Build.VERSION.SDK_INT >= 24) {
            val audioManager = requireNotNull(mContext).getSystemService(Context.AUDIO_SERVICE) as AudioManager
            for (device in audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
                if (device.type == AudioDeviceInfo.TYPE_TELEPHONY) {
                    continue
                }
                nativeAddAudioDevice(device.isSink, device.productName.toString(), device.id)
            }
            for (device in audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)) {
                nativeAddAudioDevice(device.isSink, device.productName.toString(), device.id)
            }
            audioManager.registerAudioDeviceCallback(mAudioDeviceCallback, null)
        }
    }

    @JvmStatic
    fun unregisterAudioDeviceCallback() {
        if (Build.VERSION.SDK_INT >= 24) {
            val audioManager = requireNotNull(mContext).getSystemService(Context.AUDIO_SERVICE) as AudioManager
            audioManager.unregisterAudioDeviceCallback(mAudioDeviceCallback)
        }
    }

    /** This method is called by SDL using JNI. */
    @JvmStatic
    fun audioSetThreadPriority(recording: Boolean, device_id: Int) {
        try {
            Thread.currentThread().name = if (recording) "SDLAudioC$device_id" else "SDLAudioP$device_id"
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO)
        } catch (error: Exception) {
            Log.v(TAG, "modify thread properties failed $error")
        }
    }

    @JvmStatic external fun nativeSetupJNI()
    @JvmStatic external fun nativeRemoveAudioDevice(recording: Boolean, deviceId: Int)
    @JvmStatic external fun nativeAddAudioDevice(recording: Boolean, name: String, deviceId: Int)
}
