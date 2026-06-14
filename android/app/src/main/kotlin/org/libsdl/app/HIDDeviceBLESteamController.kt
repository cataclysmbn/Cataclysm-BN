package org.libsdl.app

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.hardware.usb.UsbDevice
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.util.Arrays
import java.util.LinkedList
import java.util.UUID

internal class HIDDeviceBLESteamController(
    private var manager: HIDDeviceManager?,
    private val device: BluetoothDevice,
) : BluetoothGattCallback(), HIDDevice {
    private val deviceId = requireNotNull(manager).getDeviceIDForIdentifier(getIdentifier())
    private var gatt: BluetoothGatt? = connectGatt()
    private var isRegistered = false
    private var isConnected = false
    private val isChromebook = SDLActivity.isChromebook()
    private var isReconnecting = false
    private var frozen = false
    private val operations = LinkedList<GattOperation>()
    private var currentOperation: GattOperation? = null
    private val handler = Handler(Looper.getMainLooper())

    fun getIdentifier(): String = String.format("SteamController.%s", device.address)

    fun getGatt(): BluetoothGatt? = gatt

    private fun connectGatt(managed: Boolean): BluetoothGatt =
        if (Build.VERSION.SDK_INT >= 23) {
            try {
                device.connectGatt(requireNotNull(manager).getContext(), managed, this, TRANSPORT_LE)
            } catch (_: Exception) {
                device.connectGatt(requireNotNull(manager).getContext(), managed, this)
            }
        } else {
            device.connectGatt(requireNotNull(manager).getContext(), managed, this)
        }

    private fun connectGatt(): BluetoothGatt = connectGatt(false)

    protected fun getConnectionState(): Int {
        val context = requireNotNull(manager).getContext()
        val btManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager?
            ?: return BluetoothProfile.STATE_DISCONNECTED
        return btManager.getConnectionState(device, BluetoothProfile.GATT)
    }

    fun reconnect() {
        if (getConnectionState() != BluetoothProfile.STATE_CONNECTED) {
            gatt?.disconnect()
            gatt = connectGatt()
        }
    }

    protected fun checkConnectionForChromebookIssue() {
        if (!isChromebook) {
            return
        }
        when (getConnectionState()) {
            BluetoothProfile.STATE_CONNECTED -> {
                if (!isConnected) {
                    Log.v(TAG, "Chromebook: We are in a very bad state; the controller shows as connected in the underlying Bluetooth layer, but we never received a callback.  Forcing a reconnect.")
                    isReconnecting = true
                    gatt?.disconnect()
                    gatt = connectGatt(false)
                } else if (!isRegistered()) {
                    if (requireNotNull(gatt).services.size > 0) {
                        Log.v(TAG, "Chromebook: We are connected to a controller, but never got our registration.  Trying to recover.")
                        probeService(this)
                    } else {
                        Log.v(TAG, "Chromebook: We are connected to a controller, but never discovered services.  Trying to recover.")
                        isReconnecting = true
                        gatt?.disconnect()
                        gatt = connectGatt(false)
                    }
                } else {
                    Log.v(TAG, "Chromebook: We are connected, and registered.  Everything's good!")
                    return
                }
            }
            BluetoothProfile.STATE_DISCONNECTED -> {
                Log.v(TAG, "Chromebook: We have either been disconnected, or the Chromebook BtGatt.ContextMap bug has bitten us.  Attempting a disconnect/reconnect, but we may not be able to recover.")
                isReconnecting = true
                gatt?.disconnect()
                gatt = connectGatt(false)
            }
            BluetoothProfile.STATE_CONNECTING -> Log.v(TAG, "Chromebook: We're still trying to connect.  Waiting a bit longer.")
        }
        handler.postDelayed({ checkConnectionForChromebookIssue() }, CHROMEBOOK_CONNECTION_CHECK_INTERVAL.toLong())
    }

    private fun isRegistered(): Boolean = isRegistered
    private fun setRegistered() { isRegistered = true }

    private fun probeService(controller: HIDDeviceBLESteamController): Boolean {
        if (isRegistered()) {
            return true
        }
        if (!isConnected) {
            return false
        }
        Log.v(TAG, "probeService controller=$controller")
        val localGatt = requireNotNull(gatt)
        for (service in localGatt.services) {
            if (service.uuid == steamControllerService) {
                Log.v(TAG, "Found Valve steam controller service ${service.uuid}")
                for (chr in service.characteristics) {
                    if (chr.uuid == inputCharacteristic) {
                        Log.v(TAG, "Found input characteristic")
                        val cccd = chr.getDescriptor(CLIENT_CHARACTERISTIC_CONFIG)
                        if (cccd != null) {
                            enableNotification(chr.uuid)
                        }
                    }
                }
                return true
            }
        }
        if (localGatt.services.isEmpty() && isChromebook && !isReconnecting) {
            Log.e(TAG, "Chromebook: Discovered services were empty; this almost certainly means the BtGatt.ContextMap bug has bitten us.")
            isConnected = false
            isReconnecting = true
            localGatt.disconnect()
            gatt = connectGatt(false)
        }
        return false
    }

    private fun finishCurrentGattOperation() {
        var op: GattOperation? = null
        synchronized(operations) {
            if (currentOperation != null) {
                op = currentOperation
                currentOperation = null
            }
        }
        op?.let {
            if (!it.finish()) {
                operations.addFirst(it)
            }
        }
        executeNextGattOperation()
    }

    private fun executeNextGattOperation() {
        synchronized(operations) {
            if (currentOperation != null || operations.isEmpty()) {
                return
            }
            currentOperation = operations.removeFirst()
        }
        handler.post {
            synchronized(operations) {
                val operation = currentOperation
                if (operation == null) {
                    Log.e(TAG, "Current operation null in executor?")
                    return@post
                }
                operation.run()
            }
        }
    }

    private fun queueGattOperation(op: GattOperation) {
        synchronized(operations) {
            operations.add(op)
        }
        executeNextGattOperation()
    }

    private fun enableNotification(chrUuid: UUID) {
        queueGattOperation(GattOperation.enableNotification(requireNotNull(gatt), chrUuid))
    }

    fun writeCharacteristic(uuid: UUID, value: ByteArray) {
        queueGattOperation(GattOperation.writeCharacteristic(requireNotNull(gatt), uuid, value))
    }

    fun readCharacteristic(uuid: UUID) {
        queueGattOperation(GattOperation.readCharacteristic(requireNotNull(gatt), uuid))
    }

    override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
        isReconnecting = false
        if (newState == BluetoothProfile.STATE_CONNECTED) {
            isConnected = true
            if (!isRegistered()) {
                handler.post { gatt?.discoverServices() }
            }
        } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
            isConnected = false
        }
    }

    override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
        if (status == BluetoothGatt.GATT_SUCCESS) {
            if (gatt.services.isEmpty()) {
                Log.v(TAG, "onServicesDiscovered returned zero services; something has gone horribly wrong down in Android's Bluetooth stack.")
                isReconnecting = true
                isConnected = false
                gatt.disconnect()
                this.gatt = connectGatt(false)
            } else {
                probeService(this)
            }
        }
    }

    override fun onCharacteristicRead(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
        if (characteristic.uuid == reportCharacteristic && !frozen) {
            requireNotNull(manager).HIDDeviceReportResponse(getId(), characteristic.value)
        }
        finishCurrentGattOperation()
    }

    override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
        if (characteristic.uuid == reportCharacteristic && !isRegistered()) {
            Log.v(TAG, "Registering Steam Controller with ID: ${getId()}")
            requireNotNull(manager).HIDDeviceConnected(getId(), getIdentifier(), getVendorId(), getProductId(), getSerialNumber(), getVersion(), getManufacturerName(), getProductName(), 0, 0, 0, 0, true)
            setRegistered()
        }
        finishCurrentGattOperation()
    }

    override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
        if (characteristic.uuid == inputCharacteristic && !frozen) {
            requireNotNull(manager).HIDDeviceInputReport(getId(), characteristic.value)
        }
    }

    override fun onDescriptorRead(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
    }

    override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
        val chr = descriptor.characteristic
        if (chr.uuid == inputCharacteristic) {
            val reportChr = chr.service.getCharacteristic(reportCharacteristic)
            if (reportChr != null) {
                Log.v(TAG, "Writing report characteristic to enter valve mode")
                reportChr.value = enterValveMode
                gatt.writeCharacteristic(reportChr)
            }
        }
        finishCurrentGattOperation()
    }

    override fun onReliableWriteCompleted(gatt: BluetoothGatt, status: Int) {
    }

    override fun onReadRemoteRssi(gatt: BluetoothGatt, rssi: Int, status: Int) {
    }

    override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
    }

    override fun getId(): Int = deviceId
    override fun getVendorId(): Int = 0x28DE
    override fun getProductId(): Int = 0x1106
    override fun getSerialNumber(): String = "12345"
    override fun getVersion(): Int = 0
    override fun getManufacturerName(): String = "Valve Corporation"
    override fun getProductName(): String = "Steam Controller"
    override fun getDevice(): UsbDevice? = null
    override fun open(): Boolean = true

    override fun writeReport(report: ByteArray, feature: Boolean): Int {
        if (!isRegistered()) {
            Log.e(TAG, "Attempted writeReport before Steam Controller is registered!")
            if (isConnected) {
                probeService(this)
            }
            return -1
        }
        if (feature) {
            val actualReport = Arrays.copyOfRange(report, 1, report.size - 1)
            writeCharacteristic(reportCharacteristic, actualReport)
        } else {
            writeCharacteristic(reportCharacteristic, report)
        }
        return report.size
    }

    override fun readReport(report: ByteArray, feature: Boolean): Boolean {
        if (!isRegistered()) {
            Log.e(TAG, "Attempted readReport before Steam Controller is registered!")
            if (isConnected) {
                probeService(this)
            }
            return false
        }
        return if (feature) {
            readCharacteristic(reportCharacteristic)
            true
        } else {
            false
        }
    }

    override fun close() {
    }

    override fun setFrozen(frozen: Boolean) {
        this.frozen = frozen
    }

    override fun shutdown() {
        close()
        gatt?.let {
            it.disconnect()
            it.close()
            gatt = null
        }
        manager = null
        isRegistered = false
        isConnected = false
        operations.clear()
    }

    internal class GattOperation private constructor(
        private val gatt: BluetoothGatt,
        private val op: Operation,
        private val uuid: UUID,
        private val value: ByteArray? = null,
    ) {
        private enum class Operation { CHR_READ, CHR_WRITE, ENABLE_NOTIFICATION }
        private var result = true

        fun run() {
            val chr: BluetoothGattCharacteristic?
            when (op) {
                Operation.CHR_READ -> {
                    chr = getCharacteristic(uuid)
                    if (chr == null || !gatt.readCharacteristic(chr)) {
                        Log.e(TAG, "Unable to read characteristic $uuid")
                        result = false
                    } else {
                        result = true
                    }
                }
                Operation.CHR_WRITE -> {
                    chr = getCharacteristic(uuid)
                    if (chr == null) {
                        Log.e(TAG, "Unable to write characteristic $uuid")
                        result = false
                        return
                    }
                    chr.value = value
                    if (!gatt.writeCharacteristic(chr)) {
                        Log.e(TAG, "Unable to write characteristic $uuid")
                        result = false
                    } else {
                        result = true
                    }
                }
                Operation.ENABLE_NOTIFICATION -> {
                    chr = getCharacteristic(uuid)
                    if (chr != null) {
                        val cccd = chr.getDescriptor(CLIENT_CHARACTERISTIC_CONFIG)
                        if (cccd != null) {
                            val descriptorValue = if ((chr.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY) == BluetoothGattCharacteristic.PROPERTY_NOTIFY) {
                                BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            } else if ((chr.properties and BluetoothGattCharacteristic.PROPERTY_INDICATE) == BluetoothGattCharacteristic.PROPERTY_INDICATE) {
                                BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
                            } else {
                                Log.e(TAG, "Unable to start notifications on input characteristic")
                                result = false
                                return
                            }
                            gatt.setCharacteristicNotification(chr, true)
                            cccd.value = descriptorValue
                            if (!gatt.writeDescriptor(cccd)) {
                                Log.e(TAG, "Unable to write descriptor $uuid")
                                result = false
                                return
                            }
                            result = true
                        }
                    }
                }
            }
        }

        fun finish(): Boolean = result

        private fun getCharacteristic(uuid: UUID): BluetoothGattCharacteristic? {
            val valveService: BluetoothGattService = gatt.getService(steamControllerService) ?: return null
            return valveService.getCharacteristic(uuid)
        }

        companion object {
            fun readCharacteristic(gatt: BluetoothGatt, uuid: UUID): GattOperation = GattOperation(gatt, Operation.CHR_READ, uuid)
            fun writeCharacteristic(gatt: BluetoothGatt, uuid: UUID, value: ByteArray): GattOperation = GattOperation(gatt, Operation.CHR_WRITE, uuid, value)
            fun enableNotification(gatt: BluetoothGatt, uuid: UUID): GattOperation = GattOperation(gatt, Operation.ENABLE_NOTIFICATION, uuid)
        }
    }

    companion object {
        private const val TAG = "hidapi"
        private const val TRANSPORT_LE = 2
        private const val CHROMEBOOK_CONNECTION_CHECK_INTERVAL = 10000
        private val CLIENT_CHARACTERISTIC_CONFIG: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        @JvmField val steamControllerService: UUID = UUID.fromString("100F6C32-1735-4313-B402-38567131E5F3")
        @JvmField val inputCharacteristic: UUID = UUID.fromString("100F6C33-1735-4313-B402-38567131E5F3")
        @JvmField val reportCharacteristic: UUID = UUID.fromString("100F6C34-1735-4313-B402-38567131E5F3")
        private val enterValveMode = byteArrayOf(0xC0.toByte(), 0x87.toByte(), 0x03, 0x08, 0x07, 0x00)
    }
}
