import Foundation
import Security

@MainActor
protocol CredentialStore {
    func loadAccount() throws -> Account?
    func saveAccount(_ account: Account) throws
    func forgetAccount() throws
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
    func loadAccount() throws -> Account? {
        let (status, data) = access.read()
        if status == errSecItemNotFound { return nil }
        guard status == errSecSuccess, let data,
              let account = try? JSONDecoder().decode(Account.self, from: data) else { throw AuthError.storage }
        return account
    }
    func saveAccount(_ account: Account) throws {
        let data = try JSONEncoder().encode(account)
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
