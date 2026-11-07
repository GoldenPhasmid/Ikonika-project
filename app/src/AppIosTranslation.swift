import SwiftUI
import Translation
import UIKit

typealias IkonikaStatusCallback = @convention(c) (UnsafePointer<CChar>?, UnsafePointer<CChar>?, Int32) -> Void
typealias IkonikaDownloadCallback = @convention(c) (UnsafePointer<CChar>?, UnsafePointer<CChar>?, Bool, UnsafePointer<CChar>?) -> Void
typealias IkonikaResultCallback = @convention(c) (UInt64, UnsafePointer<CChar>?, UnsafePointer<CChar>?) -> Void

private enum TranslationStatus: Int32 {
    case unsupported = 0
    case supported = 1
    case installed = 2
}

private struct TranslationCallbacks {
    var status: IkonikaStatusCallback?
    var download: IkonikaDownloadCallback?
    var result: IkonikaResultCallback?
}

private var callbacks = TranslationCallbacks()

@MainActor private func reportStatus(_ source: String, _ target: String, _ status: TranslationStatus) {
    source.withCString { sourcePointer in
        target.withCString { targetPointer in
            callbacks.status?(sourcePointer, targetPointer, status.rawValue)
        }
    }
}

@MainActor private func reportDownload(_ source: String, _ target: String, _ success: Bool, _ error: String) {
    source.withCString { sourcePointer in
        target.withCString { targetPointer in
            error.withCString { errorPointer in
                callbacks.download?(sourcePointer, targetPointer, success, errorPointer)
            }
        }
    }
}

@MainActor private func reportResult(_ generation: UInt64, _ text: String, _ result: String) {
    text.withCString { textPointer in
        result.withCString { resultPointer in
            callbacks.result?(generation, textPointer, resultPointer)
        }
    }
}

private enum TranslationJob {
    case prepare
    case translate(UInt64, String)
}

@available(iOS 18.0, *)
@MainActor private final class TranslationHost: ObservableObject {
    static let shared = TranslationHost()

    @Published var configuration: TranslationSession.Configuration?

    private var source = ""
    private var target = ""
    private var jobs: AsyncStream<TranslationJob>?
    private var continuation: AsyncStream<TranslationJob>.Continuation?
    private var controller: UIHostingController<TranslationHostView>?

    func submit(_ job: TranslationJob, source: String, target: String) {
        attach()

        if source != self.source || target != self.target || continuation == nil {
            continuation?.finish()

            let (stream, newContinuation) = AsyncStream<TranslationJob>.makeStream()

            self.source = source
            self.target = target
            jobs = stream
            continuation = newContinuation
            configuration = TranslationSession.Configuration(source: Locale.Language(identifier: source),
                                                             target: Locale.Language(identifier: target))
        }

        continuation?.yield(job)
    }

    func run(_ session: TranslationSession) async {
        guard let jobs else {
            return
        }

        let source = self.source
        let target = self.target

        for await job in jobs {
            switch job {
            case .prepare:
                do {
                    try await session.prepareTranslation()
                    reportDownload(source, target, true, "")
                } catch {
                    reportDownload(source, target, false, error.localizedDescription)
                }
            case .translate(let generation, let text):
                do {
                    let response = try await session.translate(text)
                    reportResult(generation, text, response.targetText)
                } catch {
                    reportResult(generation, text, "")
                }
            }
        }
    }

    private func attach() {
        if controller != nil {
            return
        }

        let window = UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .flatMap { $0.windows }
            .first { $0.isKeyWindow }

        guard var top = window?.rootViewController else {
            return
        }

        while let presented = top.presentedViewController {
            top = presented
        }

        let hosting = UIHostingController(rootView: TranslationHostView(host: self))
        hosting.view.frame = CGRect(x: 0, y: 0, width: 1, height: 1)
        hosting.view.alpha = 0
        hosting.view.isUserInteractionEnabled = false

        top.addChild(hosting)
        top.view.addSubview(hosting.view)
        hosting.didMove(toParent: top)

        controller = hosting
    }
}

@available(iOS 18.0, *)
private struct TranslationHostView: View {
    @ObservedObject var host: TranslationHost

    var body: some View {
        Color.clear
            .frame(width: 1, height: 1)
            .translationTask(host.configuration) { session in
                await host.run(session)
            }
    }
}

@_cdecl("IkonikaTranslationInstall")
public func ikonikaTranslationInstall(_ status: IkonikaStatusCallback?, _ download: IkonikaDownloadCallback?,
                                      _ result: IkonikaResultCallback?) {
    callbacks = TranslationCallbacks(status: status, download: download, result: result)
}

@_cdecl("IkonikaTranslationCheck")
public func ikonikaTranslationCheck(_ sourcePointer: UnsafePointer<CChar>, _ targetPointer: UnsafePointer<CChar>) {
    let source = String(cString: sourcePointer)
    let target = String(cString: targetPointer)

    Task { @MainActor in
        guard #available(iOS 18.0, *) else {
            reportStatus(source, target, .unsupported)
            return
        }

        let availability = await LanguageAvailability().status(from: Locale.Language(identifier: source),
                                                               to: Locale.Language(identifier: target))

        switch availability {
        case .installed:
            reportStatus(source, target, .installed)
        case .supported:
            reportStatus(source, target, .supported)
        default:
            reportStatus(source, target, .unsupported)
        }
    }
}

@_cdecl("IkonikaTranslationDownload")
public func ikonikaTranslationDownload(_ sourcePointer: UnsafePointer<CChar>, _ targetPointer: UnsafePointer<CChar>) {
    let source = String(cString: sourcePointer)
    let target = String(cString: targetPointer)

    Task { @MainActor in
        guard #available(iOS 18.0, *) else {
            reportDownload(source, target, false, "translation needs iOS 18")
            return
        }

        TranslationHost.shared.submit(.prepare, source: source, target: target)
    }
}

@_cdecl("IkonikaTranslationTranslate")
public func ikonikaTranslationTranslate(_ generation: UInt64, _ textPointer: UnsafePointer<CChar>,
                                        _ sourcePointer: UnsafePointer<CChar>, _ targetPointer: UnsafePointer<CChar>) {
    let text = String(cString: textPointer)
    let source = String(cString: sourcePointer)
    let target = String(cString: targetPointer)

    Task { @MainActor in
        guard #available(iOS 18.0, *) else {
            reportResult(generation, text, "")
            return
        }

        TranslationHost.shared.submit(.translate(generation, text), source: source, target: target)
    }
}
