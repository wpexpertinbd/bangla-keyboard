// Render the flag-style input-source icon (green rounded square, red disc, white Bangla letter)
// at every iconset size.   swift make_icon.swift <letter> <out.iconset>   then: iconutil -c icns
import AppKit
import CoreText

let args = CommandLine.arguments
let letter = args.count > 1 ? args[1] : "প"
let outDir = args.count > 2 ? args[2] : "out.iconset"
try? FileManager.default.createDirectory(atPath: outDir, withIntermediateDirectories: true)

let green = CGColor(red: 0x00/255.0, green: 0x6A/255.0, blue: 0x4E/255.0, alpha: 1)
let red   = CGColor(red: 0xF4/255.0, green: 0x2A/255.0, blue: 0x41/255.0, alpha: 1)
let fontName = "SolaimanLipi" as CFString

func render(_ px: Int) -> Data {
    let s = CGFloat(px)
    let cs = CGColorSpaceCreateDeviceRGB()
    let ctx = CGContext(data: nil, width: px, height: px, bitsPerComponent: 8, bytesPerRow: 0,
                        space: cs, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    ctx.clear(CGRect(x: 0, y: 0, width: s, height: s))
    let inset = s * 0.07, side = s - 2 * inset
    let square = CGPath(roundedRect: CGRect(x: inset, y: inset, width: side, height: side),
                        cornerWidth: side * 0.18, cornerHeight: side * 0.18, transform: nil)
    ctx.addPath(square); ctx.setFillColor(green); ctx.fillPath()
    let r = s * 0.33
    ctx.setFillColor(red)
    ctx.fillEllipse(in: CGRect(x: s / 2 - r, y: s / 2 - r, width: 2 * r, height: 2 * r))

    let font = CTFontCreateWithName(fontName, s * 0.62, nil)
    let attr = NSAttributedString(string: letter, attributes: [
        NSAttributedString.Key(kCTFontAttributeName as String): font,
        NSAttributedString.Key(kCTForegroundColorAttributeName as String): CGColor(gray: 1, alpha: 1)])
    let line = CTLineCreateWithAttributedString(attr)
    let ink = CTLineGetImageBounds(line, ctx)              // actual glyph ink, not the em box
    ctx.textPosition = CGPoint(x: s / 2 - ink.midX, y: s / 2 - ink.midY)
    CTLineDraw(line, ctx)

    let rep = NSBitmapImageRep(cgImage: ctx.makeImage()!)
    return rep.representation(using: .png, properties: [:])!
}

for (name, px) in [("16x16", 16), ("16x16@2x", 32), ("32x32", 32), ("32x32@2x", 64),
                   ("128x128", 128), ("128x128@2x", 256), ("256x256", 256), ("256x256@2x", 512),
                   ("512x512", 512), ("512x512@2x", 1024)] {
    try! render(px).write(to: URL(fileURLWithPath: "\(outDir)/icon_\(name).png"))
}
print("wrote \(outDir)")
