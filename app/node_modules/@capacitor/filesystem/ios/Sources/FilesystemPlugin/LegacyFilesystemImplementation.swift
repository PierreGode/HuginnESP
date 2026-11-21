import Foundation
import Capacitor

@objc public class LegacyFilesystemImplementation: NSObject {

    public typealias ProgressEmitter = (_ bytes: Int64, _ contentLength: Int64) -> Void

    // swiftlint:disable function_body_length
    @objc public func downloadFile(call: CAPPluginCall, emitter: @escaping ProgressEmitter, config: InstanceConfiguration?) throws {
        let directory = call.getString("directory", "DOCUMENTS")
        guard let path = call.getString("path") else {
            call.reject("Invalid file path")
            return
        }
        guard var urlString = call.getString("url") else { throw URLError(.badURL) }

        func handleDownload(downloadLocation: URL?, response: URLResponse?, error: Error?) {
            if let error = error {
                CAPLog.print("Error on download file", String(describing: downloadLocation), String(describing: response), String(describing: error))
                call.reject(error.localizedDescription, "DOWNLOAD", error, nil)
                return
            }

            if let httpResponse = response as? HTTPURLResponse {
                if !(200...299).contains(httpResponse.statusCode) {
                    CAPLog.print("Error downloading file:", urlString, httpResponse)
                    call.reject("Error downloading file: \(urlString)", "DOWNLOAD")
                    return
                }
                HttpRequestHandler.setCookiesFromResponse(httpResponse, config)
            }

            guard let location = downloadLocation else {
                call.reject("Unable to get file after downloading")
                return
            }

            let fileManager = FileManager.default

            if let foundDir = getDirectory(directory: directory) {
                let dir = fileManager.urls(for: foundDir, in: .userDomainMask).first

                do {
                    let dest = dir!.appendingPathComponent(path)
                    CAPLog.print("Attempting to write to file destination: \(dest.absoluteString)")

                    if !FileManager.default.fileExists(atPath: dest.deletingLastPathComponent().absoluteString) {
                        try FileManager.default.createDirectory(at: dest.deletingLastPathComponent(), withIntermediateDirectories: true, attributes: nil)
                    }

                    if FileManager.default.fileExists(atPath: dest.relativePath) {
                        do {
                            CAPLog.print("File already exists. Attempting to remove file before writing.")
                            try fileManager.removeItem(at: dest)
                        } catch let error {
                            call.reject("Unable to remove existing file: \(error.localizedDescription)")
                            return
                        }
                    }

                    try fileManager.moveItem(at: location, to: dest)
                    CAPLog.print("Downloaded file successfully! \(dest.absoluteString)")
                    call.resolve(["path": dest.absoluteString])
                } catch let error {
                    call.reject("Unable to download file: \(error.localizedDescription)", "DOWNLOAD", error)
                    return
                }
            } else {
                call.reject("Unable to download file. Couldn't find directory \(directory)")
            }
        }

        let method = call.getString("method", "GET")

        let headers = (call.getObject("headers") ?? [:]) as [String: Any]
        let params = (call.getObject("params") ?? [:]) as [String: Any]
        let responseType = call.getString("responseType", "text")
        let connectTimeout = call.getDouble("connectTimeout")
        let readTimeout = call.getDouble("readTimeout")

        if urlString == urlString.removingPercentEncoding {
            guard let encodedUrlString = urlString.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed)  else { throw URLError(.badURL) }
            urlString = encodedUrlString
        }

        let progress = call.getBool("progress", false)

        let request = try HttpRequestHandler.CapacitorHttpRequestBuilder()
            .setUrl(urlString)
            .setMethod(method)
            .setUrlParams(params)
            .openConnection()
            .build()

        request.setRequestHeaders(headers)

        // Timeouts in iOS are in seconds. So read the value in millis and divide by 1000
        let timeout = (connectTimeout ?? readTimeout ?? 600000.0) / 1000.0
        request.setTimeout(timeout)

        if let data = call.options["data"] as? JSValue {
            do {
                try request.setRequestBody(data)
            } catch {
                // Explicitly reject if the http request body was not set successfully,
                // so as to not send a known malformed request, and to provide the developer with additional context.
                call.reject(error.localizedDescription, (error as NSError).domain, error, nil)
                return
            }
        }

        var session: URLSession!
        var task: URLSessionDownloadTask!
        let urlRequest = request.getUrlRequest()

        if progress {
            class ProgressDelegate: NSObject, URLSessionDataDelegate, URLSessionDownloadDelegate {
                private var handler: (URL?, URLResponse?, Error?) -> Void
                private var downloadLocation: URL?
                private var response: URLResponse?
                private var emitter: (Int64, Int64) -> Void
                private var lastEmitTimestamp: TimeInterval = 0.0

                init(downloadHandler: @escaping (URL?, URLResponse?, Error?) -> Void, progressEmitter: @escaping (Int64, Int64) -> Void) {
                    handler = downloadHandler
                    emitter = progressEmitter
                }

                func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didWriteData bytesWritten: Int64, totalBytesWritten: Int64, totalBytesExpectedToWrite: Int64) {
                    let currentTimestamp = Date().timeIntervalSince1970
                    let timeElapsed = currentTimestamp - lastEmitTimestamp

                    if totalBytesExpectedToWrite > 0 {
                        if timeElapsed >= 0.1 {
                            emitter(totalBytesWritten, totalBytesExpectedToWrite)
                            lastEmitTimestamp = currentTimestamp
                        }
                    } else {
                        emitter(totalBytesWritten, 0)
                        lastEmitTimestamp = currentTimestamp
                    }
                }

                func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didFinishDownloadingTo location: URL) {
                    downloadLocation = location
                    handler(downloadLocation, downloadTask.response, downloadTask.error)
                }

                func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
                    if error != nil {
                        handler(downloadLocation, task.response, error)
                    }
                }
            }

            let progressDelegate = ProgressDelegate(downloadHandler: handleDownload, progressEmitter: emitter)
            session = URLSession(configuration: .default, delegate: progressDelegate, delegateQueue: nil)
            task = session.downloadTask(with: urlRequest)
        } else {
            task = URLSession.shared.downloadTask(with: urlRequest, completionHandler: handleDownload)
        }

        task.resume()
    }
    // swiftlint:enable function_body_length

    /**
     * Get the SearchPathDirectory corresponding to the JS string
     */
    private func getDirectory(directory: String?) -> FileManager.SearchPathDirectory? {
        if let directory = directory {
            switch directory {
            case "CACHE":
                return .cachesDirectory
            case "LIBRARY":
                return .libraryDirectory
            default:
                return .documentDirectory
            }
        }
        return nil
    }
}
