import XCTest
import Darwin
@testable import FreePS5CaptureCard

@MainActor
final class FakeCredentialFile: CredentialFileAccess {
    var data: Data?
    var denied = false
    var denyUpdates = false
    var removeOnNextUpdate = false
    var writeAttempts = 0
    func read() throws -> Data? {
        if denied { throw AuthError.storage }
        return data
    }
    func write(_ value: Data, replacing expected: Data?) throws {
        writeAttempts += 1
        if denied || denyUpdates { throw AuthError.storage }
        if removeOnNextUpdate { removeOnNextUpdate = false; data = nil }
        guard data == expected else { throw AuthError.storage }
        data = value
    }
    func delete(replacing expected: Data?) throws {
        if denied || data != expected { throw AuthError.storage }
        data = nil
    }
}

@MainActor
final class CredentialStoreTests: XCTestCase {
    private func withFile(_ body: (PrivateCredentialFile, URL) throws -> Void) throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("capture-credentials-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: false)
        defer { try? FileManager.default.removeItem(at: root) }
        try body(PrivateCredentialFile(directory: root.appendingPathComponent("private", isDirectory: true)), root)
    }
    private func console(_ account: Account) -> PairedConsole {
        PairedConsole(accountID: account.accountID, address: "192.168.1.20", mac: Data([1,2,3,4,5,6]), name: "PS5",
                      target: 1000100, registrationKey: Data(repeating: 2, count: 16), sessionKey: Data(repeating: 3, count: 16), keyType: 1)
    }
    private func mode(_ path: URL) throws -> Int {
        let attributes = try FileManager.default.attributesOfItem(atPath: path.path)
        return try XCTUnwrap(attributes[.posixPermissions] as? NSNumber).intValue
    }
    func testAccountPersistsAndCanBeForgotten() throws {
        let access = FakeCredentialFile()
        let subject = FileCredentialStore(access: access)
        XCTAssertNil(try subject.loadAccount())
        let account = try Account(decimalID: "42")
        try subject.saveAccount(account); XCTAssertEqual(try subject.loadAccount(), account)
        try subject.forgetAccount(); XCTAssertNil(try subject.loadAccount()); XCTAssertNil(access.data)
    }
    func testDeniedStoreNeverFallsBackElsewhere() throws {
        let access = FakeCredentialFile(); access.denied = true
        let store = FileCredentialStore(access: access)
        XCTAssertThrowsError(try store.loadAccount())
        XCTAssertThrowsError(try store.saveAccount(Account(decimalID: "42")))
        XCTAssertThrowsError(try store.forgetAccount())
        XCTAssertNil(access.data)
    }
    func testAccountReplacementKeepsPairingOnlyForTheSameAccount() throws {
        let file = FakeCredentialFile()
        let subject = FileCredentialStore(access: file), first = try Account(decimalID: "42"), other = try Account(decimalID: "43")
        try subject.saveAccount(first); try subject.savePairing(console(first)); try subject.saveAccount(first)
        XCTAssertEqual(try subject.loadPairing(), console(first))
        try subject.saveAccount(other); XCTAssertEqual(try subject.loadAccount(), other); XCTAssertNil(try subject.loadPairing())
        file.denyUpdates = true
        XCTAssertThrowsError(try subject.saveAccount(first))
        XCTAssertEqual(try subject.loadAccount(), other); XCTAssertNil(try subject.loadPairing())
    }
    func testCorruptedAccountRecordIsRejectedWithoutOverwriting() throws {
        let access = FakeCredentialFile(); access.data = Data(#"{"accountID":"AA=="}"#.utf8)
        let original = access.data, store = FileCredentialStore(access: access)
        XCTAssertThrowsError(try store.loadAccount())
        XCTAssertThrowsError(try store.saveAccount(Account(decimalID: "42")))
        XCTAssertEqual(access.data, original)
        try store.forgetAccount(); XCTAssertNil(access.data)
    }
    func testDiskPersistenceUsesPrivateModesAndOnlyExpectedFields() throws {
        try withFile { file, _ in
            let account = try Account(decimalID: "42"), paired = console(account)
            let store = FileCredentialStore(access: file)
            try store.saveAccount(account); try store.savePairing(paired)
            let restored = FileCredentialStore(access: PrivateCredentialFile(directory: file.directory))
            XCTAssertEqual(try restored.loadAccount(), account); XCTAssertEqual(try restored.loadPairing(), paired)
            XCTAssertEqual(try mode(file.directory), 0o700)
            XCTAssertEqual(try mode(file.directory.appendingPathComponent(PrivateCredentialFile.fileName)), 0o600)
            XCTAssertEqual(try mode(file.directory.appendingPathComponent(PrivateCredentialFile.lockFileName)), 0o600)
            let bytes = try XCTUnwrap(file.read())
            let object = try XCTUnwrap(JSONSerialization.jsonObject(with: bytes) as? [String: Any])
            XCTAssertEqual(Set(object.keys), ["version", "account", "console"])
            let fields = try XCTUnwrap(object["console"] as? [String: Any])
            XCTAssertEqual(Set(fields.keys), ["accountID", "address", "mac", "name", "target", "registrationKey", "sessionKey", "keyType"])
            XCTAssertEqual(Set(try FileManager.default.contentsOfDirectory(atPath: file.directory.path)),
                           [PrivateCredentialFile.fileName, PrivateCredentialFile.lockFileName])
        }
    }
    func testDiskForgetPairingRetainsAccountAndForgetAccountRemovesFile() throws {
        try withFile { file, _ in
            let account = try Account(decimalID: "42"), store = FileCredentialStore(access: file)
            try store.saveAccount(account); try store.savePairing(console(account))
            try store.forgetPairing(); XCTAssertEqual(try store.loadAccount(), account); XCTAssertNil(try store.loadPairing())
            try store.savePairing(console(account)); try store.forgetAccount()
            XCTAssertNil(try file.read())
            XCTAssertFalse(FileManager.default.fileExists(atPath: file.directory.appendingPathComponent(PrivateCredentialFile.fileName).path))
        }
    }
    func testFileRejectsStaleWritesAndDeletionWithoutResurrection() throws {
        try withFile { file, _ in
            let first = Data("first".utf8), next = Data("next".utf8)
            try file.write(first, replacing: nil)
            try PrivateCredentialFile(directory: file.directory).write(next, replacing: first)
            XCTAssertThrowsError(try file.write(first, replacing: first))
            XCTAssertThrowsError(try file.delete(replacing: first))
            XCTAssertEqual(try file.read(), next)
            try file.delete(replacing: next)
            XCTAssertThrowsError(try file.write(first, replacing: next))
            XCTAssertNil(try file.read())
        }
    }
    func testFileRejectsWorldReadableCredentialsAndPublicDirectory() throws {
        try withFile { file, _ in
            let bytes = Data("sample".utf8); try file.write(bytes, replacing: nil)
            let path = file.directory.appendingPathComponent(PrivateCredentialFile.fileName)
            try FileManager.default.setAttributes([.posixPermissions: 0o644], ofItemAtPath: path.path)
            XCTAssertThrowsError(try file.read()); XCTAssertThrowsError(try file.write(Data("new".utf8), replacing: bytes))
            try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: path.path)
            try FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: file.directory.path)
            XCTAssertThrowsError(try file.read())
            try FileManager.default.setAttributes([.posixPermissions: 0o700], ofItemAtPath: file.directory.path)
            XCTAssertEqual(try file.read(), bytes)
        }
    }
    func testFileRejectsCredentialSymlinkWithoutChangingTarget() throws {
        try withFile { file, root in
            let bytes = Data("sample".utf8); try file.write(bytes, replacing: nil)
            let path = file.directory.appendingPathComponent(PrivateCredentialFile.fileName), other = root.appendingPathComponent("other")
            try bytes.write(to: other); try FileManager.default.removeItem(at: path)
            try FileManager.default.createSymbolicLink(at: path, withDestinationURL: other)
            XCTAssertThrowsError(try file.read()); XCTAssertThrowsError(try file.write(Data("new".utf8), replacing: nil))
            XCTAssertEqual(try Data(contentsOf: other), bytes)
        }
    }
    func testFileRejectsDirectorySymlinkWithoutWritingTarget() throws {
        try withFile { file, root in
            let other = root.appendingPathComponent("other", isDirectory: true)
            try FileManager.default.createDirectory(at: other, withIntermediateDirectories: false, attributes: [.posixPermissions: 0o700])
            try FileManager.default.createSymbolicLink(at: file.directory, withDestinationURL: other)
            XCTAssertThrowsError(try file.read()); XCTAssertThrowsError(try file.write(Data("sample".utf8), replacing: nil))
            XCTAssertTrue(try FileManager.default.contentsOfDirectory(atPath: other.path).isEmpty)
        }
    }
    func testFileRejectsHardlinkedCredentials() throws {
        try withFile { file, root in
            let bytes = Data("sample".utf8); try file.write(bytes, replacing: nil)
            let path = file.directory.appendingPathComponent(PrivateCredentialFile.fileName), other = root.appendingPathComponent("other")
            try FileManager.default.linkItem(at: path, to: other)
            XCTAssertThrowsError(try file.read()); XCTAssertThrowsError(try file.write(Data("new".utf8), replacing: bytes))
            XCTAssertEqual(try Data(contentsOf: other), bytes)
        }
    }
    func testFileRejectsOversizedAndUnsupportedRecords() throws {
        try withFile { file, _ in
            let bytes = Data("sample".utf8); try file.write(bytes, replacing: nil)
            let path = file.directory.appendingPathComponent(PrivateCredentialFile.fileName)
            try Data(repeating: 65, count: PrivateCredentialFile.maxBytes + 1).write(to: path)
            XCTAssertThrowsError(try file.read())
            try FileManager.default.removeItem(at: path)
            let account = try Account(decimalID: "42")
            let accountJSON = try JSONSerialization.jsonObject(with: JSONEncoder().encode(account))
            for version in [2, 999] {
                let unsupported = try JSONSerialization.data(withJSONObject: ["version": version, "account": accountJSON])
                try file.write(unsupported, replacing: nil)
                let store = FileCredentialStore(access: file)
                XCTAssertThrowsError(try store.loadAccount()); XCTAssertThrowsError(try store.saveAccount(account))
                XCTAssertEqual(try file.read(), unsupported)
                try file.delete(replacing: unsupported)
            }
            XCTAssertThrowsError(try file.write(Data(repeating: 65, count: PrivateCredentialFile.maxBytes + 1), replacing: nil))
        }
    }
    func testFileRejectsNonRegularCredentialsWithoutBlocking() throws {
        try withFile { file, _ in
            let bytes = Data("sample".utf8); try file.write(bytes, replacing: nil)
            let path = file.directory.appendingPathComponent(PrivateCredentialFile.fileName)
            try FileManager.default.removeItem(at: path)
            XCTAssertEqual(mkfifo(path.path, 0o600), 0)
            XCTAssertThrowsError(try file.read()); XCTAssertThrowsError(try file.write(bytes, replacing: nil))
            XCTAssertThrowsError(try file.delete(replacing: nil))
        }
    }
    func testUnsafeLockPermissionsAreRejectedWithoutChangingCredentials() throws {
        try withFile { file, _ in
            let bytes = Data("sample".utf8); try file.write(bytes, replacing: nil)
            let lock = file.directory.appendingPathComponent(PrivateCredentialFile.lockFileName)
            try FileManager.default.setAttributes([.posixPermissions: 0o644], ofItemAtPath: lock.path)
            XCTAssertThrowsError(try file.read()); XCTAssertThrowsError(try file.write(Data("new".utf8), replacing: bytes))
            try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: lock.path)
            XCTAssertEqual(try file.read(), bytes)
        }
    }
    func testContendedLockFailsInsteadOfWaitingOrOverwriting() throws {
        try withFile { file, _ in
            let bytes = Data("sample".utf8); try file.write(bytes, replacing: nil)
            let fd = Darwin.open(file.directory.appendingPathComponent(PrivateCredentialFile.lockFileName).path, O_RDWR | O_CLOEXEC)
            XCTAssertGreaterThanOrEqual(fd, 0); guard fd >= 0 else { return }
            defer { _ = flock(fd, LOCK_UN); _ = Darwin.close(fd) }
            XCTAssertEqual(flock(fd, LOCK_EX | LOCK_NB), 0)
            XCTAssertThrowsError(try file.read()); XCTAssertThrowsError(try file.write(Data("new".utf8), replacing: bytes))
            XCTAssertEqual(flock(fd, LOCK_UN), 0)
            XCTAssertEqual(try file.read(), bytes)
        }
    }
}
