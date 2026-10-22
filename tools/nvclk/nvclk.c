// nvclk: GPU clocks and the Fn+Q GPU clock cap on NullMoth (NVRM user client opened the way NVK opens it:
// IOServiceOpen "NVAccel"/"NVRM", escape selector 1).
//   nvclk [seconds]          read-only: PMU clock samples (gpc / dram, MHz) and the current P-state, once a second
//   nvclk tdp [ACTION]       RATED_TDP request of the GLOBAL client; ACTION = get | default | limit | lock | floor,
//                            NVCLK_TURBO=1 uses the TURBO_BOOST point (limit = ~1.4 GHz) instead of RATED_TDP (~0.9 GHz)
//   nvclk watch              Fn+Q follower, for the Legion mode now and after every FnQNotify change, until killed:
//                            red = PERF_BOOST to max held by this process, white = GPU's own clock control,
//                            blue = own clock control capped at the TURBO_BOOST point (RATED_TDP GLOBAL limit)
// The RATED_TDP request is GPU state, not tied to this RM client: it stays after nvclk exits. Writes no file.
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "nvtypes.h"
#include "nvos.h"
#include "nv_escape.h"
#include "class/cl0000.h"
#include "class/cl0080.h"
#include "class/cl2080.h"
#include "ctrl/ctrl0000/ctrl0000gpu.h"
#include "ctrl/ctrl2080/ctrl2080clk.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"

enum { SEL_ESC = 1, SEL_BINDFD = 3 };
static io_connect_t conn;
static int bindFd = -1;
static NvU32 hClient, hDev, hSub;

static int esc(uint32_t cmd, void *p, uint32_t size)
{
    uint64_t in = cmd; size_t out = size;
    kern_return_t kr = IOConnectCallMethod(conn, SEL_ESC, &in, 1, p, size, NULL, NULL, p, &out);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "esc 0x%02x -> 0x%08x\n", cmd, kr); return -1; }
    return 0;
}
static NvU32 alloc(NvU32 parent, NvU32 *h, NvU32 cls, void *params)
{
    NVOS21_PARAMETERS p = { .hRoot = hClient, .hObjectParent = parent, .hObjectNew = *h, .hClass = cls, .pAllocParms = params };
    if (esc(NV_ESC_RM_ALLOC, &p, sizeof p)) return NV_ERR_GENERIC;
    *h = p.hObjectNew; return p.status;
}
static NvU32 ctrl(NvU32 h, NvU32 cmd, void *params, NvU32 size)
{
    NVOS54_PARAMETERS p = { .hClient = hClient, .hObject = h, .cmd = cmd, .params = params, .paramsSize = size };
    if (esc(NV_ESC_RM_CONTROL, &p, sizeof p)) return NV_ERR_GENERIC;
    return p.status;
}

static void rm_close(void)
{
    if (hClient) { NVOS00_PARAMETERS f = { .hRoot = hClient, .hObjectParent = hClient, .hObjectOld = hClient }; esc(NV_ESC_RM_FREE, &f, sizeof f); }
    if (conn) IOServiceClose(conn);
    if (bindFd >= 0) close(bindFd);
    conn = 0; bindFd = -1; hClient = hDev = hSub = 0;
}
static int rm_open(void)
{
    io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("NVAccel"));
    uint32_t type = 0x4E560000u;                       // NVAccel: "NV" | ctl (as nvrm_xnu_open)
    if (!svc) { svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("NVRM")); type = 0; }
    if (!svc || IOServiceOpen(svc, mach_task_self(), type, &conn) != KERN_SUCCESS) { fprintf(stderr, "no NVRM user client\n"); return -1; }
    IOObjectRelease(svc);
    bindFd = open("/dev/null", O_RDONLY | O_CLOEXEC);
    uint64_t in = (uint64_t)bindFd; IOConnectCallScalarMethod(conn, SEL_BINDFD, &in, 1, NULL, NULL);

    NvU32 st;
    if ((st = alloc(0, &hClient, NV01_ROOT_CLIENT, NULL))) { fprintf(stderr, "root client 0x%x\n", st); goto fail; }
    NV0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS ids; memset(&ids, 0, sizeof ids);
    if ((st = ctrl(hClient, NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS, &ids, sizeof ids))) { fprintf(stderr, "attached ids 0x%x\n", st); goto fail; }
    NV0000_CTRL_GPU_GET_ID_INFO_V2_PARAMS idi = { .gpuId = ids.gpuIds[0] };
    if ((st = ctrl(hClient, NV0000_CTRL_CMD_GPU_GET_ID_INFO_V2, &idi, sizeof idi))) { fprintf(stderr, "id info 0x%x\n", st); goto fail; }
    NV0080_ALLOC_PARAMETERS a80 = { .deviceId = idi.deviceInstance, .hClientShare = hClient };
    NV2080_ALLOC_PARAMETERS a20 = { .subDeviceId = idi.subDeviceInstance };
    if ((st = alloc(hClient, &hDev, NV01_DEVICE_0, &a80)) || (st = alloc(hDev, &hSub, NV20_SUBDEVICE_0, &a20))) { fprintf(stderr, "device 0x%x\n", st); goto fail; }
    return 0;
