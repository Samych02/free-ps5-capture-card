import Combine
import Foundation

@MainActor
final class AccountModel: ObservableObject {
    @Published private(set) var account: Account?
    @Published private(set) var awaitingRedirect = false
    @Published private(set) var busy = false
    @Published private(set) var message: String?
    private let service: PSNAuthService
    private let store: CredentialStore
    private var generation = UUID()
    init(service: PSNAuthService? = nil, store: CredentialStore? = nil) {
        self.service = service ?? PSNAuthService()
        self.store = store ?? KeychainCredentialStore()
    }
    func restore() {
        guard !awaitingRedirect, !busy else { return }
        do { account = try store.loadAccount(); message = nil }
        catch { message = AuthError.storage.localizedDescription }
    }
    func begin() -> URL {
        cancel()
        message = nil
        awaitingRedirect = true
        return service.begin()
    }
    func cancel() {
        generation = UUID()
        service.cancel()
        awaitingRedirect = false
        busy = false
        message = nil
    }
    func complete(_ redirect: String) async {
        guard awaitingRedirect, !busy else { return }
        busy = true
        let current = generation
        defer { if current == generation { busy = false } }
        do {
            let identity = try await service.finish(redirect)
            guard generation == current, !Task.isCancelled else { return }
            try store.saveAccount(identity)
            account = identity
            awaitingRedirect = false
            message = nil
        } catch {
            guard generation == current else { return }
            message = ((error as? AuthError) ?? .storage).localizedDescription
            // Code exchange consumes the code; require a fresh login attempt.
            awaitingRedirect = false
        }
    }
    func forget() {
        cancel()
        do { try store.forgetAccount(); account = nil; message = nil }
        catch { message = AuthError.storage.localizedDescription }
    }
}
