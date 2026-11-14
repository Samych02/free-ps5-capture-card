import Foundation
import CaptureEngine

struct StreamOptions: Codable, Equatable {
    enum Codec: String, Codable, CaseIterable { case h264, h265, hdr }
    var codec: Codec = .h265
    var height = 1080
    var fps = 60
    var bitrate = 15000
    var port = 9295
    static func validPort(_ text: String) -> Int? {
        guard !text.isEmpty, text.utf8.count <= 5, text.utf8.allSatisfy({ (48...57).contains($0) }),
              let port = Int(text), (1024...65535).contains(port) else { return nil }
        return port
    }
    var isValid: Bool { [720, 1080].contains(height) && [30, 60].contains(fps) && (2000...100000).contains(bitrate) && (1024...65535).contains(port) }
    var native: capture_options {
        var value = capture_default_options()
        guard isValid else { return value }
        value.width = height == 1080 ? 1920 : 1280; value.height = UInt32(height)
        value.fps = UInt32(fps); value.bitrate_kbps = UInt32(bitrate); value.port = UInt32(port)
        value.codec = codec == .h264 ? CAPTURE_H264 : codec == .hdr ? CAPTURE_H265_HDR : CAPTURE_H265
        return value
    }
    private static let key = "streamOptions.v1"
    static func load(from defaults: UserDefaults) -> Self {
        guard let data = defaults.data(forKey: key), let value = try? JSONDecoder().decode(Self.self, from: data), value.isValid else { return Self() }
        return value
    }
    func save(to defaults: UserDefaults) {
        guard isValid, let data = try? JSONEncoder().encode(self) else { return }
        defaults.set(data, forKey: Self.key)
    }
}
