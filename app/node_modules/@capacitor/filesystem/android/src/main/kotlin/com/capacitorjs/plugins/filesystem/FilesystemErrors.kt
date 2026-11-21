package com.capacitorjs.plugins.filesystem

import io.ionic.libs.ionfilesystemlib.model.IONFILEExceptions

object FilesystemErrors {
    private fun formatErrorCode(number: Int): String {
        return "OS-PLUG-FILE-" + number.toString().padStart(4, '0')
    }

    data class ErrorInfo(
        val code: String,
        val message: String
    )

    fun invalidInputMethod(methodName: String): ErrorInfo = ErrorInfo(
        code = formatErrorCode(5),
        message = "The '$methodName' input parameters aren't valid."
    )

    fun invalidPath(path: String): ErrorInfo = ErrorInfo(
        code = formatErrorCode(6),
        message = "Invalid ${if (path.isNotBlank()) "'$path' " else ""}path."
    )

    val filePermissionsDenied: ErrorInfo = ErrorInfo(
        code = formatErrorCode(7),
        message = "Unable to do file operation, user denied permission request."
    )

    fun doesNotExist(methodName: String, path: String): ErrorInfo = ErrorInfo(
        code = formatErrorCode(8),
        message = "'$methodName' failed because file ${if (path.isNotBlank()) "at '$path' " else ""}does not exist."
    )

    fun notAllowed(methodName: String, notAllowedFor: String): ErrorInfo = ErrorInfo(
        code = formatErrorCode(9),
        message = "'$methodName' not supported for $notAllowedFor."
    )

    fun directoryCreationAlreadyExists(path: String): ErrorInfo = ErrorInfo(
        code = formatErrorCode(10),
        message = "Directory ${if (path.isNotBlank()) "at '$path' " else ""}already exists, cannot be overwritten."
    )

    val missingParentDirectories: ErrorInfo = ErrorInfo(
        code = formatErrorCode(11),
        message = "Missing parent directory – possibly recursive=false was passed or parent directory creation failed."
    )

    val cannotDeleteChildren: ErrorInfo = ErrorInfo(
        code = formatErrorCode(12),
        message = "Cannot delete directory with children; received recursive=false but directory has contents."
    )

    fun operationFailed(methodName: String, errorMessage: String): ErrorInfo = ErrorInfo(
        code = formatErrorCode(13),
        message = "'$methodName' failed with${if (errorMessage.isNotBlank()) ": $errorMessage" else "an unknown error."}"
    )
}

fun Throwable.toFilesystemError(methodName: String): FilesystemErrors.ErrorInfo = when (this) {

    is IONFILEExceptions.UnresolvableUri -> FilesystemErrors.invalidPath(this.uri)

    is IONFILEExceptions.DoesNotExist -> FilesystemErrors.doesNotExist(methodName, this.path)

    is IONFILEExceptions.NotSupportedForContentScheme -> FilesystemErrors.notAllowed(
        methodName,
        notAllowedFor = "content:// URIs"
    )

    is IONFILEExceptions.NotSupportedForDirectory -> FilesystemErrors.notAllowed(
        methodName,
        notAllowedFor = "directories"
    )

    is IONFILEExceptions.NotSupportedForFiles -> FilesystemErrors.notAllowed(
        methodName,
        notAllowedFor = "files, only directories are supported"
    )

    is IONFILEExceptions.CreateFailed.AlreadyExists ->
        FilesystemErrors.directoryCreationAlreadyExists(this.path)

    is IONFILEExceptions.CreateFailed.NoParentDirectory -> FilesystemErrors.missingParentDirectories

    is IONFILEExceptions.DeleteFailed.CannotDeleteChildren -> FilesystemErrors.cannotDeleteChildren

    is IONFILEExceptions.CopyRenameFailed.MixingFilesAndDirectories,
    is IONFILEExceptions.CopyRenameFailed.LocalToContent,
    is IONFILEExceptions.CopyRenameFailed.SourceAndDestinationContent ->
        FilesystemErrors.notAllowed(methodName, "the provided source and destinations")

    is IONFILEExceptions.CopyRenameFailed.DestinationDirectoryExists ->
        FilesystemErrors.directoryCreationAlreadyExists(this.path)

    is IONFILEExceptions.CopyRenameFailed.NoParentDirectory ->
        FilesystemErrors.missingParentDirectories

    else -> FilesystemErrors.operationFailed(methodName, this.localizedMessage ?: "")
}