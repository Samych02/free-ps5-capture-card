import Foundation
import Darwin

enum PairingError: Error, Equatable, LocalizedError {
    case address, pin, failed, cancelled, storage, identity, permission, pairingChanged
    var errorDescription: String? {
        switch self {
        case .address: return "Enter the PS5’s IP address, without a URL or port."
        case .pin: return "Enter the 8-digit code from PS5 Settings → System → Remote Play → Link Device. This is not the 4-digit login passcode."
        case .failed: return "Could not pair with the PS5. Enable Remote Play, open Link Device, and try a fresh code on the same network."
        case .cancelled: return "Pairing cancelled."
        case .storage: return "Could not save PS5 credentials to the local file. No replacement was saved."
        case .identity: return "The returned PS5 identity does not match this pairing request. No credentials were saved."
        case .pairingChanged: return "The saved PS5 or account changed, or pairing is in progress. Close Change IP and try again after pairing finishes."
        case .permission: return "Local network access was denied. Allow this app in System Settings → Privacy & Security → Local Network. You can also try the PS5’s IP address."
        }
    }
}
struct DiscoveredConsole: Identifiable, Equatable {
    let id: String
    let address: String
    let name: String
    let target: UInt32
    let standby: Bool
}
struct PairingRequest {
    let address: String
    let pin: String
    let account: Account
    let expectedID: String?
    init(address: String, pin: String, account: Account, expectedID: String? = nil) throws {
        self.address = try Self.validAddress(address)
        guard pin.utf8.count == 8, pin.utf8.allSatisfy({ (48...57).contains($0) }) else { throw PairingError.pin }
        self.pin = pin; self.account = account; self.expectedID = expectedID
    }
    static func validAddress(_ text: String) throws -> String {
        let text = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.utf8.contains(0) else { throw PairingError.address }
        var v4 = in_addr(), v6 = in6_addr()
        if inet_pton(AF_INET, text, &v4) == 1 {
            let ip = UInt32(bigEndian: v4.s_addr)
            guard ip != 0, ip != .max, ip >> 24 != 127, ip >> 28 < 14 else { throw PairingError.address }
        } else if inet_pton(AF_INET6, text, &v6) == 1 {
            let bytes = withUnsafeBytes(of: v6) { Array($0) }
            guard bytes.contains(where: { $0 != 0 }), bytes != Array(repeating: 0, count: 15) + [1], bytes[0] != 255 else { throw PairingError.address }
        } else { throw PairingError.address }
        return text.lowercased()
    }
}
struct PairedConsole: Codable, Equatable, Identifiable {
    let accountID: Data
    let address: String
    let mac: Data
    let name: String
    let target: UInt32
    let registrationKey: Data
    let sessionKey: Data
    let keyType: UInt32
    var id: String { mac.map { String(format: "%02x", $0) }.joined() }
    func replacingAddress(_ text: String) throws -> PairedConsole {
        PairedConsole(accountID: accountID, address: try PairingRequest.validAddress(text), mac: mac,
                      name: name, target: target, registrationKey: registrationKey, sessionKey: sessionKey, keyType: keyType)
    }
    func validate(for account: Account) throws {
        guard accountID == account.accountID, mac.count == 6, mac.contains(where: { $0 != 0 }),
              registrationKey.count == 16, sessionKey.count == 16, target == 1000100,
              registrationKey.contains(where: { $0 != 0 }), sessionKey.contains(where: { $0 != 0 }),
              name.utf8.count <= 256, (try? PairingRequest.validAddress(address)) == address else { throw PairingError.identity }
    }
}
