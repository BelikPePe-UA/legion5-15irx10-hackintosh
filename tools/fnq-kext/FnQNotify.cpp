// FnQNotify: reports the Legion power mode (Fn+Q) to user space. Read-only.
// The Game Zone WMI device (\_SB.GZFD, _UID "GMZN") sends Notify 0xE3 (LENOVO_GAMEZONE_SMART_FAN_MODE_EVENT) when the EC
// switches mode. YogaSMC 1.5.3 receives it but drops it. On that notify we read the current mode with the WMI method
// GetSmartFanMode (WMAA, instance 0, method 0x2D, input 0 - the same call shape as YogaSMC's WMI::evaluateInteger),
// publish it as the "SmartFanMode" property and message our interested clients (FnQOSD).
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <kern/thread_call.h>

class FnQNotify : public IOService {
    OSDeclareDefaultStructors(FnQNotify)
    IOACPIPlatformDevice *fAcpi = nullptr;
    thread_call_t fCall = nullptr;
    volatile SInt32 fEvents = 0;
    static void readCall(thread_call_param_t self, thread_call_param_t);
    bool readMode(UInt32 *mode);
    void publish(bool notify);
public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    IOReturn message(UInt32 type, IOService *provider, void *argument) override;
};
OSDefineMetaClassAndStructors(FnQNotify, IOService)

static const UInt32 kSmartFanEvent = 0xE3, kGetSmartFanMode = 0x2D;

bool FnQNotify::readMode(UInt32 *mode) {
    OSNumber *inst = OSNumber::withNumber(0ULL, 8), *mid = OSNumber::withNumber(kGetSmartFanMode, 32), *in = OSNumber::withNumber(0ULL, 32);
    if (!inst || !mid || !in) { OSSafeReleaseNULL(inst); OSSafeReleaseNULL(mid); OSSafeReleaseNULL(in); return false; }
    OSObject *params[3] = { inst, mid, in }, *res = nullptr;
    IOReturn r = fAcpi->evaluateObject("WMAA", &res, params, 3);
    inst->release(); mid->release(); in->release();
    OSNumber *n = OSDynamicCast(OSNumber, res);
    bool ok = r == kIOReturnSuccess && n;
    if (ok) *mode = n->unsigned32BitValue();
    OSSafeReleaseNULL(res);
    return ok;
}

void FnQNotify::publish(bool notify) {
    UInt32 mode;
    if (!readMode(&mode)) { IOLog("FnQNotify: GetSmartFanMode failed\n"); return; }
    const char *name = mode == 1 ? "Quiet" : mode == 2 ? "Balanced" : mode == 3 ? "Performance" : mode == 0xE0 ? "Extreme" : mode == 0xFF ? "Custom" : "Unknown";
    setProperty("SmartFanMode", mode, 32);
    setProperty("SmartFanModeName", name);
    setProperty("Events", (UInt32)fEvents, 32);
    if (notify) messageClients(kIOMessageServicePropertyChange, (void *)(uintptr_t)mode);
}

void FnQNotify::readCall(thread_call_param_t self, thread_call_param_t) {
    static_cast<FnQNotify *>(self)->publish(true);
}

IOReturn FnQNotify::message(UInt32 type, IOService *provider, void *argument) {
    if (type == kIOACPIMessageDeviceNotification && argument && *static_cast<UInt32 *>(argument) == kSmartFanEvent) {
        OSIncrementAtomic(&fEvents);
        if (fCall) thread_call_enter(fCall);   // ACPI evaluation off the notification path
        return kIOReturnSuccess;
    }
    return IOService::message(type, provider, argument);
}

bool FnQNotify::start(IOService *provider) {
    if (!IOService::start(provider)) return false;
    fAcpi = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (!fAcpi || fAcpi->validateObject("WMAA") != kIOReturnSuccess) { IOLog("FnQNotify: no WMAA on provider\n"); return false; }
    fCall = thread_call_allocate(readCall, this);
    if (!fCall) return false;
    publish(false);
    registerService();
    IOLog("FnQNotify: started\n");
    return true;
}

void FnQNotify::stop(IOService *provider) {
    if (fCall) { thread_call_cancel_wait(fCall); thread_call_free(fCall); fCall = nullptr; }
    IOService::stop(provider);
}
