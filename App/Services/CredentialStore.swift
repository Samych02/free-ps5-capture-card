import Foundation
import Security

@MainActor
protocol CredentialStore {
    func loadAccount() throws -> Account?
    func saveAccount(_ account: Account) throws
    func forgetAccount() throws
    func loadPairing() throws -> PairedConsole?
    func savePairing(_ console: PairedConsole) throws
    func forgetPairing() throws
}

@MainActor
protocol KeychainAccess {
    func read() -> (OSStatus, Data?)
    func update(_ data: Data) -> OSStatus
    func add(_ data: Data) -> OSStatus
    func delete() -> OSStatus
}

@MainActor
final class KeychainCredentialStore: CredentialStore {
    private let access: KeychainAccess
    init(access: KeychainAccess? = nil) { self.access = access ?? SystemAccountKeychain() }
    private struct Record: Codable {
        let version: Int
        let account: Account
        var console: PairedConsole?
    }
    private func load() throws -> Record? {
        let (status, data) = access.read()
        if status == errSecItemNotFound { return nil }
        guard status == errSecSuccess, let data else { throw AuthError.storage }
        if let record = try? JSONDecoder().decode(Record.self, from: data), record.version == 2 {
            if let console = record.console {
                guard (try? console.validate(for: record.account)) != nil else { throw AuthError.storage }
            }
            return record
        }
        // Existing account-only Keychain records migrate on the next write.
        guard let account = try? JSONDecoder().decode(Account.self, from: data) else { throw AuthError.storage }
        return Record(version: 2, account: account, console: nil)
    }
    func loadAccount() throws -> Account? { try load()?.account }
    func loadPairing() throws -> PairedConsole? { try load()?.console }
    func savePairing(_ console: PairedConsole) throws {
        guard var record = try load() else { throw PairingError.identity }
        try console.validate(for: record.account)
        record.console = console
        try save(record)
    }
    func forgetPairing() throws {
        guard var record = try load() else { return }
        record.console = nil
        try save(record)
    }
    func saveAccount(_ account: Account) throws {
        let old = try load()
        try save(Record(version: 2, account: account, console: old?.account == account ? old?.console : nil))
    }
    private func save(_ record: Record) throws {
        let data = try JSONEncoder().encode(record)
        let status = access.update(data)
        if status == errSecItemNotFound {
            guard access.add(data) == errSecSuccess else { throw AuthError.storage }
        } else if status != errSecSuccess { throw AuthError.storage }
    }
    func forgetAccount() throws {
        let status = access.delete()
        guard status == errSecSuccess || status == errSecItemNotFound else { throw AuthError.storage }
    }
}

@MainActor
private final class SystemAccountKeychain: KeychainAccess {
    private var query: [String: Any] {
        [kSecClass as String: kSecClassGenericPassword,
         kSecAttrService as String: "org.freeps5capturecard.account",
         kSecAttrAccount as String: "psn-identity-v1",
         kSecAttrSynchronizable as String: false]
    }
    func read() -> (OSStatus, Data?) {
        var request = query
        request[kSecReturnData as String] = true
        request[kSecMatchLimit as String] = kSecMatchLimitOne
        var value: CFTypeRef?
        let status = SecItemCopyMatching(request as CFDictionary, &value)
        return (status, value as? Data)
    }
    func update(_ data: Data) -> OSStatus {
        SecItemUpdate(query as CFDictionary, [kSecValueData as String: data] as CFDictionary)
    }
    func add(_ data: Data) -> OSStatus {
        var request = query
        request[kSecValueData as String] = data
        request[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        return SecItemAdd(request as CFDictionary, nil)
    }
    func delete() -> OSStatus { SecItemDelete(query as CFDictionary) }
}
