import Foundation

enum AuthError: Error, Equatable, LocalizedError {
    case invalidRedirect, stateMismatch, expiredAttempt, noActiveAttempt
    case invalidResponse, rejected, network, cancelled, storage
    var errorDescription: String? {
        switch self {
        case .invalidRedirect: return "Paste the complete final Sony redirect URL from your browser."
        case .stateMismatch: return "This URL does not match the current sign-in. Start a new sign-in."
        case .expiredAttempt: return "Sign-in expired. Start again in your browser."
        case .noActiveAttempt: return "Start a new sign-in before submitting a redirect."
        case .invalidResponse: return "Sony returned an unsupported account response."
        case .rejected: return "Sony rejected this request. Start a new sign-in and try again."
        case .network: return "Could not reach Sony. Check your connection and start again."
        case .cancelled: return "Sign-in cancelled."
        case .storage: return "Could not access or validate the local credentials file. Check its permissions or reset the saved account."
        }
    }
}

@MainActor
protocol AuthTransport {
    func send(_ request: URLRequest) async throws -> (Data, Int)
}

@MainActor
final class PSNAuthService {
    static let redirect = "https://remoteplay.dl.playstation.net/remoteplay/redirect"
    // Compatible Remote Play client configuration adapted from pinned Chiaki.
    // These distributed application constants are not user credentials.
    private static let clientID = "ba495a24-818c-472b-b12d-ff231c1b5745"
    private static let clientCredential = "mvaiZkRsAsI1IBkY"
    private static let tokenURL = "https://auth.api.sonyentertainmentnetwork.com/2.0/oauth/token"
    private static let scope = "psn:clientapp referenceDataService:countryConfig.read pushNotification:webSocket.desktop.connect sessionManager:remotePlaySession.system.update"
    private let transport: AuthTransport
    private let now: () -> Date
    private struct Attempt { let id: UUID; let state: String; let expires: Date }
    private var attempt: Attempt?
    private var generation = UUID()
    init(transport: AuthTransport? = nil, now: @escaping () -> Date = Date.init) {
        self.transport = transport ?? URLSessionAuthTransport()
        self.now = now
    }
    func begin() -> URL {
        generation = UUID()
        let state = UUID().uuidString + UUID().uuidString
        attempt = Attempt(id: generation, state: state, expires: now().addingTimeInterval(600))
        var url = URLComponents(string: "https://auth.api.sonyentertainmentnetwork.com/2.0/oauth/authorize")!
        let items = ["service_entity": "urn:service-entity:psn", "response_type": "code",
                     "client_id": Self.clientID, "redirect_uri": Self.redirect, "scope": Self.scope,
                     "request_locale": "en_US", "ui": "pr", "service_logo": "ps", "layout_type": "popup",
                     "smcid": "remoteplay", "prompt": "always", "PlatformPrivacyWs1": "minimal", "state": state]
        url.queryItems = items.sorted(by: { $0.key < $1.key }).map { URLQueryItem(name: $0.key, value: $0.value) }
        return url.url!
    }
    func cancel() { generation = UUID(); attempt = nil }
    private func code(from text: String, matching attempt: Attempt) throws -> String {
        guard text.utf8.count <= 16384,
              let url = URLComponents(string: text.trimmingCharacters(in: .whitespacesAndNewlines)),
              url.scheme == "https", url.host == "remoteplay.dl.playstation.net",
              url.percentEncodedPath == "/remoteplay/redirect", url.user == nil, url.password == nil,
              url.port == nil || url.port == 443, url.fragment == nil else { throw AuthError.invalidRedirect }
        let items = url.queryItems ?? []
        guard !items.contains(where: { $0.name == "error" }) else { throw AuthError.rejected }
        let codes = items.filter { $0.name == "code" }, states = items.filter { $0.name == "state" }
        guard codes.count == 1, let code = codes[0].value, !code.isEmpty,
              states.count == 1, let state = states[0].value else { throw AuthError.invalidRedirect }
        guard state == attempt.state else { throw AuthError.stateMismatch }
        return code
    }
    private func checkedSend(_ request: URLRequest, generation expected: UUID) async throws -> Data {
        do {
            let (data, status) = try await transport.send(request)
            guard expected == generation, !Task.isCancelled else { throw AuthError.cancelled }
            guard status == 200 else { throw AuthError.rejected }
            guard data.count <= 65536 else { throw AuthError.invalidResponse }
            return data
        } catch let error as AuthError { throw error }
        catch {
            if expected != generation || Task.isCancelled { throw AuthError.cancelled }
            throw AuthError.network // Never expose URLSession errors with token-bearing URLs.
        }
    }
    func finish(_ redirect: String) async throws -> Account {
        guard let attempt else { throw AuthError.noActiveAttempt }
        guard now() < attempt.expires else { self.attempt = nil; throw AuthError.expiredAttempt }
        let code = try code(from: redirect, matching: attempt)
        self.attempt = nil // A valid authorization code is consumed only once, even on network failure.
        var request = URLRequest(url: URL(string: Self.tokenURL)!)
        request.httpMethod = "POST"
        request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        let basic = Data("\(Self.clientID):\(Self.clientCredential)".utf8).base64EncodedString()
        request.setValue("Basic " + basic, forHTTPHeaderField: "Authorization")
        let allowed = CharacterSet(charactersIn: "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~")
        let fields = ["grant_type": "authorization_code", "code": code, "scope": Self.scope, "redirect_uri": Self.redirect]
        request.httpBody = Data(fields.sorted(by: { $0.key < $1.key }).map {
            $0.key + "=" + $0.value.addingPercentEncoding(withAllowedCharacters: allowed)!
        }.joined(separator: "&").utf8)
        let response = try await checkedSend(request, generation: attempt.id)
        struct Token: Decodable { let access_token: String }
        guard let token = try? JSONDecoder().decode(Token.self, from: response),
              !token.access_token.isEmpty, token.access_token.utf8.count <= 8192,
              let encoded = token.access_token.addingPercentEncoding(withAllowedCharacters: allowed)
        else { throw AuthError.invalidResponse }
        var identityRequest = URLRequest(url: URL(string: Self.tokenURL + "/" + encoded)!)
        identityRequest.setValue("Basic " + basic, forHTTPHeaderField: "Authorization")
        let identity = try await checkedSend(identityRequest, generation: attempt.id)
        struct Identity: Decodable {
            let user_id: String
            enum CodingKeys: String, CodingKey { case user_id }
            init(from decoder: Decoder) throws {
                let values = try decoder.container(keyedBy: CodingKeys.self)
                if let string = try? values.decode(String.self, forKey: .user_id) { user_id = string }
                else { user_id = String(try values.decode(UInt64.self, forKey: .user_id)) }
            }
        }
        guard let user = try? JSONDecoder().decode(Identity.self, from: identity) else { throw AuthError.invalidResponse }
        return try Account(decimalID: user.user_id)
    }
}

