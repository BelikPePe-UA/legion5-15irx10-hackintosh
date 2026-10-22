// render-modes.swift <out.png>: chart of the RTX 5070 Laptop clock by Fn+Q mode (idle / under load), measured with
// tools/nvclk + tools/gpu-sustain on 22.10.2026. Grouped bars, values from CHANGELOG [148]–[149].
import AppKit

let modes = ["Синій (тихий)", "Білий (збалансований)", "Червоний (продуктивність)"]
let idle: [Double] = [790, 1340, 2090]
let load: [Double] = [1417, 2270, 2240]
let W = 1200.0, H = 640.0
let surface = NSColor(srgbRed: 0xfc/255, green: 0xfc/255, blue: 0xfb/255, alpha: 1)
let ink = NSColor(srgbRed: 0x0b/255, green: 0x0b/255, blue: 0x0b/255, alpha: 1)
let ink2 = NSColor(srgbRed: 0x52/255, green: 0x51/255, blue: 0x4e/255, alpha: 1)
let grid = NSColor(srgbRed: 0xe4/255, green: 0xe3/255, blue: 0xdf/255, alpha: 1)
let c1 = NSColor(srgbRed: 0x2a/255, green: 0x78/255, blue: 0xd6/255, alpha: 1)   // series 1: idle
let c2 = NSColor(srgbRed: 0xeb/255, green: 0x68/255, blue: 0x34/255, alpha: 1)   // series 2: load

let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(W), pixelsHigh: Int(H), bitsPerSample: 8,
                           samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
surface.setFill(); NSRect(x: 0, y: 0, width: W, height: H).fill()

func text(_ s: String, _ x: Double, _ y: Double, size: Double, color: NSColor, bold: Bool = false, align: NSTextAlignment = .left) {
    let p = NSMutableParagraphStyle(); p.alignment = align
    let a: [NSAttributedString.Key: Any] = [.font: bold ? NSFont.boldSystemFont(ofSize: size) : NSFont.systemFont(ofSize: size),
                                            .foregroundColor: color, .paragraphStyle: p]
    let str = NSAttributedString(string: s, attributes: a), sz = str.size()
    let ox = align == .center ? x - sz.width / 2 : align == .right ? x - sz.width : x
    str.draw(at: NSPoint(x: ox, y: y))
}

text("RTX 5070 Laptop у macOS: частота GPU за режимом Fn+Q", 48, H - 60, size: 26, color: ink, bold: true)
text("МГц, виміряно nvclk; навантаження: gpu-sustain (60 с Metal FMA)", 48, H - 92, size: 16, color: ink2)
// legend
c1.setFill(); NSBezierPath(roundedRect: NSRect(x: 48, y: H - 130, width: 16, height: 16), xRadius: 3, yRadius: 3).fill()
text("Простій", 72, H - 132, size: 16, color: ink)
c2.setFill(); NSBezierPath(roundedRect: NSRect(x: 168, y: H - 130, width: 16, height: 16), xRadius: 3, yRadius: 3).fill()
text("Під навантаженням", 192, H - 132, size: 16, color: ink)

let left = 110.0, right = W - 48, bottom = 80.0, top = H - 170
let maxV = 2500.0
func y(_ v: Double) -> Double { bottom + (top - bottom) * v / maxV }
for g in stride(from: 0.0, through: 2500.0, by: 500.0) {
    grid.setStroke(); let l = NSBezierPath(); l.lineWidth = 1
    l.move(to: NSPoint(x: left, y: y(g))); l.line(to: NSPoint(x: right, y: y(g))); l.stroke()
    text(String(Int(g)), left - 12, y(g) - 9, size: 14, color: ink2, align: .right)
}
let groupW = (right - left) / Double(modes.count), barW = 120.0, gap = 4.0
func bar(_ x: Double, _ v: Double, _ c: NSColor) {
    c.setFill()
    let r = NSRect(x: x, y: bottom, width: barW, height: y(v) - bottom)
    let p = NSBezierPath(roundedRect: r, xRadius: 4, yRadius: 4); p.fill()
    NSRect(x: x, y: bottom, width: barW, height: 4).fill()          // square end on the baseline
    text("\(Int(v))", x + barW / 2, y(v) + 6, size: 16, color: ink, bold: true, align: .center)
}
for (i, m) in modes.enumerated() {
    let cx = left + groupW * (Double(i) + 0.5)
    bar(cx - barW - gap / 2, idle[i], c1)
    bar(cx + gap / 2, load[i], c2)
    text(m, cx, bottom - 34, size: 16, color: ink, align: .center)
}
let base = NSBezierPath(); base.lineWidth = 1.5; ink2.setStroke()
base.move(to: NSPoint(x: left, y: bottom)); base.line(to: NSPoint(x: right, y: bottom)); base.stroke()

let png = rep.representation(using: .png, properties: [:])!
try! png.write(to: URL(fileURLWithPath: CommandLine.arguments[1]))
