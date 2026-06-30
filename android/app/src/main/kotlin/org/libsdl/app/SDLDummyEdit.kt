package org.libsdl.app

import android.content.Context
import android.text.InputType
import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection

/**
 * Fake invisible editor view that receives input and defines the pan&scan region.
 */
internal class SDLDummyEdit(context: Context) : View(context), View.OnKeyListener {
    var ic: InputConnection? = null
    private var inputTypeValue: Int = InputType.TYPE_CLASS_TEXT

    init {
        isFocusableInTouchMode = true
        isFocusable = true
        setOnKeyListener(this)
    }

    fun setInputType(inputType: Int) {
        inputTypeValue = inputType
    }

    override fun onCheckIsTextEditor(): Boolean = true

    override fun onKey(v: View, keyCode: Int, event: KeyEvent): Boolean =
        SDLActivity.handleKeyEvent(v, keyCode, event, ic)

    override fun onKeyPreIme(keyCode: Int, event: KeyEvent): Boolean {
        if (event.action == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
            if (SDLActivity.mTextEdit != null && SDLActivity.mTextEdit.visibility == VISIBLE) {
                SDLActivity.onNativeKeyboardFocusLost()
            }
        }
        return super.onKeyPreIme(keyCode, event)
    }

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        val connection = SDLInputConnection(this, true)
        ic = connection
        outAttrs.inputType = inputTypeValue
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_FLAG_NO_FULLSCREEN
        return connection
    }
}
