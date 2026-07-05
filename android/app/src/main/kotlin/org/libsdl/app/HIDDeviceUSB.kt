package org.libsdl.app

import android.hardware.usb.UsbConstants
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.util.Log
import java.util.Arrays
import java.util.Locale

internal class HIDDeviceUSB(
    private var manager: HIDDeviceManager?,
    private val usbDevice: UsbDevice,
    private val interfaceIndex: Int,
) : HIDDevice {
    private val interfaceId = usbDevice.getInterface(interfaceIndex).id
    private val deviceId = requireNotNull(manager).getDeviceIDForIdentifier(getIdentifier())
    private var connection: UsbDeviceConnection? = null
    private var inputEndpoint: UsbEndpoint? = null
    private var outputEndpoint: UsbEndpoint? = null
    private var inputThread: InputThread? = null
    private var running = false
    private var frozen = false

    fun getIdentifier(): String = String.format(
        Locale.ENGLISH,
        "%s/%x/%x/%d",
        usbDevice.deviceName,
        usbDevice.vendorId,
        usbDevice.productId,
        interfaceIndex,
    )

    override fun getId(): Int = deviceId
    override fun getVendorId(): Int = usbDevice.vendorId
    override fun getProductId(): Int = usbDevice.productId

    override fun getSerialNumber(): String {
        val result = try {
            usbDevice.serialNumber
        } catch (_: SecurityException) {
            null
        }
        return result ?: ""
    }

    override fun getVersion(): Int = 0
    override fun getManufacturerName(): String = usbDevice.manufacturerName ?: String.format("%x", getVendorId())
    override fun getProductName(): String = usbDevice.productName ?: String.format("%x", getProductId())
    override fun getDevice(): UsbDevice = usbDevice

    private fun getDeviceName(): String = "${getManufacturerName()} ${getProductName()}(0x${String.format("%x", getVendorId())}/0x${String.format("%x", getProductId())})"

    override fun open(): Boolean {
        val manager = requireNotNull(manager)
        connection = manager.getUSBManager().openDevice(usbDevice)
        val connection = connection
        if (connection == null) {
            Log.w(TAG, "Unable to open USB device ${getDeviceName()}")
            return false
        }

        val iface = usbDevice.getInterface(interfaceIndex)
        if (!connection.claimInterface(iface, true)) {
            Log.w(TAG, "Failed to claim interfaces on USB device ${getDeviceName()}")
            close()
            return false
        }

        for (j in 0 until iface.endpointCount) {
            val endpoint = iface.getEndpoint(j)
            when (endpoint.direction) {
                UsbConstants.USB_DIR_IN -> if (inputEndpoint == null) inputEndpoint = endpoint
                UsbConstants.USB_DIR_OUT -> if (outputEndpoint == null) outputEndpoint = endpoint
            }
        }

        if (inputEndpoint == null || outputEndpoint == null) {
            Log.w(TAG, "Missing required endpoint on USB device ${getDeviceName()}")
            close()
            return false
        }

        running = true
        inputThread = InputThread().also { it.start() }
        return true
    }

    override fun writeReport(report: ByteArray, feature: Boolean): Int {
        val connection = connection
        if (connection == null) {
            Log.w(TAG, "writeReport() called with no device connection")
            return -1
        }
        if (feature) {
            var offset = 0
            var length = report.size
            var skippedReportId = false
            val reportNumber = report[0]
            if (reportNumber == 0.toByte()) {
                ++offset
                --length
                skippedReportId = true
            }
            val result = connection.controlTransfer(
                UsbConstants.USB_TYPE_CLASS or 0x01 or UsbConstants.USB_DIR_OUT,
                0x09,
                (3 shl 8) or reportNumber.toInt(),
                interfaceId,
                report,
                offset,
                length,
                1000,
            )
            if (result < 0) {
                Log.w(TAG, "writeFeatureReport() returned $result on device ${getDeviceName()}")
                return -1
            }
            if (skippedReportId) {
                ++length
            }
            return length
        }
        val result = connection.bulkTransfer(outputEndpoint, report, report.size, 1000)
        if (result != report.size) {
            Log.w(TAG, "writeOutputReport() returned $result on device ${getDeviceName()}")
        }
        return result
    }

    override fun readReport(report: ByteArray, feature: Boolean): Boolean {
        val connection = connection
        if (connection == null) {
            Log.w(TAG, "readReport() called with no device connection")
            return false
        }
        var offset = 0
        var length = report.size
        var skippedReportId = false
        val reportNumber = report[0]
        if (reportNumber == 0.toByte()) {
            ++offset
            --length
            skippedReportId = true
        }
        var result = connection.controlTransfer(
            UsbConstants.USB_TYPE_CLASS or 0x01 or UsbConstants.USB_DIR_IN,
            0x01,
            ((if (feature) 3 else 1) shl 8) or reportNumber.toInt(),
            interfaceId,
            report,
            offset,
            length,
            1000,
        )
        if (result < 0) {
            Log.w(TAG, "getFeatureReport() returned $result on device ${getDeviceName()}")
            return false
        }
        if (skippedReportId) {
            ++result
            ++length
        }
        val data = if (result == length) report else Arrays.copyOfRange(report, 0, result)
        requireNotNull(manager).HIDDeviceReportResponse(deviceId, data)
        return true
    }

    override fun close() {
        running = false
        inputThread?.let { thread ->
            while (thread.isAlive) {
                thread.interrupt()
                try {
                    thread.join()
                } catch (_: InterruptedException) {
                }
            }
            inputThread = null
        }
        connection?.let { conn ->
            conn.releaseInterface(usbDevice.getInterface(interfaceIndex))
            conn.close()
            connection = null
        }
    }

    override fun shutdown() {
        close()
        manager = null
    }

    override fun setFrozen(frozen: Boolean) {
        this.frozen = frozen
    }

    private inner class InputThread : Thread() {
        override fun run() {
            val endpoint = requireNotNull(inputEndpoint)
            val packetSize = endpoint.maxPacketSize
            val packet = ByteArray(packetSize)
            while (running) {
                val result = try {
                    requireNotNull(connection).bulkTransfer(endpoint, packet, packetSize, 1000)
                } catch (error: Exception) {
                    Log.v(TAG, "Exception in UsbDeviceConnection bulktransfer: $error")
                    break
                }
                if (result > 0) {
                    if (!frozen) {
                        requireNotNull(manager).HIDDeviceInputReport(deviceId, Arrays.copyOfRange(packet, 0, result))
                    }
                }
            }
        }
    }

    companion object {
        private const val TAG = "hidapi"
    }
}
