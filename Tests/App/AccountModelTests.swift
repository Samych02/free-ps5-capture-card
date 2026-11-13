import XCTest
@testable import FreePS5CaptureCard

final class AccountModelTests: XCTestCase {
    @MainActor func testRestoresPersistedAccountWithoutNetwork() throws {
        let store = KeychainCredentialStore(access: FakeKeychainAccess())
        let account = try Account(decimalID: "42")
        try store.saveAccount(account)
        let model = AccountModel(service: PSNAuthService(transport: StubAuthTransport()), store: store)
        model.restore()
        XCTAssertEqual(model.account, account)
    }
    @MainActor func testCancelClearsPendingLogin() {
        let model = AccountModel(service: PSNAuthService(transport: StubAuthTransport()),
                                 store: KeychainCredentialStore(access: FakeKeychainAccess()))
        _ = model.begin()
        XCTAssertTrue(model.awaitingRedirect)
        model.cancel()
        XCTAssertFalse(model.awaitingRedirect)
        XCTAssertFalse(model.busy)
    }
    @MainActor func testKeychainFailureDoesNotClaimSavedSignIn() async {
        let transport = StubAuthTransport()
        transport.responses = [(Data(#"{"access_token":"private-token"}"#.utf8), 200),
                               (Data(#"{"user_id":"42"}"#.utf8), 200)]
        let keychain = FakeKeychainAccess()
        keychain.denied = true
        let model = AccountModel(service: PSNAuthService(transport: transport),
                                 store: KeychainCredentialStore(access: keychain))
        let url = model.begin()
        let state = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems?
            .first(where: { $0.name == "state" })?.value ?? "missing"
        await model.complete(PSNAuthService.redirect + "?code=private-code&state=" + state)
        XCTAssertNil(model.account)
        XCTAssertEqual(model.message, AuthError.storage.localizedDescription)
        XCTAssertFalse(model.busy)
    }
    @MainActor func testForgetRemovesSavedIdentity() throws {
        let store = KeychainCredentialStore(access: FakeKeychainAccess())
        try store.saveAccount(Account(decimalID: "42"))
        let model = AccountModel(service: PSNAuthService(transport: StubAuthTransport()), store: store)
        model.restore()
        model.forget()
        XCTAssertNil(try store.loadAccount())
        XCTAssertNil(model.account)
    }
}
