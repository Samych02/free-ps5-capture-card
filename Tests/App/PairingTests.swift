import XCTest
import CaptureEngine
@testable import FreePS5CaptureCard

@MainActor final class FakeConsoleService: ConsoleServicing {
    var discovery: ((Result<[DiscoveredConsole], PairingError>) -> Void)?
    var pending: CheckedContinuation<PairedConsole, Error>?
    var requests: [PairingRequest] = []
    var cancellations = 0
    func discover(_ handler: @escaping (Result<[DiscoveredConsole], PairingError>) -> Void) { discovery = handler }
    func stopDiscovery() async {}
    func pair(_ request: PairingRequest) async throws -> PairedConsole {
        requests.append(request)
        return try await withCheckedThrowingContinuation { pending = $0 }
    }
    func cancelPairing() { cancellations += 1 }
    func shutdown() async {}
}
final class PairingTests: XCTestCase {
    @MainActor func testShutdownWaitsForAlreadyStartedNativeJoin() async throws {
        let started = expectation(description: "pairing created")
        let joining = expectation(description: "native join started")
        let releaseJoin = DispatchSemaphore(value: 0)
        var callback: capture_pairing_callback?
        var context: UnsafeMutableRawPointer?
        var api = NativePairingAPI()
        api.create = { _, cb, ctx, _ in callback = cb; context = ctx; started.fulfill(); return OpaquePointer(bitPattern: 1) }
        api.stop = { _ in }
        api.destroy = { _ in joining.fulfill(); _ = releaseJoin.wait(timeout: .now() + 5); return CAPTURE_OK }
        let service = NativeConsoleService(pairingAPI: api)
        let request = try PairingRequest(address: "192.168.1.20", pin: "01234567", account: Account(decimalID: "42"))
        let pairing = Task { try? await service.pair(request) }
        await fulfillment(of: [started], timeout: 2)
        callback?(CAPTURE_SETUP_CANCELLED, nil, context)
        await fulfillment(of: [joining], timeout: 2)
        var shutdownReturned = false
        let shutdown = Task { await service.shutdown(); shutdownReturned = true }
        for _ in 0..<10 { await Task.yield() }
        XCTAssertFalse(shutdownReturned, "Shutdown returned while the native join still owned its callback context")
        releaseJoin.signal()
        await shutdown.value; _ = await pairing.value
    }
    @MainActor func testNativeOptionsCarryExactPairingIdentity() throws {
        let request = try PairingRequest(address: "192.168.1.20", pin: "01234567", account: Account(decimalID: "42"))
        var options = NativeConsoleService.options(request)
        XCTAssertEqual(capture_validate_pairing(&options), CAPTURE_OK)
        XCTAssertEqual(withUnsafeBytes(of: options.account_id) { Data($0) }, request.account.accountID)
        XCTAssertEqual(withUnsafeBytes(of: options.pin) { String(decoding: $0.prefix(8), as: UTF8.self) }, "01234567")
    }
    @MainActor func testNativeRegistrationCopiesCredentialBytes() throws {
        let request = try PairingRequest(address: "192.168.1.20", pin: "01234567", account: Account(decimalID: "42"))
        var native = capture_registration(); native.target = 1000100; native.key_type = 7
        withUnsafeMutableBytes(of: &native.mac) { $0.copyBytes(from: [1,2,3,4,5,6]) }
        withUnsafeMutableBytes(of: &native.registration_key) { $0.copyBytes(from: Array(repeating: UInt8(2), count: 16)) }
        withUnsafeMutableBytes(of: &native.session_key) { $0.copyBytes(from: Array(repeating: UInt8(3), count: 16)) }
        let result = NativeConsoleService.registration(native, request: request)
        native = capture_registration()
        try result.validate(for: request.account)
        XCTAssertEqual(result.id, "010203040506")
        XCTAssertEqual(result.address, request.address)
        XCTAssertEqual(result.registrationKey, Data(repeating: 2, count: 16))
        XCTAssertEqual(result.sessionKey, Data(repeating: 3, count: 16))
        XCTAssertEqual(result.keyType, 7)
    }
    private func console(_ account: Account, address: String = "192.168.1.20") -> PairedConsole {
        PairedConsole(accountID: account.accountID, address: address, mac: Data([1,2,3,4,5,6]),
                      name: "PS5", target: 1000100, registrationKey: Data(repeating: 2, count: 16),
                      sessionKey: Data(repeating: 3, count: 16), keyType: 1)
    }
    @MainActor func testAddressChangePreservesAllPairingFieldsAndRestores() throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let store = KeychainCredentialStore(access: FakeKeychainAccess())
        try store.saveAccount(account); try store.savePairing(original)
        let service = FakeConsoleService(), model = PairingModel(service: FakeConsoleService(), store: store)
        let subject = PairingModel(service: service, store: store); subject.setAccount(account)
        XCTAssertTrue(subject.updateAddress(" 192.168.1.35\n", expected: original))
        let expected = console(account, address: "192.168.1.35")
        XCTAssertEqual(subject.paired, expected)
        XCTAssertEqual(try store.loadPairing(), expected)
        XCTAssertEqual(try store.loadAccount(), account)
        XCTAssertTrue(service.requests.isEmpty)
        model.setAccount(account); XCTAssertEqual(model.paired, expected)
    }
    @MainActor func testAddressChangeAcceptsNormalizedIPv6() throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let store = KeychainCredentialStore(access: FakeKeychainAccess())
        try store.saveAccount(account); try store.savePairing(original)
        let model = PairingModel(service: FakeConsoleService(), store: store); model.setAccount(account)
        XCTAssertTrue(model.updateAddress(" 2001:DB8::12\n", expected: original))
        XCTAssertEqual(model.paired, console(account, address: "2001:db8::12"))
    }
    @MainActor func testInvalidAddressChangeNeverWritesOrLosesKeys() throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let access = FakeKeychainAccess(), service = FakeConsoleService()
        let store = KeychainCredentialStore(access: access)
        try store.saveAccount(account); try store.savePairing(original)
        let model = PairingModel(service: service, store: store); model.setAccount(account)
        let writes = access.writeAttempts
        for text in ["", "localhost", "http://192.168.1.35", "192.168.1.35:9295", "127.0.0.1", "::1", "192.168.1.35\0"] {
            XCTAssertFalse(model.updateAddress(text, expected: original))
            XCTAssertNotNil(model.message)
            XCTAssertEqual(model.paired, original)
        }
        XCTAssertEqual(access.writeAttempts, writes)
        XCTAssertEqual(try store.loadPairing(), original)
        XCTAssertTrue(service.requests.isEmpty)
    }
    @MainActor func testAddressSaveFailureRetainsOldRecordAndModel() throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let access = FakeKeychainAccess()
        let subjectStore = KeychainCredentialStore(access: access)
        try subjectStore.saveAccount(account); try subjectStore.savePairing(original)
        let model = PairingModel(service: FakeConsoleService(), store: subjectStore); model.setAccount(account)
        let data = access.data, writes = access.writeAttempts
        access.denyUpdates = true
        XCTAssertFalse(model.updateAddress("192.168.1.35", expected: original))
        XCTAssertEqual(access.writeAttempts, writes + 1)
        XCTAssertEqual(model.paired, original); XCTAssertEqual(access.data, data)
        XCTAssertNotNil(model.message)
    }
    @MainActor func testAddressEditNeverRecreatesDeletedKeychainItem() throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let access = FakeKeychainAccess(), service = FakeConsoleService()
        let store = KeychainCredentialStore(access: access)
        try store.saveAccount(account); try store.savePairing(original)
        let model = PairingModel(service: service, store: store); model.setAccount(account)
        let writes = access.writeAttempts
        access.removeOnNextUpdate = true
        XCTAssertFalse(model.updateAddress("192.168.1.35", expected: original))
        XCTAssertEqual(access.writeAttempts, writes + 1)
        XCTAssertNil(access.data)
        XCTAssertEqual(model.paired, original)
    }
    @MainActor func testStaleAddressSheetCannotRestoreForgottenOrReplacedPairing() throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let access = FakeKeychainAccess()
        let subjectStore = KeychainCredentialStore(access: access)
        try subjectStore.saveAccount(account); try subjectStore.savePairing(original)
        let model = PairingModel(service: FakeConsoleService(), store: subjectStore); model.setAccount(account)
        try subjectStore.forgetPairing()
        var writes = access.writeAttempts
        XCTAssertFalse(model.updateAddress("192.168.1.35", expected: original))
        XCTAssertEqual(access.writeAttempts, writes); XCTAssertNil(try subjectStore.loadPairing())
        let replacement = console(account, address: "192.168.1.99")
        try subjectStore.savePairing(replacement); writes = access.writeAttempts
        XCTAssertFalse(model.updateAddress("192.168.1.35", expected: original))
        XCTAssertEqual(access.writeAttempts, writes); XCTAssertEqual(try subjectStore.loadPairing(), replacement)
    }
    @MainActor func testChangedAccountRejectsOldAddressSheet() throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let store = KeychainCredentialStore(access: FakeKeychainAccess())
        try store.saveAccount(account); try store.savePairing(original)
        let model = PairingModel(service: FakeConsoleService(), store: store); model.setAccount(account)
        let other = try Account(decimalID: "43"); try store.saveAccount(other); model.setAccount(other)
        XCTAssertFalse(model.updateAddress("192.168.1.35", expected: original))
        XCTAssertNil(model.paired); XCTAssertNil(try store.loadPairing()); XCTAssertEqual(try store.loadAccount(), other)
    }
    @MainActor func testAddressChangeIsRejectedWhilePairing() async throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let store = KeychainCredentialStore(access: FakeKeychainAccess())
        try store.saveAccount(account); try store.savePairing(original)
        let service = FakeConsoleService()
        let subject = PairingModel(service: service, store: store); subject.setAccount(account)
        let task = Task { await subject.pair(address: "192.168.1.99", pin: "01234567") }
        await Task.yield(); XCTAssertTrue(subject.busy)
        XCTAssertFalse(subject.updateAddress("192.168.1.35", expected: original))
        XCTAssertEqual(try store.loadPairing(), original)
        subject.cancel(); service.pending?.resume(throwing: PairingError.cancelled); service.pending = nil
        await task.value
    }
    func testEightDigitPairingCodePreservesLeadingZero() throws {
        let account = try Account(decimalID: "42")
        XCTAssertEqual(try PairingRequest(address: "192.168.1.20", pin: "01234567", account: account).pin, "01234567")
        for pin in ["1234", "1234567", "123456789", "１２３４５６７８", "1234567 "] {
            XCTAssertThrowsError(try PairingRequest(address: "192.168.1.20", pin: pin, account: account))
        }
    }
    func testManualIPDoesNotRequireDiscovery() throws {
        let account = try Account(decimalID: "42")
        XCTAssertEqual(try PairingRequest(address: " 192.168.1.20\n", pin: "01234567", account: account).address, "192.168.1.20")
        for address in ["", "localhost", "http://192.168.1.20", "192.168.1.20:9295", "127.0.0.1", "255.255.255.255"] {
            XCTAssertThrowsError(try PairingRequest(address: address, pin: "01234567", account: account))
        }
    }
    @MainActor func testAccountRecordMigratesWithoutLosingIdentity() throws {
        let keychain = FakeKeychainAccess(); let account = try Account(decimalID: "42")
        keychain.data = try JSONEncoder().encode(account)
        let store = KeychainCredentialStore(access: keychain)
        XCTAssertEqual(try store.loadAccount(), account)
        let paired = console(account)
        try store.savePairing(paired)
        XCTAssertEqual(try store.loadPairing(), paired)
        try store.saveAccount(account)
        XCTAssertEqual(try store.loadPairing(), paired)
        try store.saveAccount(Account(decimalID: "43"))
        XCTAssertNil(try store.loadPairing())
    }
    @MainActor func testPairingCannotBeSavedUnderDifferentAccount() throws {
        let store = KeychainCredentialStore(access: FakeKeychainAccess())
        try store.saveAccount(Account(decimalID: "43"))
        XCTAssertThrowsError(try store.savePairing(console(Account(decimalID: "42"))))
        XCTAssertNil(try store.loadPairing())
    }
    @MainActor func testSuccessfulPairingIsRestoredFromStore() async throws {
        let account = try Account(decimalID: "42"), expected = console(account)
        let store = KeychainCredentialStore(access: FakeKeychainAccess()); try store.saveAccount(account)
        let service = FakeConsoleService(); let model = PairingModel(service: service, store: store)
        model.setAccount(account)
        let task = Task { await model.pair(address: expected.address, pin: "01234567") }
        await Task.yield()
        XCTAssertEqual(service.requests.count, 1)
        service.pending?.resume(returning: expected); service.pending = nil
        await task.value
        XCTAssertEqual(model.paired, expected)
        let restored = PairingModel(service: FakeConsoleService(), store: store); restored.setAccount(account)
        XCTAssertEqual(restored.paired, expected)
    }
    @MainActor func testCancelledReplacementCannotOverwriteSavedConsole() async throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let store = KeychainCredentialStore(access: FakeKeychainAccess()); try store.saveAccount(account); try store.savePairing(original)
        let service = FakeConsoleService(); let model = PairingModel(service: service, store: store); model.setAccount(account)
        let task = Task { await model.pair(address: "192.168.1.21", pin: "01234567") }
        await Task.yield(); model.cancel()
        service.pending?.resume(returning: console(account, address: "192.168.1.21")); service.pending = nil
        await task.value
        XCTAssertEqual(model.paired, original)
        XCTAssertEqual(try store.loadPairing(), original)
    }
    @MainActor func testDeniedSavePreservesOldPairing() async throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let access = FakeKeychainAccess(); let store = KeychainCredentialStore(access: access)
        try store.saveAccount(account); try store.savePairing(original)
        let service = FakeConsoleService(); let model = PairingModel(service: service, store: store); model.setAccount(account)
        let task = Task { await model.pair(address: "192.168.1.21", pin: "01234567") }
        await Task.yield(); access.denied = true
        service.pending?.resume(returning: console(account, address: "192.168.1.21")); service.pending = nil
        await task.value
        XCTAssertEqual(model.paired, original)
        XCTAssertNotNil(model.message)
        access.denied = false; XCTAssertEqual(try store.loadPairing(), original)
    }
    @MainActor func testDiscoveryPermissionErrorIsNotEmptyList() throws {
        let account = try Account(decimalID: "42")
        let service = FakeConsoleService(); let model = PairingModel(service: service, store: KeychainCredentialStore(access: FakeKeychainAccess()))
        model.setAccount(account); model.discover(); service.discovery?(.failure(.permission))
        XCTAssertEqual(model.message, PairingError.permission.localizedDescription)
    }
    @MainActor func testFailedReplacementKeepsExistingPairing() async throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let store = KeychainCredentialStore(access: FakeKeychainAccess()); try store.saveAccount(account); try store.savePairing(original)
        let service = FakeConsoleService(); let model = PairingModel(service: service, store: store); model.setAccount(account)
        let task = Task { await model.pair(address: "192.168.1.21", pin: "01234567") }
        await Task.yield()
        service.pending?.resume(throwing: PairingError.failed); service.pending = nil
        await task.value
        XCTAssertEqual(model.paired, original)
        XCTAssertEqual(try store.loadPairing(), original)
        XCTAssertEqual(model.message, PairingError.failed.localizedDescription)
    }
    @MainActor func testWrongDiscoveredMACCannotReplacePairing() async throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let store = KeychainCredentialStore(access: FakeKeychainAccess()); try store.saveAccount(account); try store.savePairing(original)
        let service = FakeConsoleService(); let model = PairingModel(service: service, store: store); model.setAccount(account)
        let task = Task { await model.pair(address: "192.168.1.21", pin: "01234567", expectedID: "ffffffffffff") }
        await Task.yield()
        service.pending?.resume(returning: console(account, address: "192.168.1.21")); service.pending = nil
        await task.value
        XCTAssertEqual(try store.loadPairing(), original)
        XCTAssertEqual(model.message, PairingError.identity.localizedDescription)
    }
    @MainActor func testAccountChangeRejectsLatePairingAndDiscovery() async throws {
        let account = try Account(decimalID: "42")
        let store = KeychainCredentialStore(access: FakeKeychainAccess()); try store.saveAccount(account)
        let service = FakeConsoleService(); let model = PairingModel(service: service, store: store); model.setAccount(account)
        model.discover(); let staleDiscovery = service.discovery
        let task = Task { await model.pair(address: "192.168.1.20", pin: "01234567") }
        await Task.yield()
        let other = try Account(decimalID: "43"); try store.saveAccount(other); model.setAccount(other)
        staleDiscovery?(.success([DiscoveredConsole(id: "old", address: "192.168.1.20", name: "Old", target: 1000100, standby: false)]))
        service.pending?.resume(returning: console(account)); service.pending = nil
        await task.value
        XCTAssertNil(model.paired); XCTAssertTrue(model.consoles.isEmpty)
        XCTAssertNil(try store.loadPairing())
    }
    @MainActor func testForgettingPS5RetainsAccountButForgettingAccountDeletesBoth() throws {
        let account = try Account(decimalID: "42"), original = console(account)
        let store = KeychainCredentialStore(access: FakeKeychainAccess()); try store.saveAccount(account); try store.savePairing(original)
        try store.forgetPairing()
        XCTAssertEqual(try store.loadAccount(), account); XCTAssertNil(try store.loadPairing())
        try store.savePairing(original); try store.forgetAccount()
        XCTAssertNil(try store.loadAccount()); XCTAssertNil(try store.loadPairing())
    }
    func testMissingCredentialPayloadIsRejected() throws {
        let account = try Account(decimalID: "42")
        let invalid = PairedConsole(accountID: account.accountID, address: "192.168.1.20", mac: Data([1,2,3,4,5,6]),
                                    name: "PS5", target: 1000100, registrationKey: Data(repeating: 0, count: 16),
                                    sessionKey: Data(repeating: 0, count: 16), keyType: 1)
        XCTAssertThrowsError(try invalid.validate(for: account))
    }
}
