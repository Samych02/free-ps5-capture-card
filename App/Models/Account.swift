import Foundation

struct Account: Codable, Equatable {
    let accountID: Data
    init(decimalID: String) throws {
        guard !decimalID.isEmpty, decimalID.utf8.allSatisfy({ (48...57).contains($0) }),
              let number = UInt64(decimalID), number > 0 else { throw AuthError.invalidResponse }
        accountID = Data((0..<8).map { UInt8(truncatingIfNeeded: number >> ($0 * 8)) })
    }
    private enum CodingKeys: String, CodingKey { case version, accountID }
    init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        guard try values.decode(Int.self, forKey: .version) == 1 else { throw AuthError.storage }
        let data = try values.decode(Data.self, forKey: .accountID)
        guard data.count == 8, data.contains(where: { $0 != 0 }) else { throw AuthError.storage }
        accountID = data
    }
    func encode(to encoder: Encoder) throws {
        var values = encoder.container(keyedBy: CodingKeys.self)
        try values.encode(1, forKey: .version)
        try values.encode(accountID, forKey: .accountID)
    }
}
