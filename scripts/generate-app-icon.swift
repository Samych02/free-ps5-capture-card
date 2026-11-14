#!/usr/bin/env swift
// Reproducible original artwork; run from the repository root. No external assets.
import AppKit

let destination = URL(fileURLWithPath: "App/Assets.xcassets/AppIcon.appiconset", isDirectory: true)
try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)

func color(_ red: CGFloat, _ green: CGFloat, _ blue: CGFloat) -> NSColor {
    NSColor(srgbRed: red, green: green, blue: blue, alpha: 1)
}

func drawIcon(pixels: Int) throws -> Data {
    guard let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels,
                                        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
                                        isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0),
          let context = NSGraphicsContext(bitmapImageRep: bitmap) else {
        throw NSError(domain: "AppIcon", code: 1)
    }
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = context
    defer { NSGraphicsContext.restoreGraphicsState() }
    context.cgContext.scaleBy(x: CGFloat(pixels) / 1024, y: CGFloat(pixels) / 1024)
    context.imageInterpolation = .high

    // Native icon silhouette with transparent margins, not a full-bleed square.
    let tile = NSBezierPath(roundedRect: NSRect(x: 100, y: 100, width: 824, height: 824), xRadius: 184, yRadius: 184)
    NSGraphicsContext.saveGraphicsState()
    let shadow = NSShadow()
    shadow.shadowColor = NSColor.black.withAlphaComponent(0.28)
    shadow.shadowBlurRadius = 22
    shadow.shadowOffset = NSSize(width: 0, height: -10)
    shadow.set()
    color(0.035, 0.055, 0.10).setFill()
    tile.fill()
    NSGraphicsContext.restoreGraphicsState()
    NSGradient(starting: color(0.13, 0.19, 0.28), ending: color(0.025, 0.045, 0.085))!
        .draw(in: tile, angle: -90)
    NSColor.white.withAlphaComponent(0.15).setStroke()
    tile.lineWidth = 2
    tile.stroke()

    // Four precise capture corners: generous strokes survive the 16px rendition.
    let frame = NSBezierPath()
    for (x, y, dx, dy) in [(CGFloat(280), CGFloat(310), CGFloat(1), CGFloat(1)),
                            (744, 310, -1, 1), (280, 714, 1, -1), (744, 714, -1, -1)] {
        frame.move(to: NSPoint(x: x, y: y + dy * 100))
        frame.line(to: NSPoint(x: x, y: y))
        frame.line(to: NSPoint(x: x + dx * 100, y: y))
    }
    frame.lineWidth = 48
    frame.lineCapStyle = .round
    frame.lineJoinStyle = .round
    color(0.12, 0.64, 1).setStroke()
    frame.stroke()

    // A single forward-flow mark, not a playback triangle or third-party logo.
    let flow = NSBezierPath()
    flow.move(to: NSPoint(x: 385, y: 512))
    flow.line(to: NSPoint(x: 625, y: 512))
    flow.move(to: NSPoint(x: 533, y: 604))
    flow.line(to: NSPoint(x: 625, y: 512))
    flow.line(to: NSPoint(x: 533, y: 420))
    flow.lineWidth = 48
    flow.lineCapStyle = .round
    flow.lineJoinStyle = .round
    color(0.91, 0.97, 1).setStroke()
    flow.stroke()

    guard let data = bitmap.representation(using: .png, properties: [:]) else {
        throw NSError(domain: "AppIcon", code: 2)
    }
    return data
}

var images: [[String: String]] = []
for size in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let name = "icon_\(size)x\(size)@\(scale)x.png"
        try drawIcon(pixels: size * scale).write(to: destination.appendingPathComponent(name), options: .atomic)
        images.append(["idiom": "mac", "size": "\(size)x\(size)", "scale": "\(scale)x", "filename": name])
    }
}
let info: [String: Any] = ["version": 1, "author": "xcode"]
let catalog = try JSONSerialization.data(withJSONObject: ["images": images, "info": info], options: [.prettyPrinted, .sortedKeys])
try catalog.write(to: destination.appendingPathComponent("Contents.json"), options: .atomic)
let rootCatalog = try JSONSerialization.data(withJSONObject: ["info": info], options: [.prettyPrinted, .sortedKeys])
try rootCatalog.write(to: destination.deletingLastPathComponent().appendingPathComponent("Contents.json"), options: .atomic)
print("Generated all 10 macOS AppIcon renditions.")
