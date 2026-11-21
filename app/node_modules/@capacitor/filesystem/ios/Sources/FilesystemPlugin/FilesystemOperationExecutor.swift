import Capacitor
import Foundation
import IONFilesystemLib
import Combine

class FilesystemOperationExecutor {
    let service: FileService
    private var cancellables = Set<AnyCancellable>()

    init(service: FileService) {
        self.service = service
    }

    func execute(_ operation: FilesystemOperation, _ call: CAPPluginCall) {
        do {
            var resultData: PluginCallResultData?

            switch operation {
            case .readFile(let url, let encoding):
                let data = try service.readEntireFile(atURL: url, withEncoding: encoding).textValue
                resultData = [Constants.ResultDataKey.data: data]
            case .readFileInChunks(let url, let encoding, let chunkSize):
                try processFileInChunks(at: url, withEncoding: encoding, chunkSize: chunkSize, for: operation, call)
                return
            case .write(let url, let encodingMapper, let recursive):
                try service.saveFile(atURL: url, withEncodingAndData: encodingMapper, includeIntermediateDirectories: recursive)
                resultData = [Constants.ResultDataKey.uri: url.absoluteString]
            case .append(let url, let encodingMapper, let recursive):
                try service.appendData(encodingMapper, atURL: url, includeIntermediateDirectories: recursive)
            case .delete(let url):
                try service.deleteFile(atURL: url)
            case .mkdir(let url, let recursive):
                try service.createDirectory(atURL: url, includeIntermediateDirectories: recursive)
            case .rmdir(let url, let recursive):
                try service.removeDirectory(atURL: url, includeIntermediateDirectories: recursive)
            case .readdir(let url):
                let directoryAttributes = try service.listDirectory(atURL: url)
                    .map { try fetchItemAttributesJSObject(using: service, atURL: $0) }
                resultData = [Constants.ResultDataKey.files: directoryAttributes]
            case .stat(let url):
                resultData = try fetchItemAttributesJSObject(using: service, atURL: url)
            case .getUri(let url):
                resultData = [Constants.ResultDataKey.uri: url.absoluteString]
            case .rename(let source, let destination):
                try service.renameItem(fromURL: source, toURL: destination)
            case .copy(let source, let destination):
                try service.copyItem(fromURL: source, toURL: destination)
                resultData = [Constants.ResultDataKey.uri: destination.absoluteString]
            }

            call.handleSuccess(resultData)
        } catch {
            call.handleError(mapError(error, for: operation))
        }
    }
}

private extension FilesystemOperationExecutor {
    func processFileInChunks(at url: URL, withEncoding encoding: IONFILEEncoding, chunkSize: Int, for operation: FilesystemOperation, _ call: CAPPluginCall) throws {
        let chunkSizeToUse = chunkSizeToUse(basedOn: chunkSize, and: encoding)
        try service.readFileInChunks(atURL: url, withEncoding: encoding, andChunkSize: chunkSizeToUse)
            .sink(receiveCompletion: { completion in
                switch completion {
                case .finished:
                    call.handleSuccess([Constants.ResultDataKey.data: Constants.ConfigurationValue.endOfFile])
                case .failure(let error):
                    call.handleError(self.mapError(error, for: operation))
                }
            }, receiveValue: {
                call.handleSuccess([Constants.ResultDataKey.data: $0.textValue], true)
            })
            .store(in: &cancellables)
    }

    private func chunkSizeToUse(basedOn chunkSize: Int, and encoding: IONFILEEncoding) -> Int {
        // When dealing with byte buffers, we need chunk size that are multiples of 3
        // We're treating byte buffers as base64 data, and size multiple of 3 makes it so that chunks can be concatenated
        encoding == .byteBuffer ? chunkSize - chunkSize % 3 + 3 : chunkSize
    }

    func mapError(_ error: Error, for operation: FilesystemOperation) -> FilesystemError {
        var path = ""
        var method: IONFileMethod = IONFileMethod.getUri
        switch operation {
        case .readFile(let url, _): path = url.absoluteString; method = .readFile
        case .readFileInChunks(let url, _, _): path = url.absoluteString; method = .readFileInChunks
        case .write(let url, _, _): path = url.absoluteString; method = .writeFile
        case .append(let url, _, _): path = url.absoluteString; method = .appendFile
        case .delete(let url): path = url.absoluteString; method = .deleteFile
        case .mkdir(let url, _): path = url.absoluteString; method = .mkdir
        case .rmdir(let url, _): path = url.absoluteString; method = .rmdir
        case .readdir(let url): path = url.absoluteString; method = .readdir
        case .stat(let url): path = url.absoluteString; method = .stat
        case .getUri(let url): return FilesystemError.invalidPath(url.absoluteString)
        case .rename(let sourceUrl, _): path = sourceUrl.absoluteString; method = .rename
        case .copy(let sourceUrl, _): path = sourceUrl.absoluteString; method = .copy
        }

        return mapError(error, withPath: path, andMethod: method)
    }

    private func mapError(_ error: Error, withPath path: String, andMethod method: IONFileMethod) -> FilesystemError {
        return switch error {
        case IONFILEDirectoryManagerError.notEmpty: .cannotDeleteChildren
        case IONFILEDirectoryManagerError.alreadyExists: .directoryAlreadyExists(path)
        case IONFILEFileManagerError.missingParentFolder: .parentDirectoryMissing
        case IONFILEFileManagerError.fileNotFound: .fileNotFound(method: method, path)
        default: .operationFailed(method: method, error)
        }
    }

    func fetchItemAttributesJSObject(using service: FileService, atURL url: URL) throws -> IONFILEItemAttributeModel.JSResult {
        let attributes = try service.getItemAttributes(atURL: url)
        return attributes.toJSResult(with: url)
    }
}
