import XCTest
@testable import FreePS5CaptureCard

final class AccountModelTests: XCTestCase {
    @MainActor func testRestoresPersistedAccountWithoutNetwork() throws {
        let store = FileCredentialStore(access: FakeCredentialFile())
        let account = try Account(decimalID: "42")
        try store.saveAccount(account)
        let model = AccountModel(service: PSNAuthService(transport: StubAuthTransport()), store: store)
        model.restore()
        XCTAssertEqual(model.account, account)
    }
    @MainActor func testCancelClearsPendingLogin() {
        let model = AccountModel(service: PSNAuthService(transport: StubAuthTransport()),
                                 store: FileCredentialStore(access: FakeCredentialFile()))
        _ = model.begin()
        XCTAssertTrue(model.awaitingRedirect)
        model.cancel()
        XCTAssertFalse(model.awaitingRedirect)
        XCTAssertFalse(model.busy)
    }
    @MainActor func testFileFailureDoesNotClaimSavedSignIn() async {
        let transport = StubAuthTransport()
        transport.responses = [(Data(#"{"access_token":"private-token"}"#.utf8), 200),
                               (Data(#"{"user_id":"42"}"#.utf8), 200)]
        let file = FakeCredentialFile()
        file.denied = true
        let model = AccountModel(service: PSNAuthService(transport: transport),
                                 store: FileCredentialStore(access: file))
        let url = model.begin()
        let state = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems?
            .first(where: { $0.name == "state" })?.value ?? "missing"
        await model.complete(PSNAuthService.redirect + "?code=private-code&state=" + state)
        XCTAssertNil(model.account)
        XCTAssertEqual(model.message, AuthError.storage.localizedDescription)
        XCTAssertFalse(model.busy)
    }
    @MainActor func testSuccessfulLoginPersistsIdentityButNotOAuthSecrets() async throws {
        let transport = StubAuthTransport()
        transport.responses = [(Data(#"{"access_token":"private-token","refresh_token":"private-refresh"}"#.utf8), 200),
                               (Data(#"{"user_id":"42"}"#.utf8), 200)]
        let file = FakeCredentialFile()
        let model = AccountModel(service: PSNAuthService(transport: transport), store: FileCredentialStore(access: file))
        let url = model.begin()
        let state = try XCTUnwrap(URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems?.first(where: { $0.name == "state" })?.value)
        await model.complete(PSNAuthService.redirect + "?code=private-code&state=" + state)
        XCTAssertEqual(model.account, try Account(decimalID: "42"))
        let saved = String(decoding: try XCTUnwrap(file.data), as: UTF8.self)
        for secret in ["private-token", "private-refresh", "private-code", state, "access_token", "refresh_token"] {
            XCTAssertFalse(saved.contains(secret))
        }
    }
    @MainActor func testExplicitResetCanRemoveCorruptLocalRecord() {
        let file = FakeCredentialFile(); file.data = Data("invalid JSON".utf8)
        let model = AccountModel(service: PSNAuthService(transport: StubAuthTransport()), store: FileCredentialStore(access: file))
        model.restore(); XCTAssertEqual(model.message, AuthError.storage.localizedDescription)
        model.forget(); XCTAssertNil(file.data); XCTAssertNil(model.account); XCTAssertNil(model.message)
    }
    @MainActor func testForgetRemovesSavedIdentity() throws {
        let store = FileCredentialStore(access: FakeCredentialFile())
        try store.saveAccount(Account(decimalID: "42"))
        let model = AccountModel(service: PSNAuthService(transport: StubAuthTransport()), store: store)
        model.restore()
        model.forget()
        XCTAssertNil(try store.loadAccount())
        XCTAssertNil(model.account)
    }
}
