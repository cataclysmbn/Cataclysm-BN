package org.libsdl.app

import android.Manifest
import android.app.PendingIntent
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.hardware.usb.UsbConstants
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log

class HIDDeviceManager private constructor(private val context: Context) {
    private val devicesById = HashMap<Int, HIDDevice>()
    private val bluetoothDevices = HashMap<BluetoothDevice, HIDDeviceBLESteamController>()
    private var nextDeviceId = 0
    private val sharedPreferences: SharedPreferences = context.getSharedPreferences("hidapi", Context.MODE_PRIVATE)
    private val isChromebook = SDLActivity.isChromebook()
    private var usbManager: UsbManager? = null
    private var handler: Handler? = null
    private var bluetoothManager: BluetoothManager? = null
    private var lastBluetoothDevices: List<BluetoothDevice> = emptyList()

    private val usbBroadcast: BroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val action = intent.action
            val usbDevice = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
            when (action) {
                UsbManager.ACTION_USB_DEVICE_ATTACHED -> handleUsbDeviceAttached(usbDevice)
                UsbManager.ACTION_USB_DEVICE_DETACHED -> handleUsbDeviceDetached(usbDevice)
                ACTION_USB_PERMISSION -> handleUsbDevicePermission(
                    usbDevice,
                    intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false),
                )
            }
        }
    }

    private val bluetoothBroadcast: BroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val action = intent.action
            val device = intent.getParcelableExtra<BluetoothDevice>(BluetoothDevice.EXTRA_DEVICE)
            if (action == BluetoothDevice.ACTION_ACL_CONNECTED) {
                Log.d(TAG, "Bluetooth device connected: $device")
                if (isSteamController(device)) {
                    connectBluetoothDevice(device!!)
                }
            }
            if (action == BluetoothDevice.ACTION_ACL_DISCONNECTED) {
                Log.d(TAG, "Bluetooth device disconnected: $device")
                if (device != null) {
                    disconnectBluetoothDevice(device)
                }
            }
        }
    }

    init {
        HIDDeviceRegisterCallback()
        nextDeviceId = sharedPreferences.getInt("next_device_id", 0)
    }

    fun getContext(): Context = context

    fun getDeviceIDForIdentifier(identifier: String): Int {
        val editor = sharedPreferences.edit()
        var result = sharedPreferences.getInt(identifier, 0)
        if (result == 0) {
            result = nextDeviceId++
            editor.putInt("next_device_id", nextDeviceId)
        }
        editor.putInt(identifier, result)
        editor.apply()
        return result
    }

    private fun initializeUSB() {
        usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager? ?: return
        val filter = IntentFilter().apply {
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
            addAction(ACTION_USB_PERMISSION)
        }
        if (Build.VERSION.SDK_INT >= 33) {
            context.registerReceiver(usbBroadcast, filter, Context.RECEIVER_EXPORTED)
        } else {
            context.registerReceiver(usbBroadcast, filter)
        }
        for (usbDevice in getUSBManager().deviceList.values) {
            handleUsbDeviceAttached(usbDevice)
        }
    }

    fun getUSBManager(): UsbManager = requireNotNull(usbManager)

    private fun shutdownUSB() {
        try {
            context.unregisterReceiver(usbBroadcast)
        } catch (_: Exception) {
        }
    }

    private fun isHIDDeviceInterface(usbDevice: UsbDevice, usbInterface: UsbInterface): Boolean =
        usbInterface.interfaceClass == UsbConstants.USB_CLASS_HID ||
            isXbox360Controller(usbDevice, usbInterface) ||
            isXboxOneController(usbDevice, usbInterface)

    private fun isXbox360Controller(usbDevice: UsbDevice, usbInterface: UsbInterface): Boolean {
        val supportedVendors = intArrayOf(
            0x0079, 0x044f, 0x045e, 0x046d, 0x056e, 0x06a3, 0x0738, 0x07ff,
            0x0e6f, 0x0f0d, 0x1038, 0x11c9, 0x12ab, 0x1430, 0x146b, 0x1532,
            0x15e4, 0x162e, 0x1689, 0x1949, 0x1bad, 0x20d6, 0x24c6, 0x2c22,
            0x2dc8, 0x9886,
        )
        return usbInterface.interfaceClass == UsbConstants.USB_CLASS_VENDOR_SPEC &&
            usbInterface.interfaceSubclass == 93 &&
            (usbInterface.interfaceProtocol == 1 || usbInterface.interfaceProtocol == 129) &&
            supportedVendors.any { usbDevice.vendorId == it }
    }

    private fun isXboxOneController(usbDevice: UsbDevice, usbInterface: UsbInterface): Boolean {
        val supportedVendors = intArrayOf(
            0x03f0, 0x044f, 0x045e, 0x0738, 0x0b05, 0x0e6f, 0x0f0d, 0x10f5,
            0x1532, 0x20d6, 0x24c6, 0x294b, 0x2dc8, 0x2e24, 0x2e95, 0x3285,
            0x3537, 0x366c,
        )
        return usbInterface.id == 0 &&
            usbInterface.interfaceClass == UsbConstants.USB_CLASS_VENDOR_SPEC &&
            usbInterface.interfaceSubclass == 71 &&
            usbInterface.interfaceProtocol == 208 &&
            supportedVendors.any { usbDevice.vendorId == it }
    }

    private fun handleUsbDeviceAttached(usbDevice: UsbDevice?) {
        if (usbDevice != null) {
            connectHIDDeviceUSB(usbDevice)
        }
    }

    private fun handleUsbDeviceDetached(usbDevice: UsbDevice?) {
        val devices = devicesById.values.filter { usbDevice == it.getDevice() }.map { it.getId() }
        for (id in devices) {
            val device = devicesById.remove(id) ?: continue
            device.shutdown()
            HIDDeviceDisconnected(id)
        }
    }

    private fun handleUsbDevicePermission(usbDevice: UsbDevice?, permissionGranted: Boolean) {
        for (device in devicesById.values) {
            if (usbDevice == device.getDevice()) {
                val opened = permissionGranted && device.open()
                HIDDeviceOpenResult(device.getId(), opened)
            }
        }
    }

    private fun connectHIDDeviceUSB(usbDevice: UsbDevice) {
        synchronized(this) {
            var interfaceMask = 0
            for (interfaceIndex in 0 until usbDevice.interfaceCount) {
                val usbInterface = usbDevice.getInterface(interfaceIndex)
                if (isHIDDeviceInterface(usbDevice, usbInterface)) {
                    val interfaceId = usbInterface.id
                    if ((interfaceMask and (1 shl interfaceId)) != 0) {
                        continue
                    }
                    interfaceMask = interfaceMask or (1 shl interfaceId)
                    val device = HIDDeviceUSB(this, usbDevice, interfaceIndex)
                    val id = device.getId()
                    devicesById[id] = device
                    HIDDeviceConnected(
                        id,
                        device.getIdentifier(),
                        device.getVendorId(),
                        device.getProductId(),
                        device.getSerialNumber(),
                        device.getVersion(),
                        device.getManufacturerName(),
                        device.getProductName(),
                        usbInterface.id,
                        usbInterface.interfaceClass,
                        usbInterface.interfaceSubclass,
                        usbInterface.interfaceProtocol,
                        false,
                    )
                }
            }
        }
    }

    private fun initializeBluetooth() {
        Log.d(TAG, "Initializing Bluetooth")
        if (Build.VERSION.SDK_INT >= 31 && context.packageManager.checkPermission(Manifest.permission.BLUETOOTH_CONNECT, context.packageName) != PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG, "Couldn't initialize Bluetooth, missing android.permission.BLUETOOTH_CONNECT")
            return
        }
        if (Build.VERSION.SDK_INT <= 30 && context.packageManager.checkPermission(Manifest.permission.BLUETOOTH, context.packageName) != PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG, "Couldn't initialize Bluetooth, missing android.permission.BLUETOOTH")
            return
        }
        if (!context.packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            Log.d(TAG, "Couldn't initialize Bluetooth, this version of Android does not support Bluetooth LE")
            return
        }
        bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager? ?: return
        val adapter = bluetoothManager?.adapter ?: return
        for (device in adapter.bondedDevices) {
            Log.d(TAG, "Bluetooth device available: $device")
            if (isSteamController(device)) {
                connectBluetoothDevice(device)
            }
        }
        val filter = IntentFilter().apply {
            addAction(BluetoothDevice.ACTION_ACL_CONNECTED)
            addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED)
        }
        if (Build.VERSION.SDK_INT >= 33) {
            context.registerReceiver(bluetoothBroadcast, filter, Context.RECEIVER_EXPORTED)
        } else {
            context.registerReceiver(bluetoothBroadcast, filter)
        }
        if (isChromebook) {
            handler = Handler(Looper.getMainLooper())
            lastBluetoothDevices = ArrayList()
        }
    }

    private fun shutdownBluetooth() {
        try {
            context.unregisterReceiver(bluetoothBroadcast)
        } catch (_: Exception) {
        }
    }

    fun chromebookConnectionHandler() {
        if (!isChromebook) {
            return
        }
        val currentConnected = requireNotNull(bluetoothManager).getConnectedDevices(BluetoothProfile.GATT)
        val disconnected = lastBluetoothDevices.filter { it !in currentConnected }
        val connected = currentConnected.filter { it !in lastBluetoothDevices }
        lastBluetoothDevices = currentConnected
        disconnected.forEach(::disconnectBluetoothDevice)
        connected.forEach(::connectBluetoothDevice)
        handler?.postDelayed({ chromebookConnectionHandler() }, 10000)
    }

    fun connectBluetoothDevice(bluetoothDevice: BluetoothDevice): Boolean {
        Log.v(TAG, "connectBluetoothDevice device=$bluetoothDevice")
        synchronized(this) {
            bluetoothDevices[bluetoothDevice]?.let { device ->
                Log.v(TAG, "Steam controller with address $bluetoothDevice already exists, attempting reconnect")
                device.reconnect()
                return false
            }
            val device = HIDDeviceBLESteamController(this, bluetoothDevice)
            val id = device.getId()
            bluetoothDevices[bluetoothDevice] = device
            devicesById[id] = device
        }
        return true
    }

    fun disconnectBluetoothDevice(bluetoothDevice: BluetoothDevice) {
        synchronized(this) {
            val device = bluetoothDevices[bluetoothDevice] ?: return
            val id = device.getId()
            bluetoothDevices.remove(bluetoothDevice)
            devicesById.remove(id)
            device.shutdown()
            HIDDeviceDisconnected(id)
        }
    }

    fun isSteamController(bluetoothDevice: BluetoothDevice?): Boolean =
        bluetoothDevice?.name != null &&
            bluetoothDevice.name == "SteamController" &&
            (bluetoothDevice.type and BluetoothDevice.DEVICE_TYPE_LE) != 0

    private fun close() {
        shutdownUSB()
        shutdownBluetooth()
        synchronized(this) {
            for (device in devicesById.values) {
                device.shutdown()
            }
            devicesById.clear()
            bluetoothDevices.clear()
            HIDDeviceReleaseCallback()
        }
    }

    fun setFrozen(frozen: Boolean) {
        synchronized(this) {
            for (device in devicesById.values) {
                device.setFrozen(frozen)
            }
        }
    }

    private fun getDevice(id: Int): HIDDevice? = synchronized(this) {
        val result = devicesById[id]
        if (result == null) {
            Log.v(TAG, "No device for id: $id")
            Log.v(TAG, "Available devices: ${devicesById.keys}")
        }
        result
    }

    fun initialize(usb: Boolean, bluetooth: Boolean): Boolean {
        Log.v(TAG, "initialize($usb, $bluetooth)")
        if (usb) {
            initializeUSB()
        }
        if (bluetooth) {
            initializeBluetooth()
        }
        return true
    }

    fun openDevice(deviceID: Int): Boolean {
        Log.v(TAG, "openDevice deviceID=$deviceID")
        val device = getDevice(deviceID)
        if (device == null) {
            HIDDeviceDisconnected(deviceID)
            return false
        }
        val usbDevice = device.getDevice()
        val manager = usbManager
        if (usbDevice != null && manager != null && !manager.hasPermission(usbDevice)) {
            HIDDeviceOpenPending(deviceID)
            try {
                val flags = if (Build.VERSION.SDK_INT >= 31) PendingIntent.FLAG_MUTABLE else 0
                val intent = Intent(ACTION_USB_PERMISSION).setPackage(context.packageName)
                manager.requestPermission(usbDevice, PendingIntent.getBroadcast(context, 0, intent, flags))
            } catch (error: Exception) {
                Log.v(TAG, "Couldn't request permission for USB device $usbDevice")
                HIDDeviceOpenResult(deviceID, false)
            }
            return false
        }
        return try {
            device.open()
        } catch (error: Exception) {
            Log.e(TAG, "Got exception: ${Log.getStackTraceString(error)}")
            false
        }
    }

    fun writeReport(deviceID: Int, report: ByteArray, feature: Boolean): Int = try {
        val device = getDevice(deviceID)
        if (device == null) {
            HIDDeviceDisconnected(deviceID)
            -1
        } else {
            device.writeReport(report, feature)
        }
    } catch (error: Exception) {
        Log.e(TAG, "Got exception: ${Log.getStackTraceString(error)}")
        -1
    }

    fun readReport(deviceID: Int, report: ByteArray, feature: Boolean): Boolean = try {
        val device = getDevice(deviceID)
        if (device == null) {
            HIDDeviceDisconnected(deviceID)
            false
        } else {
            device.readReport(report, feature)
        }
    } catch (error: Exception) {
        Log.e(TAG, "Got exception: ${Log.getStackTraceString(error)}")
        false
    }

    fun closeDevice(deviceID: Int) {
        try {
            Log.v(TAG, "closeDevice deviceID=$deviceID")
            val device = getDevice(deviceID)
            if (device == null) {
                HIDDeviceDisconnected(deviceID)
                return
            }
            device.close()
        } catch (error: Exception) {
            Log.e(TAG, "Got exception: ${Log.getStackTraceString(error)}")
        }
    }

    private external fun HIDDeviceRegisterCallback()
    private external fun HIDDeviceReleaseCallback()
    external fun HIDDeviceConnected(deviceID: Int, identifier: String, vendorId: Int, productId: Int, serial_number: String?, release_number: Int, manufacturer_string: String?, product_string: String?, interface_number: Int, interface_class: Int, interface_subclass: Int, interface_protocol: Int, bBluetooth: Boolean)
    external fun HIDDeviceOpenPending(deviceID: Int)
    external fun HIDDeviceOpenResult(deviceID: Int, opened: Boolean)
    external fun HIDDeviceDisconnected(deviceID: Int)
    external fun HIDDeviceInputReport(deviceID: Int, report: ByteArray)
    external fun HIDDeviceReportResponse(deviceID: Int, report: ByteArray)

    companion object {
        private const val TAG = "hidapi"
        private const val ACTION_USB_PERMISSION = "org.libsdl.app.USB_PERMISSION"
        private var sManager: HIDDeviceManager? = null
        private var sManagerRefCount = 0

        @JvmStatic
        fun acquire(context: Context): HIDDeviceManager {
            if (sManagerRefCount == 0) {
                sManager = HIDDeviceManager(context)
            }
            ++sManagerRefCount
            return requireNotNull(sManager)
        }

        @JvmStatic
        fun release(manager: HIDDeviceManager?) {
            if (manager == sManager) {
                --sManagerRefCount
                if (sManagerRefCount == 0) {
                    sManager?.close()
                    sManager = null
                }
            }
        }
    }
}
