import XCTest
import Security
@testable import FreePS5CaptureCard

@MainActor
final class FakeKeychainAccess: KeychainAccess {
    var data: Data?
    var denied = false
    func read() -> (OSStatus, Data?) { denied ? (errSecInteractionNotAllowed, nil) : (data == nil ? errSecItemNotFound : errSecSuccess, data) }
    func update(_ value: Data) -> OSStatus {
        if denied { return errSecInteractionNotAllowed }
        if data == nil { return errSecItemNotFound }
        data = value; return errSecSuccess
    }
    func add(_ value: Data) -> OSStatus {
        if denied { return errSecInteractionNotAllowed }
        data = value; return errSecSuccess
    }
    func delete() -> OSStatus {
        if denied { return errSecInteractionNotAllowed }
        data = nil; return errSecSuccess
    }
}

final class CredentialStoreTests: XCTestCase {
    @MainActor func testAccountPersistsAndCanBeForgotten() throws {
        let access = FakeKeychainAccess()
        let store = KeychainCredentialStore(access: access)
        XCTAssertNil(try store.loadAccount())
        let account = try Account(decimalID: "42")
        try store.saveAccount(account)
        XCTAssertEqual(try store.loadAccount(), account)
        try store.forgetAccount()
        XCTAssertNil(try store.loadAccount())
    }
    @MainActor func testDeniedStoreNeverFallsBackToPlaintext() throws {
        let access = FakeKeychainAccess()
        access.denied = true
        let store = KeychainCredentialStore(access: access)
        XCTAssertThrowsError(try store.loadAccount())
        XCTAssertThrowsError(try store.saveAccount(Account(decimalID: "42")))
        XCTAssertThrowsError(try store.forgetAccount())
        XCTAssertNil(access.data)
    }
    @MainActor func testCorruptedAccountRecordIsRejected() {
        let access = FakeKeychainAccess()
        access.data = Data(#"{"accountID":"AA=="}"#.utf8)
        XCTAssertThrowsError(try KeychainCredentialStore(access: access).loadAccount())
    }
}
