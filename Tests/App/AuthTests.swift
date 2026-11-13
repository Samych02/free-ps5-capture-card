import XCTest
@testable import FreePS5CaptureCard

@MainActor
final class StubAuthTransport: AuthTransport {
    var requests: [URLRequest] = []
    var responses: [(Data, Int)] = []
    var beforeResponse: (() -> Void)?
    func send(_ request: URLRequest) async throws -> (Data, Int) {
        requests.append(request)
        beforeResponse?()
        guard !responses.isEmpty else { throw URLError(.notConnectedToInternet) }
        return responses.removeFirst()
    }
}

final class AuthTests: XCTestCase {
    @MainActor private func callback(_ service: PSNAuthService, code: String = "private-code") -> String {
        let url = service.begin()
        let state = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems?
            .first(where: { $0.name == "state" })?.value ?? "missing-state"
        var components = URLComponents(string: PSNAuthService.redirect)!
        components.queryItems = [URLQueryItem(name: "code", value: code), URLQueryItem(name: "state", value: state)]
        return components.url!.absoluteString
    }
    func testAccountIDConversionPreservesAll64Bits() throws {
        let account = try Account(decimalID: "18446744073709551615")
        XCTAssertEqual(account.accountID, Data(repeating: 255, count: 8))
        XCTAssertEqual(try Account(decimalID: "72623859790382856").accountID,
                       Data([8, 7, 6, 5, 4, 3, 2, 1]))
        for invalid in ["0", "-1", "1.5", "1e3", "18446744073709551616", " 1", ""] {
            XCTAssertThrowsError(try Account(decimalID: invalid))
        }
    }
    @MainActor func testBeginGeneratesFreshStateAndTrustedSonyURL() {
        let service = PSNAuthService(transport: StubAuthTransport())
        let a = service.begin(), b = service.begin()
        XCTAssertEqual(a.host, "auth.api.sonyentertainmentnetwork.com")
        XCTAssertNotEqual(a, b)
        XCTAssertTrue(a.absoluteString.contains("state="))
    }
    @MainActor func testUnstartedLoginRejectsRedirect() async {
        let service = PSNAuthService(transport: StubAuthTransport())
        do { _ = try await service.finish(PSNAuthService.redirect + "?code=x&state=y"); XCTFail() }
        catch { XCTAssertEqual(error as? AuthError, .noActiveAttempt) }
    }
    @MainActor func testRejectsWrongHostDuplicateCodeAndMismatchedState() async {
        let transport = StubAuthTransport()
        let service = PSNAuthService(transport: transport)
        let valid = callback(service)
        let invalid = [valid.replacingOccurrences(of: "remoteplay.dl.playstation.net", with: "remoteplay.dl.playstation.net.attacker.invalid"),
                       valid + "&code=second", valid + "&state=other",
                       valid.replacingOccurrences(of: "https:", with: "http:"),
                       valid.replacingOccurrences(of: "/remoteplay/redirect", with: "/remoteplay/redirect/extra"),
                       valid + "#fragment"]
        for url in invalid {
            do { _ = try await service.finish(url); XCTFail("Invalid callback accepted") }
            catch { XCTAssertTrue(error is AuthError) }
        }
        XCTAssertTrue(transport.requests.isEmpty)
    }
    @MainActor func testExpiredAttemptMakesNoHTTPRequest() async {
        var now = Date()
        let transport = StubAuthTransport()
        let service = PSNAuthService(transport: transport, now: { now })
        let url = callback(service)
        now += 601
        do { _ = try await service.finish(url); XCTFail() }
        catch { XCTAssertEqual(error as? AuthError, .expiredAttempt) }
        XCTAssertTrue(transport.requests.isEmpty)
    }
    @MainActor func testWrongStateIsRejectedBeforeTokenExchange() async {
        let transport = StubAuthTransport()
        let service = PSNAuthService(transport: transport)
        var url = URLComponents(string: callback(service))!
        url.queryItems = url.queryItems!.map { $0.name == "state" ? URLQueryItem(name: "state", value: "wrong") : $0 }
        do { _ = try await service.finish(url.url!.absoluteString); XCTFail() }
        catch { XCTAssertEqual(error as? AuthError, .stateMismatch) }
        XCTAssertTrue(transport.requests.isEmpty)
    }
    @MainActor func testProviderRejectionIsSanitizedAndConsumesCode() async {
        let transport = StubAuthTransport()
        transport.responses = [(Data(#"{"error_description":"private-token"}"#.utf8), 400)]
        let service = PSNAuthService(transport: transport)
        let url = callback(service)
        do { _ = try await service.finish(url); XCTFail() }
        catch {
            XCTAssertEqual(error as? AuthError, .rejected)
            XCTAssertFalse(error.localizedDescription.contains("private-token"))
        }
        do { _ = try await service.finish(url); XCTFail() }
        catch { XCTAssertEqual(error as? AuthError, .noActiveAttempt) }
    }
    @MainActor func testNumericAccountIDDoesNotRoundThroughFloatingPoint() async throws {
        let transport = StubAuthTransport()
        transport.responses = [(Data(#"{"access_token":"private-token"}"#.utf8), 200),
                               (Data(#"{"user_id":18446744073709551615}"#.utf8), 200)]
        let service = PSNAuthService(transport: transport)
        let url = callback(service)
        let identity = try await service.finish(url)
        XCTAssertEqual(identity.accountID, Data(repeating: 255, count: 8))
    }
    @MainActor func testExchangeEncodesFormAndConsumesCodeOnce() async throws {
        let transport = StubAuthTransport()
        transport.responses = [(Data(#"{"access_token":"private-token"}"#.utf8), 200),
                               (Data(#"{"user_id":"18446744073709551615"}"#.utf8), 200)]
        let service = PSNAuthService(transport: transport)
        let url = callback(service, code: "a+b&c=d")
        let account = try await service.finish(url)
        XCTAssertEqual(account.accountID.count, 8)
        let body = String(data: transport.requests[0].httpBody!, encoding: .utf8)!
        XCTAssertTrue(body.contains("code=a%2Bb%26c%3Dd"))
        XCTAssertEqual(transport.requests.count, 2)
        do { _ = try await service.finish(url); XCTFail() }
        catch { XCTAssertEqual(error as? AuthError, .noActiveAttempt) }
    }
    @MainActor func testCancellationDiscardsLateNetworkResponse() async {
        let transport = StubAuthTransport()
        transport.responses = [(Data(#"{"access_token":"private-token"}"#.utf8), 200)]
        let service = PSNAuthService(transport: transport)
        let url = callback(service)
        transport.beforeResponse = { service.cancel() }
        do { _ = try await service.finish(url); XCTFail() }
        catch { XCTAssertEqual(error as? AuthError, .cancelled) }
        XCTAssertEqual(transport.requests.count, 1)
    }
    @MainActor func testNetworkErrorsNeverExposeSensitiveURL() async {
        let service = PSNAuthService(transport: StubAuthTransport())
        let url = callback(service)
        do { _ = try await service.finish(url); XCTFail() }
        catch {
            XCTAssertEqual(error as? AuthError, .network)
            XCTAssertFalse(error.localizedDescription.contains("private-code"))
        }
    }
}
