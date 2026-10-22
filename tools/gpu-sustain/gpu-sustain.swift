// gpu-sustain <seconds>: sustained Metal FMA load; prints GFLOPS once a second and the average of the second half.
// Used to see whether the GPU power limit (Fn+Q cTGP) changes sustained throughput. Read-only, no files written.
import Metal
import Foundation

let seconds = Double(CommandLine.arguments.dropFirst().first ?? "60") ?? 60
let src = """
#include <metal_stdlib>
using namespace metal;
kernel void burn(device float4 *out [[buffer(0)]], uint id [[thread_position_in_grid]]) {
    float4 a = float4(id) * 1e-7f, b = float4(1.0001f), c = float4(0.9999f);
    for (int i = 0; i < 4096; i++) { a = fma(a, b, c); b = fma(b, c, a); c = fma(c, a, b); a = fma(a, b, c); }
    out[id & 1023] = a + b + c;
}
"""
guard let dev = MTLCreateSystemDefaultDevice() else { print("no Metal device"); exit(1) }
let lib = try dev.makeLibrary(source: src, options: nil)
let pso = try dev.makeComputePipelineState(function: lib.makeFunction(name: "burn")!)
let q = dev.makeCommandQueue()!
let out = dev.makeBuffer(length: 1024 * 16, options: .storageModePrivate)!
let threads = 1 << 20
let flopsPerBatch = Double(threads) * 4096 * 4 * 4 * 2   // threads x iters x 4 fma x float4 x 2 flop
print("device: \(dev.name), \(Int(seconds)) s")

func batch() -> MTLCommandBuffer {
    let cb = q.makeCommandBuffer()!, e = cb.makeComputeCommandEncoder()!
    e.setComputePipelineState(pso); e.setBuffer(out, offset: 0, index: 0)
    e.dispatchThreads(MTLSize(width: threads, height: 1, depth: 1), threadsPerThreadgroup: MTLSize(width: 256, height: 1, depth: 1))
    e.endEncoding(); cb.commit(); return cb
}

let t0 = Date(); var tick = t0; var flops = 0.0; var samples: [Double] = []
var inflight: [MTLCommandBuffer] = [batch(), batch()]
while Date().timeIntervalSince(t0) < seconds {
    inflight.append(batch())
    inflight.removeFirst().waitUntilCompleted()
    flops += flopsPerBatch
    let now = Date(), dt = now.timeIntervalSince(tick)
    if dt >= 1 {
        let g = flops / dt / 1e9; samples.append(g)
        print(String(format: "%3.0f s  %7.0f GFLOPS", now.timeIntervalSince(t0), g)); fflush(stdout)
        flops = 0; tick = now
    }
}
inflight.forEach { $0.waitUntilCompleted() }
let tail = samples.suffix(samples.count / 2)
print(String(format: "avg 2nd half: %.0f GFLOPS (min %.0f, max %.0f)", tail.reduce(0, +) / Double(max(tail.count, 1)), tail.min() ?? 0, tail.max() ?? 0))