private final class RejectAuthRedirects: NSObject, URLSessionTaskDelegate {
    func urlSession(_ session: URLSession, task: URLSessionTask,
                    willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest request: URLRequest,
                    completionHandler: @escaping (URLRequest?) -> Void) { completionHandler(nil) }
}

@MainActor
final class URLSessionAuthTransport: AuthTransport {
    private let session: URLSession
    init() {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.urlCache = nil
        configuration.httpCookieStorage = nil
        configuration.httpShouldSetCookies = false
        configuration.requestCachePolicy = .reloadIgnoringLocalCacheData
        configuration.timeoutIntervalForRequest = 30
        configuration.timeoutIntervalForResource = 60
        session = URLSession(configuration: configuration, delegate: RejectAuthRedirects(), delegateQueue: nil)
    }
    func send(_ request: URLRequest) async throws -> (Data, Int) {
        let (bytes, response) = try await session.bytes(for: request)
        guard let http = response as? HTTPURLResponse else { throw AuthError.invalidResponse }
        guard http.statusCode == 200 else { return (Data(), http.statusCode) }
        var data = Data()
        for try await byte in bytes {
            guard data.count < 65536 else { throw AuthError.invalidResponse }
            data.append(byte)
        }
        return (data, http.statusCode)
    }
    deinit { session.invalidateAndCancel() }
}
