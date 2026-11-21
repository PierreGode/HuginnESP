package com.capacitorjs.plugins.filesystem

import com.getcapacitor.PluginCall
import io.ionic.libs.ionfilesystemlib.model.IONFILEEncoding
import io.ionic.libs.ionfilesystemlib.model.IONFILEFolderType
import io.ionic.libs.ionfilesystemlib.model.IONFILEReadInChunksOptions
import io.ionic.libs.ionfilesystemlib.model.IONFILEReadOptions
import io.ionic.libs.ionfilesystemlib.model.IONFILESaveMode
import io.ionic.libs.ionfilesystemlib.model.IONFILESaveOptions
import io.ionic.libs.ionfilesystemlib.model.IONFILEUri

internal const val INPUT_APPEND = "append"
private const val INPUT_PATH = "path"
private const val INPUT_DIRECTORY = "directory"
private const val INPUT_ENCODING = "encoding"
private const val INPUT_CHUNK_SIZE = "chunkSize"
private const val INPUT_DATA = "data"
private const val INPUT_RECURSIVE = "recursive"
private const val INPUT_FROM = "from"
private const val INPUT_FROM_DIRECTORY = "directory"
private const val INPUT_TO = "to"
private const val INPUT_TO_DIRECTORY = "toDirectory"

internal data class ReadFileOptions(
    val uri: IONFILEUri.Unresolved,
    val options: IONFILEReadOptions
)

internal data class ReadFileInChunksOptions(
    val uri: IONFILEUri.Unresolved,
    val options: IONFILEReadInChunksOptions
)

internal data class WriteFileOptions(
    val uri: IONFILEUri.Unresolved,
    val options: IONFILESaveOptions
)

internal data class SingleUriWithRecursiveOptions(
    val uri: IONFILEUri.Unresolved,
    val recursive: Boolean
)

internal data class DoubleUri(
    val fromUri: IONFILEUri.Unresolved,
    val toUri: IONFILEUri.Unresolved,
)

/**
 * @return [ReadFileOptions] from JSON inside [PluginCall], or null if input is invalid
 */
internal fun PluginCall.getReadFileOptions(): ReadFileOptions? {
    val uri = getSingleIONFILEUri() ?: return null
    val encoding = IONFILEEncoding.fromEncodingName(getString(INPUT_ENCODING))
    return ReadFileOptions(uri = uri, options = IONFILEReadOptions(encoding))
}

/**
 * @return [ReadFileInChunksOptions] from JSON inside [PluginCall], or null if input is invalid
 */
internal fun PluginCall.getReadFileInChunksOptions(): ReadFileInChunksOptions? {
    val uri = getSingleIONFILEUri() ?: return null
    val encoding = IONFILEEncoding.fromEncodingName(getString(INPUT_ENCODING))
    val chunkSize = getInt(INPUT_CHUNK_SIZE)?.takeIf { it > 0 } ?: return null
    return ReadFileInChunksOptions(
        uri = uri,
        options = IONFILEReadInChunksOptions(encoding, chunkSize)
    )
}

/**
 * @return [ReadFileOptions] from JSON inside [PluginCall], or null if input is invalid
 */
internal fun PluginCall.getWriteFileOptions(): WriteFileOptions? {
    val uri = getSingleIONFILEUri() ?: return null
    val data = getString(INPUT_DATA) ?: return null
    val recursive = getBoolean(INPUT_RECURSIVE) ?: false
    val append = getBoolean(INPUT_APPEND) ?: false
    val saveMode = if (append) IONFILESaveMode.APPEND else IONFILESaveMode.WRITE
    val encoding = IONFILEEncoding.fromEncodingName(getString(INPUT_ENCODING))
    return WriteFileOptions(
        uri = uri,
        options = IONFILESaveOptions(
            data = data,
            encoding = encoding,
            mode = saveMode,
            createFileRecursive = recursive
        )
    )
}

/**
 * @return [SingleUriWithRecursiveOptions] from JSON inside [PluginCall], or null if input is invalid
 */
internal fun PluginCall.getSingleUriWithRecursiveOptions(): SingleUriWithRecursiveOptions? {
    val uri = getSingleIONFILEUri() ?: return null
    val recursive = getBoolean(INPUT_RECURSIVE) ?: false
    return SingleUriWithRecursiveOptions(uri = uri, recursive = recursive)
}

/**
 * @return two uris in form of [DoubleUri] from JSON inside [PluginCall], or null if input is invalid
 */
internal fun PluginCall.getDoubleIONFILEUri(): DoubleUri? {
    val fromPath = getString(INPUT_FROM) ?: return null
    val fromFolder = IONFILEFolderType.fromStringAlias(getString(INPUT_FROM_DIRECTORY))
    val toPath = getString(INPUT_TO) ?: return null
    val toFolder = getString(INPUT_TO_DIRECTORY)?.let { toDirectory ->
        IONFILEFolderType.fromStringAlias(toDirectory)
    } ?: fromFolder
    return DoubleUri(
        fromUri = IONFILEUri.Unresolved(fromFolder, fromPath),
        toUri = IONFILEUri.Unresolved(toFolder, toPath),
    )
}

/**
 * return a single [IONFILEUri.Unresolved] from JSON inside [PluginCall], or null if input is invalid
 */
internal fun PluginCall.getSingleIONFILEUri(): IONFILEUri.Unresolved? {
    val path = getString(INPUT_PATH) ?: return null
    val directoryAlias = getString(INPUT_DIRECTORY)
    return unresolvedUri(path, directoryAlias)
}

private fun unresolvedUri(path: String, directoryAlias: String?) = IONFILEUri.Unresolved(
    parentFolder = IONFILEFolderType.fromStringAlias(directoryAlias),
    uriPath = path
)
