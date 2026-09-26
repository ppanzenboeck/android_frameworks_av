#include "CameraServiceExtFactory.h"
#include <dlfcn.h>
#include <log/log.h>

namespace android {

void* CameraServiceExtFactory::sFunctionTable = nullptr;
void* CameraServiceExtFactory::sExtObject = nullptr;
int (*CameraServiceExtFactory::sOnTransactFunc)(void*, uint32_t, const Parcel&, Parcel*, uint32_t) = nullptr;

void CameraServiceExtFactory::ensureLoaded() {
    if (sFunctionTable != nullptr) return;

    const char* libPath = "system_ext/lib64/libcsextimpl.so";
    void* handle = dlopen(libPath, RTLD_NOW);
    if (handle == nullptr) {
        ALOGE("CameraServiceExtFactory: dlopen failed: %s", dlerror());
        return;
    }
    ALOGI("CameraServiceExtFactory: dlopen succeeded, handle=%p", handle);

    typedef void* (*GetFactoryFunc)();
    GetFactoryFunc getExtFactoryImpl = (GetFactoryFunc)dlsym(handle, "getExtFactoryImpl");
    if (getExtFactoryImpl == nullptr) {
        ALOGE("CameraServiceExtFactory: dlsym getExtFactoryImpl failed: %s", dlerror());
        dlclose(handle);
        return;
    }
    ALOGI("CameraServiceExtFactory: getExtFactoryImpl at %p", getExtFactoryImpl);

    // Triple indirection as determined from logs: getExtFactoryImpl returns ptr to ptr to ptr to function
    void* ptrToPtr = getExtFactoryImpl();
    if (ptrToPtr == nullptr) {
        ALOGE("CameraServiceExtFactory: getExtFactoryImpl returned null");
        dlclose(handle);
        return;
    }

    void* ptrToFunc = *(void**)ptrToPtr;
    if (ptrToFunc == nullptr) {
        ALOGE("CameraServiceExtFactory: first deref gave null");
        dlclose(handle);
        return;
    }

    void* actualFunc = *(void**)ptrToFunc;
    if (actualFunc == nullptr) {
        ALOGE("CameraServiceExtFactory: second deref gave null");
        dlclose(handle);
        return;
    }
    ALOGI("CameraServiceExtFactory: actual factory function at %p", actualFunc);

    sFunctionTable = operator new(8);
    *(void**)sFunctionTable = actualFunc;
    ALOGI("CameraServiceExtFactory: function table at %p", sFunctionTable);

    // Resolve onTransact (for direct call via vtable)
    sOnTransactFunc = (int (*)(void*, uint32_t, const Parcel&, Parcel*, uint32_t))
        dlsym(handle, "_ZN7android20CameraServiceExtImpl10onTransactEjRKNS_6ParcelEPS1_j");
    if (sOnTransactFunc == nullptr) {
        ALOGE("CameraServiceExtFactory: dlsym onTransact failed: %s", dlerror());
    } else {
        ALOGI("CameraServiceExtFactory: onTransact found at %p", sOnTransactFunc);
    }
}

void* CameraServiceExtFactory::getInstance() {
    ensureLoaded();
    return sFunctionTable;   // may be null
}

int CameraServiceExtFactory::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
    ensureLoaded();
    if (sExtObject == nullptr) {
        if (sFunctionTable == nullptr) {
            ALOGE("CameraServiceExtFactory::onTransact: extension not loaded");
            return -1;
        }
        void* actualFunc = *(void**)sFunctionTable;
        if (actualFunc == nullptr) return -1;
        typedef void* (*GetObjectFunc)();
        sExtObject = ((GetObjectFunc)actualFunc)();
        if (sExtObject == nullptr) {
            ALOGE("CameraServiceExtFactory: factory returned null");
            return -1;
        }
        ALOGI("CameraServiceExtFactory: real extension object at %p", sExtObject);
    }

    if (sOnTransactFunc == nullptr) {
        ALOGE("CameraServiceExtFactory::onTransact: no function pointer");
        return -1;
    }
    return sOnTransactFunc(sExtObject, code, data, reply, flags);
}

CameraServiceExtFactory::~CameraServiceExtFactory() {
    // No cleanup needed – the extension library manages its own singleton.
    ALOGV("CameraServiceExtFactory destructor (stub)");
}

} // namespace android