fail:
    rm_close();
    return -1;
}

static const char *tdpNames[] = { "default", "exceed", "limit", "lock", "floor" };
static NvU32 tdp_set(int act, int turbo)
{
    NV2080_CTRL_PERF_RATED_TDP_CONTROL_PARAMS sc = { .client = NV2080_CTRL_PERF_RATED_TDP_CLIENT_GLOBAL,
        .input = (NV2080_CTRL_PERF_RATED_TDP_ACTION)act,
        .vPstateType = turbo ? NV2080_CTRL_PERF_VPSTATE_TURBO_BOOST : NV2080_CTRL_PERF_VPSTATE_RATED_TDP };
    return ctrl(hSub, NV2080_CTRL_CMD_PERF_RATED_TDP_SET_CONTROL, &sc, sizeof sc);
}

static void clocks(int secs)
{
    for (int i = 0; i < secs; i++) {
        static NV2080_CTRL_CLK_PMUMON_CLK_DOMAINS_GET_SAMPLES_PARAMS cs;
        memset(&cs, 0, sizeof cs);
        NvU32 cst = ctrl(hSub, NV2080_CTRL_CMD_CLK_PMUMON_CLK_DOMAINS_GET_SAMPLES, &cs, sizeof cs);
        NV2080_CTRL_PERF_GET_CURRENT_PSTATE_PARAMS ps = {0};
        NvU32 pst = ctrl(hSub, NV2080_CTRL_CMD_PERF_GET_CURRENT_PSTATE, &ps, sizeof ps);
        printf("%2d s  ", i + 1);
        if (cst == NV_OK && cs.super.numSamples) {
            NV2080_CTRL_CLK_PMUMON_CLK_DOMAINS_SAMPLE *s = &cs.samples[cs.super.numSamples - 1];
            printf("gpc %4u MHz (active %4u)  dram %5u MHz  nvd %4u MHz  [%u samples]  ", s->gpcClkFreqKHz / 1000,
                   s->activeGpcClkFreqKHz / 1000, s->dramClkFreqKHz / 1000, s->nvdClkFreqKHz / 1000, cs.super.numSamples);
        } else printf("clk ctrl 0x%x (samples %u)  ", cst, cst == NV_OK ? cs.super.numSamples : 0);
        if (pst == NV_OK) printf("pstate 0x%x", ps.currPstate); else printf("pstate ctrl 0x%x", pst);
        printf("\n"); fflush(stdout);
        if (i + 1 < secs) sleep(1);
    }
}

// --- watch: follow the Legion power mode published by FnQNotify (tools/fnq/kext) ---
static io_service_t fnq;
static UInt32 lastMode = ~0u;

