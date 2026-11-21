package com.capacitorjs.plugins.filesystem

import com.getcapacitor.JSObject
import com.getcapacitor.PluginCall

/**
 * Extension function to return a successful plugin result
 * @param result JSOObject with the JSON content to return
 * @param keepCallback boolean to determine if callback should be kept for future calls or not
 */
internal fun PluginCall.sendSuccess(result: JSObject? = null, keepCallback: Boolean = false) {
    this.setKeepAlive(keepCallback)
    if (result != null) {
        this.resolve(result)
    } else {
        this.resolve()
    }
}

/**
 * Extension function to return a unsuccessful plugin result
 * @param error error class representing the error to return, containing a code and message
 */
internal fun PluginCall.sendError(error: FilesystemErrors.ErrorInfo) =
    this.reject(error.message, error.code)