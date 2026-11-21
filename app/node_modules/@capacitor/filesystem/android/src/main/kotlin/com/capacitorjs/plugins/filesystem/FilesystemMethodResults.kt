package com.capacitorjs.plugins.filesystem

import android.net.Uri
import com.getcapacitor.JSArray
import com.getcapacitor.JSObject
import io.ionic.libs.ionfilesystemlib.model.IONFILEFileType
import io.ionic.libs.ionfilesystemlib.model.IONFILEMetadataResult
import io.ionic.libs.ionfilesystemlib.model.IONFILESaveMode
import io.ionic.libs.ionfilesystemlib.model.IONFILEUri

private val OUTPUT_DATA = "data"
private val OUTPUT_NAME = "name"
private val OUTPUT_TYPE = "type"
private val OUTPUT_SIZE = "size"
private val OUTPUT_MODIFIED_TIME = "mtime"
private val OUTPUT_CREATED_TIME = "ctime"
private val OUTPUT_URI = "uri"
private val OUTPUT_FILES = "files"

/**
 * @return a result [JSObject] for reading a file
 */
fun createReadResultObject(readData: String): JSObject =
    JSObject().also { it.putOpt(OUTPUT_DATA, readData) }


/**
 * @return a result [JSObject] for writing/append a file
 */
fun createWriteResultObject(uri: Uri, mode: IONFILESaveMode): JSObject? =
    if (mode == IONFILESaveMode.APPEND) {
        null
    } else {
        createUriResultObject(uri)
    }

/**
 * @return a result [JSObject] for the list of a directories contents
 */
fun createReadDirResultObject(list: List<IONFILEMetadataResult>): JSObject = JSObject().also {
    it.put(OUTPUT_FILES, JSArray(list.map { child -> child.toResultObject() }))
}

/**
 * @return a result [JSObject] for stat, from the [IONFILEMetadataResult] object
 */
fun IONFILEMetadataResult.toResultObject(): JSObject = JSObject().apply {
    put(OUTPUT_NAME, name)
    put(OUTPUT_TYPE, if (type is IONFILEFileType.Directory) "directory" else "file")
    put(OUTPUT_SIZE, size)
    put(OUTPUT_MODIFIED_TIME, lastModifiedTimestamp)
    put(OUTPUT_CREATED_TIME, createdTimestamp)
    put(OUTPUT_URI, uri)
}

/**
 * @return a result [JSObject] based on a resolved uri [IONFILEUri.Resolved]
 */
fun IONFILEUri.Resolved.toResultObject(): JSObject = createUriResultObject(this.uri)

/**
 * @return a result [JSObject] for an Android [Uri]
 */
fun createUriResultObject(uri: Uri): JSObject =
    JSObject().also { it.put(OUTPUT_URI, uri.toString()) }