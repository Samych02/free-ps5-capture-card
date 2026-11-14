import Combine
import Foundation

@MainActor protocol ConsoleServicing: AnyObject {
    func discover(_ handler: @escaping (Result<[DiscoveredConsole], PairingError>) -> Void)
    func stopDiscovery() async
    func pair(_ request: PairingRequest) async throws -> PairedConsole
    func cancelPairing()
    func shutdown() async
}
@MainActor final class PairingModel: ObservableObject {
    @Published private(set) var consoles: [DiscoveredConsole] = []
    @Published private(set) var paired: PairedConsole?
    @Published private(set) var busy = false
    @Published private(set) var message: String?
    @Published private(set) var isDiscovering = false
    @Published private(set) var discoveryMessage: String?
    private let service: ConsoleServicing
    private let store: CredentialStore
    private var account: Account?
    private var generation = UUID()
    private var discoveryGeneration = UUID()
    private var discoveryTimer: Task<Void, Never>?
    init(service: ConsoleServicing, store: CredentialStore) { self.service = service; self.store = store }
    func setAccount(_ account: Account?) {
        cancel(); self.account = account; paired = nil; consoles = []
        guard let account else { return }
        do {
            if let saved = try store.loadPairing() { try saved.validate(for: account); paired = saved }
        } catch { message = PairingError.storage.localizedDescription }
    }
    func discover() {
        guard account != nil, !busy else { return }
        discoveryTimer?.cancel(); isDiscovering = true; discoveryMessage = nil
        let current = UUID(); discoveryGeneration = current
        discoveryTimer = Task { [weak self] in
            do { try await Task.sleep(for: .seconds(5)) } catch { return }
            guard let self, self.discoveryGeneration == current else { return }
            self.isDiscovering = false
        }
        service.discover { [weak self] result in
            guard let self, self.discoveryGeneration == current else { return }
            switch result {
            case .success(let consoles): self.consoles = consoles
            case .failure(let error):
                self.discoveryTimer?.cancel(); self.isDiscovering = false
                self.discoveryMessage = error.localizedDescription
            }
        }
    }
    func pair(address: String, pin: String, expectedID: String? = nil) async {
        guard let account, !busy else { return }
        let current = UUID(); generation = current; busy = true; message = nil
        defer { if generation == current { busy = false } }
        do {
            let request = try PairingRequest(address: address, pin: pin, account: account, expectedID: expectedID)
            let result = try await service.pair(request)
            guard generation == current, self.account == account, !Task.isCancelled else { return }
            try result.validate(for: account)
            guard result.address == request.address,
                  expectedID == nil || result.id == expectedID?.lowercased() else { throw PairingError.identity }
            do { try store.savePairing(result) } catch { throw PairingError.storage }
            paired = result
        } catch {
            guard generation == current else { return }
            message = ((error as? PairingError) ?? .failed).localizedDescription
        }
    }
    func cancel() {
        generation = UUID(); discoveryGeneration = UUID(); service.cancelPairing(); busy = false; message = nil
        discoveryTimer?.cancel(); isDiscovering = false; discoveryMessage = nil
    }
    func updateAddress(_ address: String, expected: PairedConsole) -> Bool {
        message = nil
        guard !busy, let account, paired == expected else {
            message = PairingError.pairingChanged.localizedDescription; return false
        }
        do {
            try expected.validate(for: account)
            let updated = try store.updatePairingAddress(address, expected: expected)
            paired = updated
            return true
        } catch {
            message = ((error as? PairingError) ?? .storage).localizedDescription
            return false
        }
    }
    func forget() {
        cancel()
        do { try store.forgetPairing(); paired = nil } catch { message = PairingError.storage.localizedDescription }
    }
    func stopDiscovery() async {
        discoveryGeneration = UUID(); discoveryTimer?.cancel(); isDiscovering = false
        await service.stopDiscovery()
    }
    func shutdown() async { cancel(); await service.shutdown() }
}
