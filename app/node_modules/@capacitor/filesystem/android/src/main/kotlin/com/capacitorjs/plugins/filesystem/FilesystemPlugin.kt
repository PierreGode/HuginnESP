package com.capacitorjs.plugins.filesystem

import android.Manifest
import android.media.MediaScannerConnection
import android.os.Build
import android.os.Environment
import android.util.Log
import com.getcapacitor.JSObject
import com.getcapacitor.Logger
import com.getcapacitor.PermissionState
import com.getcapacitor.Plugin
import com.getcapacitor.PluginCall
import com.getcapacitor.PluginMethod
import com.getcapacitor.annotation.CapacitorPlugin
import com.getcapacitor.annotation.Permission
import com.getcapacitor.annotation.PermissionCallback
import com.getcapacitor.plugin.util.HttpRequestHandler.ProgressEmitter
import io.ionic.libs.ionfilesystemlib.IONFILEController
import io.ionic.libs.ionfilesystemlib.model.IONFILECreateOptions
import io.ionic.libs.ionfilesystemlib.model.IONFILEDeleteOptions
import io.ionic.libs.ionfilesystemlib.model.IONFILEUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onCompletion
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.launch
import org.json.JSONException

private const val PUBLIC_STORAGE = "publicStorage"
private const val PUBLIC_STORAGE_ABOVE_ANDROID_10 = "publicStorageAboveAPI29"
private const val PERMISSION_GRANTED = "granted"

@CapacitorPlugin(
    name = "Filesystem",
    permissions = [
        Permission(
            strings = [Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE],
            alias = PUBLIC_STORAGE
        ),
        /*
        For SDK versions 30-32 (Android 11 and Android 12)
        Could be that certain files may require read permission, such as local file path to photos/videos in gallery
        */
        Permission(
            strings = [Manifest.permission.READ_EXTERNAL_STORAGE],
            alias = PUBLIC_STORAGE_ABOVE_ANDROID_10
        )
    ]
)
class FilesystemPlugin : Plugin() {

    private var legacyImplementation: LegacyFilesystemImplementation? = null

    private val coroutineScope: CoroutineScope by lazy { CoroutineScope(Dispatchers.Main) }
    private val controller: IONFILEController by lazy { IONFILEController(context.applicationContext) }

    override fun load() {
        super.load()
        legacyImplementation = LegacyFilesystemImplementation(context)
    }

    override fun handleOnDestroy() {
        super.handleOnDestroy()
        coroutineScope.cancel()
    }

    @PluginMethod
    fun readFile(call: PluginCall) {
        val input: ReadFileOptions = call.getReadFileOptions() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input.uri, call) { uri ->
            controller.readFile(uri, input.options)
                .onSuccess { call.sendSuccess(result = createReadResultObject(it)) }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod(returnType = PluginMethod.RETURN_CALLBACK)
    fun readFileInChunks(call: PluginCall) {
        val input: ReadFileInChunksOptions = call.getReadFileInChunksOptions() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input.uri, call) { uri ->
            controller.readFileInChunks(uri, input.options)
                .onEach { chunk ->
                    call.sendSuccess(result = createReadResultObject(chunk), keepCallback = true)
                }
                .onCompletion { error ->
                    if (error == null) {
                        call.sendSuccess(result = createReadResultObject(""))
                    }
                }
                .catch {
                    call.sendError(it.toFilesystemError(call.methodName))
                }
                .launchIn(coroutineScope)
        }
    }

