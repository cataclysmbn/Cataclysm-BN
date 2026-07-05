package org.libsdl.app

import android.hardware.usb.UsbDevice

internal interface HIDDevice {
    fun getId(): Int
    fun getVendorId(): Int
    fun getProductId(): Int
    fun getSerialNumber(): String?
    fun getVersion(): Int
    fun getManufacturerName(): String?
    fun getProductName(): String?
    fun getDevice(): UsbDevice?
    fun open(): Boolean
    fun writeReport(report: ByteArray, feature: Boolean): Int
    fun readReport(report: ByteArray, feature: Boolean): Boolean
    fun setFrozen(frozen: Boolean)
    fun close()
    fun shutdown()
}
