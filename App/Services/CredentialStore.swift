import Foundation

@MainActor
protocol CredentialStore {
    func loadAccount() throws -> Account?
    func saveAccount(_ account: Account) throws
    func forgetAccount() throws
    func loadPairing() throws -> PairedConsole?
    func savePairing(_ console: PairedConsole) throws
    func updatePairingAddress(_ address: String, expected: PairedConsole) throws -> PairedConsole
    func forgetPairing() throws
}

@MainActor
protocol CredentialFileAccess {
    func read() throws -> Data?
    func write(_ data: Data, replacing expected: Data?) throws
    func delete(replacing expected: Data?) throws
}

@MainActor
final class FileCredentialStore: CredentialStore {
    private let access: CredentialFileAccess
    init(access: CredentialFileAccess? = nil) { self.access = access ?? PrivateCredentialFile() }
    private struct Record: Codable {
        let version: Int
        let account: Account
        var console: PairedConsole?
    }
    private struct Snapshot {
        var record: Record
        let bytes: Data
    }
    private func load() throws -> Snapshot? {
        guard let data = try access.read() else { return nil }
        guard !data.isEmpty, data.count <= PrivateCredentialFile.maxBytes,
              let record = try? JSONDecoder().decode(Record.self, from: data), record.version == 1 else { throw AuthError.storage }
        if let console = record.console {
            guard (try? console.validate(for: record.account)) != nil else { throw AuthError.storage }
        }
        return Snapshot(record: record, bytes: data)
    }
    private func save(_ record: Record, replacing expected: Data?) throws {
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
        guard let data = try? encoder.encode(record), data.count <= PrivateCredentialFile.maxBytes else { throw AuthError.storage }
        do { try access.write(data, replacing: expected) } catch { throw AuthError.storage }
    }
    func loadAccount() throws -> Account? { try load()?.record.account }
    func loadPairing() throws -> PairedConsole? { try load()?.record.console }
    func saveAccount(_ account: Account) throws {
        let old = try load()
        let console = old?.record.account == account ? old?.record.console : nil
        try save(Record(version: 1, account: account, console: console), replacing: old?.bytes)
    }
    func savePairing(_ console: PairedConsole) throws {
        guard var old = try load() else { throw PairingError.identity }
        try console.validate(for: old.record.account)
        old.record.console = console
        try save(old.record, replacing: old.bytes)
    }
    func updatePairingAddress(_ address: String, expected: PairedConsole) throws -> PairedConsole {
        guard var old = try load(), old.record.console == expected else { throw PairingError.pairingChanged }
        let updated = try expected.replacingAddress(address)
        try updated.validate(for: old.record.account)
        if updated != expected {
            old.record.console = updated
            try save(old.record, replacing: old.bytes)
        }
        return updated
    }
    func forgetPairing() throws {
        guard var old = try load(), old.record.console != nil else { return }
        old.record.console = nil
        try save(old.record, replacing: old.bytes)
    }
    func forgetAccount() throws {
        // Explicit Forget also permits removal of bounded, malformed JSON.
        let bytes = try access.read()
        do { try access.delete(replacing: bytes) } catch { throw AuthError.storage }
    }
}