    @PluginMethod
    fun writeFile(call: PluginCall) {
        val input: WriteFileOptions = call.getWriteFileOptions() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input.uri, call) { uri ->
            controller.saveFile(uri, input.options)
                .onSuccess { uriSaved ->
                    // update mediaStore index only if file was written to external storage
                    if (uri.inExternalStorage) {
                        uriSaved.path?.let {
                            MediaScannerConnection.scanFile(context, arrayOf(it), null, null)
                        }
                    }
                    call.sendSuccess(result = createWriteResultObject(uriSaved, input.options.mode))
                }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod
    fun appendFile(call: PluginCall) {
        try {
            call.data.putOpt(INPUT_APPEND, true)
        } catch (ex: JSONException) {
            Log.e(logTag, "Tried to set `append` in `PluginCall`, but got exception", ex)
            call.sendError(
                FilesystemErrors.operationFailed(call.methodName, ex.localizedMessage ?: "")
            )
            return
        }
        writeFile(call)
    }

    @PluginMethod
    fun deleteFile(call: PluginCall) {
        val input = call.getSingleIONFILEUri() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input, call) { uri ->
            controller.delete(uri, IONFILEDeleteOptions(recursive = false))
                .onSuccess { call.sendSuccess() }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod
    fun mkdir(call: PluginCall) {
        val input = call.getSingleUriWithRecursiveOptions() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input.uri, call) { uri ->
            controller.createDirectory(uri, IONFILECreateOptions(input.recursive))
                .onSuccess { call.sendSuccess() }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod
    fun rmdir(call: PluginCall) {
        val input = call.getSingleUriWithRecursiveOptions() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input.uri, call) { uri ->
            controller.delete(uri, IONFILEDeleteOptions(input.recursive))
                .onSuccess { call.sendSuccess() }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod
    fun readdir(call: PluginCall) {
        val input = call.getSingleIONFILEUri() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input, call) { uri ->
            controller.listDirectory(uri)
                .onSuccess { call.sendSuccess(result = createReadDirResultObject(it)) }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod
    fun getUri(call: PluginCall) {
        val input = call.getSingleIONFILEUri() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        coroutineScope.launch {
            controller.getFileUri(input)
                .onSuccess { resolvedUri -> call.sendSuccess(result = resolvedUri.toResultObject()) }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod
    fun stat(call: PluginCall) {
        val input = call.getSingleIONFILEUri() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input, call) { uri ->
            controller.getMetadata(uri)
                .onSuccess { metadata -> call.sendSuccess(result = metadata.toResultObject()) }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod
    fun rename(call: PluginCall) {
        val input = call.getDoubleIONFILEUri() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input.fromUri, input.toUri, call) { source, destination ->
            controller.move(source, destination)
                .onSuccess { call.sendSuccess() }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod
    fun copy(call: PluginCall) {
        val input = call.getDoubleIONFILEUri() ?: run {
            call.sendError(FilesystemErrors.invalidInputMethod(call.methodName))
            return
        }
        runWithPermission(input.fromUri, input.toUri, call) { source, destination ->
            controller.copy(source, destination)
                .onSuccess { call.sendSuccess(createUriResultObject(it)) }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    @PluginMethod
    @Deprecated("Use @capacitor/file-transfer plugin instead")
    fun downloadFile(call: PluginCall) {
        try {
            val directory = call.getString("directory", Environment.DIRECTORY_DOWNLOADS)

            if (legacyImplementation?.isPublicDirectory(directory) == true &&
                !isStoragePermissionGranted(false)
            ) {
                requestAllPermissions(call, "permissionCallback")
                return
            }

            val emitter = ProgressEmitter { bytes: Int?, contentLength: Int? ->
                val ret = JSObject()
                ret.put("url", call.getString("url"))
                ret.put("bytes", bytes)
                ret.put("contentLength", contentLength)
                notifyListeners("progress", ret)
            }

            legacyImplementation?.downloadFile(
                call,
                bridge,
                emitter,
                object : LegacyFilesystemImplementation.FilesystemDownloadCallback {
                    override fun onSuccess(result: JSObject) {
                        // update mediaStore index only if file was written to external storage
                        if (legacyImplementation?.isPublicDirectory(directory) == true) {
                            MediaScannerConnection.scanFile(
                                context,
                                arrayOf(result.getString("path")),
                                null,
                                null
                            )
                        }
                        call.resolve(result)
                    }

                    override fun onError(error: Exception) {
                        call.reject("Error downloading file: " + error.localizedMessage, error)
                    }
                }
            )
        } catch (ex: Exception) {
            call.reject("Error downloading file: " + ex.localizedMessage, ex)
        }
    }

    @PluginMethod
    override fun checkPermissions(call: PluginCall) {
        if (isStoragePermissionGranted(false)) {
            call.sendSuccess(JSObject().also { it.put(PUBLIC_STORAGE, PERMISSION_GRANTED) })
        } else {
            super.checkPermissions(call)
        }
    }

    @PluginMethod
    override fun requestPermissions(call: PluginCall) {
        if (isStoragePermissionGranted(false)) {
            call.sendSuccess(JSObject().also { it.put(PUBLIC_STORAGE, PERMISSION_GRANTED) })
        } else {
            super.requestPermissions(call)
        }
    }

    @PermissionCallback
    private fun permissionCallback(call: PluginCall) {
        if (!isStoragePermissionGranted(true)) {
            Logger.debug(logTag, "User denied storage permission")
            call.sendError(FilesystemErrors.filePermissionsDenied)
            return
        }

        when (call.methodName) {
            // appendFile and writeFile have the same implementation, hence the same method is called;
            //  the only difference being that we add a boolean for append in the PluginCall,
            //  which is done before this method is called.
            "appendFile", "writeFile" -> writeFile(call)
            "deleteFile" -> deleteFile(call)
            "mkdir" -> mkdir(call)
            "rmdir" -> rmdir(call)
            "rename" -> rename(call)
            "copy" -> copy(call)
            "readFile" -> readFile(call)
            "readFileInChunks" -> readFileInChunks(call)
            "readdir" -> readdir(call)
            "getUri" -> getUri(call)
            "stat" -> stat(call)
            "downloadFile" -> downloadFile(call)
        }
    }

    /**
     * Runs a suspend code if the app has permission to access the uri
     *
     * Will ask for permission if it has not been granted.
     *
     * May return an error if the uri is not resolvable.
     *
     * @param uri the uri pointing to the file / directory
     * @param call the capacitor plugin call
     * @param onPermissionGranted the callback to run the suspending code
     */
    private fun runWithPermission(
        uri: IONFILEUri.Unresolved,
        call: PluginCall,
        onPermissionGranted: suspend (resolvedUri: IONFILEUri.Resolved) -> Unit
    ) {
        coroutineScope.launch {
            controller.getFileUri(uri)
                .onSuccess { resolvedUri ->
                    // certain files like a photo/video in gallery may require read permission on Android 11 and 12.
                    if (
                        resolvedUri.inExternalStorage
                        && !isStoragePermissionGranted(shouldRequestAboveAndroid10 = uri.parentFolder == null)
                    ) {
                        requestAllPermissions(call, this@FilesystemPlugin::permissionCallback.name)
                    } else {
                        onPermissionGranted(resolvedUri)
                    }
                }
                .onFailure { call.sendError(it.toFilesystemError(call.methodName)) }
        }
    }

    /**
     * Runs a suspend code if the app has permission to access both to and from uris
     *
     * Will ask for permission if it has not been granted.
     *
     * May return an error if the uri is not resolvable.
     *
     * @param fromUri the source uri pointing to the file / directory
     * @param toUri the destination uri pointing to the file / directory
     * @param call the capacitor plugin call
     * @param onPermissionGranted the callback to run the suspending code
     */
    private fun runWithPermission(
        fromUri: IONFILEUri.Unresolved,
        toUri: IONFILEUri.Unresolved,
        call: PluginCall,
        onPermissionGranted: suspend (resolvedSourceUri: IONFILEUri.Resolved, resolvedDestinationUri: IONFILEUri.Resolved) -> Unit
    ) {
        runWithPermission(fromUri, call) { resolvedSourceUri ->
            runWithPermission(toUri, call) { resolvedDestinationUri ->
                onPermissionGranted(resolvedSourceUri, resolvedDestinationUri)
            }
        }
    }

    /**
     * Checks the the given permission is granted or not
     * @param shouldRequestAboveAndroid10 whether or not should check for read permission above android 10
     *  May vary with the kind of file path that is provided.
     * @return Returns true if the app is running on Android 13 (API 33) or newer, or if the permission is already granted
     * or false if it is denied.
     */
    private fun isStoragePermissionGranted(shouldRequestAboveAndroid10: Boolean): Boolean = when {
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU -> true

        Build.VERSION.SDK_INT >= Build.VERSION_CODES.R ->
            !shouldRequestAboveAndroid10 || getPermissionState(PUBLIC_STORAGE_ABOVE_ANDROID_10) == PermissionState.GRANTED

        else -> getPermissionState(PUBLIC_STORAGE) == PermissionState.GRANTED
    }
}