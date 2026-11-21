import Capacitor
import IONFilesystemLib

extension CAPPluginCall {
    func getEncoding(_ key: String) -> IONFILEEncoding {
        if let encodingParameter = getString(key) {
            .string(encoding: .create(from: encodingParameter))
        } else {
            .byteBuffer
        }
    }

    func getSearchPath(_ key: String) -> IONFILESearchPath {
        getSearchPath(key, withDefaultSearchPath: .raw, andDefaultDirectoryType: .document)
    }

    func getSearchPath(_ key: String, withDefault defaultValue: IONFILESearchPath) -> IONFILESearchPath {
        getSearchPath(key, withDefaultSearchPath: defaultValue)
    }

    func getEncodingMapper() -> IONFILEEncodingValueMapper? {
        guard let data: String = getString(Constants.MethodParameter.data) else {
            return nil
        }
        return switch getEncoding(Constants.MethodParameter.encoding) {
        case .byteBuffer:
            if let base64Data = Data.capacitor.data(base64EncodedOrDataUrl: data) {
                .byteBuffer(value: base64Data)
            } else {
                nil
            }
        case .string(encoding: let stringEncoding):
            .string(encoding: stringEncoding, value: data)
        @unknown default: nil
        }
    }

    func getIONFileMethod() -> IONFileMethod {
        return IONFileMethod(rawValue: self.methodName) ?? IONFileMethod.getUri
    }

    func handleSuccess(_ data: PluginCallResultData?, _ keepCallAlive: Bool = false) {
        keepAlive = keepCallAlive
        if let data {
            resolve(data)
        } else {
            resolve()
        }
    }

    func handlePermissionSuccess() {
        handleSuccess([Constants.ResultDataKey.publicStorage: Constants.ResultDataValue.granted])
    }

    func handleError(_ error: FilesystemError) {
        let errorPair = error.toCodeMessagePair()
        reject(errorPair.message, errorPair.code)
    }

    private func getSearchPath(
        _ key: String, withDefaultSearchPath defaultSearchPath: IONFILESearchPath, andDefaultDirectoryType defaultDirectoryType: IONFILEDirectoryType? = nil
    ) -> IONFILESearchPath {
        guard let directoryParameter = getString(key), directoryParameter.isEmpty == false else {
            return defaultSearchPath
        }

        return if let type = IONFILEDirectoryType.create(from: directoryParameter) ?? defaultDirectoryType {
            .directory(type: type)
        } else {
            defaultSearchPath
        }
    }
}