#if defined(__aarch64__)
// ---------------------------------------------------------------------------
// fmt::v11 compatibility trampolines for prebuilt OxygenOS libcsextimpl.so
// In Android 17 / lineage-23, fmtlib upgraded from v11 to v12, changing its
// mangled symbol names from _ZN3fmt3v11 to _ZN3fmt3v12.
// These tail-call branch trampolines forward libcsextimpl.so calls directly
// to the libbase.so fmt::v12 implementations with zero overhead and full
// register/ABI preservation.
// ---------------------------------------------------------------------------
__asm__(
    ".global _ZN3fmt3v116detail11assert_failEPKciS3_\n"
    ".type _ZN3fmt3v116detail11assert_failEPKciS3_, %function\n"
    "_ZN3fmt3v116detail11assert_failEPKciS3_:\n"
    "    b _ZN3fmt3v1211assert_failEPKciS2_\n"

    ".global _ZN3fmt3v1112report_errorEPKc\n"
    ".type _ZN3fmt3v1112report_errorEPKc, %function\n"
    "_ZN3fmt3v1112report_errorEPKc:\n"
    "    b _ZN3fmt3v1212report_errorEPKc\n"

    ".global _ZN3fmt3v116detail12is_printableEj\n"
    ".type _ZN3fmt3v116detail12is_printableEj, %function\n"
    "_ZN3fmt3v116detail12is_printableEj:\n"
    "    b _ZN3fmt3v126detail12is_printableEj\n"

    ".global _ZN3fmt3v116detail9dragonbox16get_cached_powerEi\n"
    ".type _ZN3fmt3v116detail9dragonbox16get_cached_powerEi, %function\n"
    "_ZN3fmt3v116detail9dragonbox16get_cached_powerEi:\n"
    "    b _ZN3fmt3v126detail9dragonbox16get_cached_powerEi\n"

    ".global _ZN3fmt3v116detail9dragonbox10to_decimalIdEENS2_10decimal_fpIT_EES5_\n"
    ".type _ZN3fmt3v116detail9dragonbox10to_decimalIdEENS2_10decimal_fpIT_EES5_, %function\n"
    "_ZN3fmt3v116detail9dragonbox10to_decimalIdEENS2_10decimal_fpIT_EES5_:\n"
    "    b _ZN3fmt3v126detail9dragonbox10to_decimalIdEENS2_10decimal_fpIT_EES5_\n"

    ".global _ZN3fmt3v116detail9dragonbox10to_decimalIfEENS2_10decimal_fpIT_EES5_\n"
    ".type _ZN3fmt3v116detail9dragonbox10to_decimalIfEENS2_10decimal_fpIT_EES5_, %function\n"
    "_ZN3fmt3v116detail9dragonbox10to_decimalIfEENS2_10decimal_fpIT_EES5_:\n"
    "    b _ZN3fmt3v126detail9dragonbox10to_decimalIfEENS2_10decimal_fpIT_EES5_\n"

    ".global _ZN3fmt3v116detail18thousands_sep_implIcEENS1_20thousands_sep_resultIT_EENS1_10locale_refE\n"
    ".type _ZN3fmt3v116detail18thousands_sep_implIcEENS1_20thousands_sep_resultIT_EENS1_10locale_refE, %function\n"
    "_ZN3fmt3v116detail18thousands_sep_implIcEENS1_20thousands_sep_resultIT_EENS1_10locale_refE:\n"
    "    b _ZN3fmt3v126detail18thousands_sep_implIcEENS1_20thousands_sep_resultIT_EENS0_10locale_refE\n"

    ".global _ZN3fmt3v116detail18decimal_point_implIcEET_NS1_10locale_refE\n"
    ".type _ZN3fmt3v116detail18decimal_point_implIcEET_NS1_10locale_refE, %function\n"
    "_ZN3fmt3v116detail18decimal_point_implIcEET_NS1_10locale_refE:\n"
    "    b _ZN3fmt3v126detail18decimal_point_implIcEET_NS0_10locale_refE\n"

    ".global _ZN3fmt3v116detail9write_locENS0_14basic_appenderIcEENS0_9loc_valueERKNS0_12format_specsENS1_10locale_refE\n"
    ".type _ZN3fmt3v116detail9write_locENS0_14basic_appenderIcEENS0_9loc_valueERKNS0_12format_specsENS1_10locale_refE, %function\n"
    "_ZN3fmt3v116detail9write_locENS0_14basic_appenderIcEENS0_9loc_valueERKNS0_12format_specsENS1_10locale_refE:\n"
    "    b _ZN3fmt3v126detail9write_locENS0_14basic_appenderIcEENS0_9loc_valueERKNS0_12format_specsENS0_10locale_refE\n"
);
#endif

