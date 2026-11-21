package com.capacitorjs.plugins.filesystem

import android.content.Context
import android.net.Uri
import android.os.Environment
import android.os.Handler
import android.os.Looper
import com.getcapacitor.Bridge
import com.getcapacitor.JSObject
import com.getcapacitor.PluginCall
import com.getcapacitor.plugin.util.HttpRequestHandler.HttpURLConnectionBuilder
import com.getcapacitor.plugin.util.HttpRequestHandler.ProgressEmitter
import org.json.JSONException
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.net.URISyntaxException
import java.net.URL
import kotlin.concurrent.thread

class LegacyFilesystemImplementation internal constructor(private val context: Context) {
    fun downloadFile(
        call: PluginCall,
        bridge: Bridge,
        emitter: ProgressEmitter?,
        callback: FilesystemDownloadCallback
    ) {
        val urlString = call.getString("url", "")
        val handler = Handler(Looper.getMainLooper())

        thread {
            try {
                val result =
                    doDownloadInBackground(urlString, call, bridge, emitter)
                handler.post { callback.onSuccess(result) }
            } catch (error: Exception) {
                handler.post { callback.onError(error) }
            }
        }
    }

    /**
     * True if the given directory string is a public storage directory, which is accessible by the user or other apps.
     * @param directory the directory string.
     */
    fun isPublicDirectory(directory: String?): Boolean {
        return "DOCUMENTS" == directory || "EXTERNAL_STORAGE" == directory
    }

    private fun getDirectory(directory: String): File? {
        val c = this.context
        when (directory) {
            "DOCUMENTS" -> return Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)
            "DATA", "LIBRARY" -> return c.filesDir
            "CACHE" -> return c.cacheDir
            "EXTERNAL" -> return c.getExternalFilesDir(null)
            "EXTERNAL_STORAGE" -> return Environment.getExternalStorageDirectory()
        }
        return null
    }

    private fun getFileObject(path: String, directory: String?): File? {
        if (directory == null) {
            val u = Uri.parse(path)
            if (u.scheme == null || u.scheme == "file") {
                return File(u.path)
            }
        }

        val androidDirectory = this.getDirectory(directory!!)

        if (androidDirectory == null) {
            return null
        } else {
            if (!androidDirectory.exists()) {
                androidDirectory.mkdir()
            }
        }

        return File(androidDirectory, path)
    }

    @Throws(IOException::class, URISyntaxException::class, JSONException::class)
    private fun doDownloadInBackground(
        urlString: String?,
        call: PluginCall,
        bridge: Bridge,
        emitter: ProgressEmitter?
    ): JSObject {
        val headers = call.getObject("headers", JSObject())
        val params = call.getObject("params", JSObject())
        val connectTimeout = call.getInt("connectTimeout")
        val readTimeout = call.getInt("readTimeout")
        val disableRedirects = call.getBoolean("disableRedirects") ?: false
        val shouldEncode = call.getBoolean("shouldEncodeUrlParams") ?: true
        val progress = call.getBoolean("progress") ?: false

        val method = call.getString("method")?.uppercase() ?: "GET"
        val path = call.getString("path")!!
        val directory = call.getString("directory", Environment.DIRECTORY_DOWNLOADS)

        val url = URL(urlString)
        val file = getFileObject(path, directory)

        val connectionBuilder = HttpURLConnectionBuilder()
            .setUrl(url)
            .setMethod(method)
            .setHeaders(headers)
            .setUrlParams(params, shouldEncode)
            .setConnectTimeout(connectTimeout)
            .setReadTimeout(readTimeout)
            .setDisableRedirects(disableRedirects)
            .openConnection()

        val connection = connectionBuilder.build()

        connection.setSSLSocketFactory(bridge)

        val connectionInputStream = connection.inputStream
        val fileOutputStream = FileOutputStream(file, false)

        val contentLength = connection.getHeaderField("content-length")
        var bytes = 0
        var maxBytes = 0

        try {
            maxBytes = contentLength?.toInt() ?: 0
        } catch (ignored: NumberFormatException) {
        }

        val buffer = ByteArray(1024)
        var len: Int

        // Throttle emitter to 100ms so it doesn't slow down app
        var lastEmitTime = System.currentTimeMillis()
        val minEmitIntervalMillis: Long = 100

        while ((connectionInputStream.read(buffer).also { len = it }) > 0) {
            fileOutputStream.write(buffer, 0, len)

            bytes += len

            if (progress!! && null != emitter) {
                val currentTime = System.currentTimeMillis()
                if (currentTime - lastEmitTime > minEmitIntervalMillis) {
                    emitter.emit(bytes, maxBytes)
                    lastEmitTime = currentTime
                }
            }
        }

        if (progress!! && null != emitter) {
            emitter.emit(bytes, maxBytes)
        }

        connectionInputStream.close()
        fileOutputStream.close()

        val ret = JSObject()
        ret.put("path", file!!.absolutePath)
        return ret
    }

    interface FilesystemDownloadCallback {
        fun onSuccess(result: JSObject)

        fun onError(error: Exception)
    }
}
