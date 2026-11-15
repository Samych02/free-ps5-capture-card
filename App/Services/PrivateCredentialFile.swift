import Foundation
import Darwin

@MainActor
final class PrivateCredentialFile: CredentialFileAccess {
    static let fileName = "credentials.json"
    static let lockFileName = ".credentials.lock"
    static let maxBytes = 64 * 1024
    let directory: URL
    init(directory: URL? = nil) {
        self.directory = directory ?? FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Application Support/FreePS5CaptureCard", isDirectory: true)
    }
    private func validateACL(_ fd: Int32) throws {
        errno = 0
        guard let acl = acl_get_fd_np(fd, ACL_TYPE_EXTENDED) else {
            // Darwin reports a descriptor with no extended ACL as ENOENT,
            // rather than returning an allocated empty ACL.
            guard errno == ENOENT else { throw AuthError.storage }
            return
        }
        defer { _ = acl_free(UnsafeMutableRawPointer(acl)) }
        guard acl_valid(acl) == 0 else { throw AuthError.storage }
        var entry: acl_entry_t?
        var index = ACL_FIRST_ENTRY.rawValue
        while true {
            errno = 0
            let result = acl_get_entry(acl, index, &entry)
            if result != 0 {
                // Darwin reports end-of-entries as EINVAL. The ACL was already
                // validated and only FIRST/NEXT are used; other errors fail closed.
                guard result == -1, errno == EINVAL else { throw AuthError.storage }
                return
            }
            guard let entry else { throw AuthError.storage }
            var tag = ACL_UNDEFINED_TAG
            // Deny-only entries cannot widen access. Conservatively reject any
            // granting entry, including inherited/inherit-only or owner grants.
            guard acl_get_tag_type(entry, &tag) == 0, tag == ACL_EXTENDED_DENY else { throw AuthError.storage }
            index = ACL_NEXT_ENTRY.rawValue
        }
    }
    private func validate(_ fd: Int32, directory: Bool = false) throws -> stat {
        var info = stat()
        guard fstat(fd, &info) == 0, info.st_uid == geteuid(),
              info.st_mode & mode_t(S_IFMT) == mode_t(directory ? S_IFDIR : S_IFREG),
              info.st_mode & 0o7777 == (directory ? 0o700 : 0o600),
              directory || info.st_nlink == 1 else { throw AuthError.storage }
        try validateACL(fd)
        return info
    }
    private func openDirectory() throws -> Int32 {
        guard directory.isFileURL else { throw AuthError.storage }
        try FileManager.default.createDirectory(at: directory.deletingLastPathComponent(), withIntermediateDirectories: true,
                                                attributes: [.posixPermissions: 0o700])
        if Darwin.mkdir(directory.path, 0o700) != 0 && errno != EEXIST { throw AuthError.storage }
        let fd = Darwin.open(directory.path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)
        guard fd >= 0 else { throw AuthError.storage }
        do { _ = try validate(fd, directory: true); return fd }
        catch { _ = Darwin.close(fd); throw AuthError.storage }
    }
    private func locked<T>(exclusive: Bool, _ body: (Int32) throws -> T) throws -> T {
        do {
            let directoryFD = try openDirectory()
            defer { _ = Darwin.close(directoryFD) }
            let lockFD = openat(directoryFD, Self.lockFileName, O_RDWR | O_CREAT | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0o600)
            guard lockFD >= 0 else { throw AuthError.storage }
            defer { _ = Darwin.close(lockFD) }
            _ = try validate(lockFD)
            guard flock(lockFD, (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) == 0 else { throw AuthError.storage }
            defer { _ = flock(lockFD, LOCK_UN) }
            return try body(directoryFD)
        } catch { throw AuthError.storage } // Never propagate raw paths/data/system errors.
    }
    private func read(in directoryFD: Int32) throws -> Data? {
        let fd = openat(directoryFD, Self.fileName, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)
        if fd < 0 {
            if errno == ENOENT { return nil }
            throw AuthError.storage
        }
        defer { _ = Darwin.close(fd) }
        let info = try validate(fd)
        guard info.st_size >= 0, info.st_size <= Self.maxBytes else { throw AuthError.storage }
        let size = Int(info.st_size)
        var data = Data(count: size)
        try data.withUnsafeMutableBytes { buffer in
            var offset = 0
            while offset < size {
                let count = Darwin.read(fd, buffer.baseAddress!.advanced(by: offset), size - offset)
                if count < 0 && errno == EINTR { continue }
                guard count > 0 else { throw AuthError.storage }
                offset += count
            }
        }
        var extra: UInt8 = 0
        var count: Int
        repeat { count = Darwin.read(fd, &extra, 1) } while count < 0 && errno == EINTR
        guard count == 0 else { throw AuthError.storage }
        return data
    }
    func read() throws -> Data? { try locked(exclusive: false) { try read(in: $0) } }
    func write(_ data: Data, replacing expected: Data?) throws {
        guard !data.isEmpty, data.count <= Self.maxBytes else { throw AuthError.storage }
        try locked(exclusive: true) { directoryFD in
            guard try read(in: directoryFD) == expected else { throw AuthError.storage }
            let temporary = ".credentials.\(UUID().uuidString).tmp"
            let fd = openat(directoryFD, temporary, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0o600)
            guard fd >= 0 else { throw AuthError.storage }
            var cleanup = true
            defer { _ = Darwin.close(fd); if cleanup { _ = unlinkat(directoryFD, temporary, 0) } }
            guard fchmod(fd, 0o600) == 0 else { throw AuthError.storage }
            _ = try validate(fd)
            try data.withUnsafeBytes { buffer in
                var offset = 0
                while offset < data.count {
                    let count = Darwin.write(fd, buffer.baseAddress!.advanced(by: offset), data.count - offset)
                    if count < 0 && errno == EINTR { continue }
                    guard count > 0 else { throw AuthError.storage }
                    offset += count
                }
            }
            guard fsync(fd) == 0, renameat(directoryFD, temporary, directoryFD, Self.fileName) == 0 else { throw AuthError.storage }
            cleanup = false
            _ = fsync(directoryFD)
        }
    }
    func delete(replacing expected: Data?) throws {
        try locked(exclusive: true) { directoryFD in
            let current = try read(in: directoryFD)
            guard current == expected else { throw AuthError.storage }
            if current != nil {
                guard unlinkat(directoryFD, Self.fileName, 0) == 0 else { throw AuthError.storage }
                _ = fsync(directoryFD)
            }
        }
    }
}