static UInt32 fnq_mode(void)
{
    UInt32 m = 0;
    CFTypeRef v = IORegistryEntryCreateCFProperty(fnq, CFSTR("SmartFanMode"), kCFAllocatorDefault, 0);
    if (v && CFGetTypeID(v) == CFNumberGetTypeID()) CFNumberGetValue((CFNumberRef)v, kCFNumberSInt32Type, &m);
    if (v) CFRelease(v);
    return m;
}
// The watcher keeps one RM client for its whole life: a PERF_BOOST request belongs to the client that made it, so the
// red-mode boost lasts exactly as long as this process (and its RM client) does.
static int session;
static void apply(UInt32 mode)
{
    if (mode == lastMode) return;
    int cap = mode == 1;                               // Quiet (blue): auto clocks, no boost above the TURBO_BOOST point
    int boost = mode == 3 || mode == 0xE0;             // Performance / Extreme (red): held at max clocks
    time_t t = time(NULL); char ts[32]; strftime(ts, sizeof ts, "%F %T", localtime(&t));
    if (!session && rm_open() == 0) session = 1;
    if (!session) { printf("%s mode %u: NVRM not reachable\n", ts, mode); fflush(stdout); return; }
    NvU32 st = tdp_set(cap ? 2 : 0, cap);
    NV2080_CTRL_PERF_BOOST_PARAMS bp = { .flags = boost ? (NV2080_CTRL_PERF_BOOST_FLAGS_CMD_BOOST_TO_MAX |
                                                           (NV2080_CTRL_PERF_BOOST_FLAGS_CUDA_YES << 4))
                                                         : NV2080_CTRL_PERF_BOOST_FLAGS_CMD_CLEAR,
                                         .duration = boost ? NV2080_CTRL_PERF_BOOST_DURATION_INFINITE : 0 };
    NvU32 bst = ctrl(hSub, NV2080_CTRL_CMD_PERF_BOOST, &bp, sizeof bp);
    printf("%s mode %u -> GPU %s (tdp 0x%x, boost 0x%x)\n", ts, mode,
           boost ? "max (boost held)" : cap ? "auto, capped at the TURBO_BOOST point (~1.4 GHz)" : "auto", st, bst);
    fflush(stdout);
    if (st != NV_OK || bst != NV_OK) { rm_close(); session = 0; return; }   // retry on the next change
    lastMode = mode;
}
static void fnq_changed(void *ref, io_service_t svc, natural_t type, void *arg)
{
    (void)ref; (void)svc; (void)arg;
    if (type == kIOMessageServicePropertyChange) apply(fnq_mode());
}
static int watch(void)
{
    // FnQNotify may come up after us at login: wait for it (up to 2 min)
    for (int i = 0; i < 120 && !fnq; i++) {
        fnq = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("FnQNotify"));
        if (!fnq) sleep(1);
    }
    if (!fnq) { fprintf(stderr, "no FnQNotify service\n"); return 1; }
    IONotificationPortRef port = IONotificationPortCreate(kIOMainPortDefault);
    io_object_t note;
    if (IOServiceAddInterestNotification(port, fnq, kIOGeneralInterest, fnq_changed, NULL, &note) != KERN_SUCCESS) {
        fprintf(stderr, "interest notification failed\n"); return 1;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(port), kCFRunLoopDefaultMode);
    apply(fnq_mode());
    CFRunLoopRun();
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "watch")) return watch();
    if (rm_open()) return 1;
    if (argc > 1 && !strcmp(argv[1], "tdp")) {
        const char *a = argc > 2 ? argv[2] : "get";
        for (int k = 0; k < 5; k++)
            if (!strcmp(a, tdpNames[k])) printf("set GLOBAL %s -> 0x%x\n", tdpNames[k], tdp_set(k, getenv("NVCLK_TURBO") != NULL));
        NV2080_CTRL_PERF_RATED_TDP_CONTROL_PARAMS gc = { .client = NV2080_CTRL_PERF_RATED_TDP_CLIENT_GLOBAL };
        NvU32 gst = ctrl(hSub, NV2080_CTRL_CMD_PERF_RATED_TDP_GET_CONTROL, &gc, sizeof gc);
        if (gst) printf("get ctrl 0x%x\n", gst);
        else printf("rated TDP, GLOBAL client input %u (0 default, 2 limit, 3 lock, 4 floor), point %s\n", gc.input,
                    gc.vPstateType == NV2080_CTRL_PERF_VPSTATE_TURBO_BOOST ? "TURBO_BOOST" : "RATED_TDP");
    } else {
        clocks(argc > 1 ? atoi(argv[1]) : 5);
    }
    rm_close();
    return 0;
}
