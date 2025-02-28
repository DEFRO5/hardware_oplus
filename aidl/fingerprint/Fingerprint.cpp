/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Fingerprint.h"
#include <android-base/logging.h>

#define FP_TA_PATH "odm/vendor/firmware"
#define FP_TA_NAME "uff_gx"
#define BASE_BUFFER_SIZE 4096
#define QSEECOM_LIB_NAME "libQSEEComAPI.so"

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

Fingerprint::Fingerprint() : mLibHandle(nullptr), mTaHandle(nullptr), 
                            mBufferSize(BASE_BUFFER_SIZE), 
                            mStartApp(nullptr), mShutdownApp(nullptr) {
    LOG(INFO) << "Fingerprint HAL constructor";
    if (loadQseecom()) {
        loadTrustZoneApp();
    }
}

Fingerprint::~Fingerprint() {
    LOG(INFO) << "Fingerprint HAL destructor";
    unloadTrustZoneApp();
    unloadQseecom();
}

bool Fingerprint::loadQseecom() {
    mLibHandle = dlopen(QSEECOM_LIB_NAME, RTLD_NOW);
    if (!mLibHandle) {
        LOG(ERROR) << "Failed to load " << QSEECOM_LIB_NAME << ": " << dlerror();
        return false;
    }

    // Load QSEECom functions
    mStartApp = reinterpret_cast<start_app_fn>(dlsym(mLibHandle, "QSEECom_start_app"));
    if (!mStartApp) {
        LOG(ERROR) << "Failed to load QSEECom_start_app: " << dlerror();
        dlclose(mLibHandle);
        mLibHandle = nullptr;
        return false;
    }

    mShutdownApp = reinterpret_cast<shutdown_app_fn>(dlsym(mLibHandle, "QSEECom_shutdown_app"));
    if (!mShutdownApp) {
        LOG(ERROR) << "Failed to load QSEECom_shutdown_app: " << dlerror();
        dlclose(mLibHandle);
        mLibHandle = nullptr;
        return false;
    }

    LOG(INFO) << "Successfully loaded QSEECom library";
    return true;
}

void Fingerprint::unloadQseecom() {
    if (mLibHandle) {
        dlclose(mLibHandle);
        mLibHandle = nullptr;
        mStartApp = nullptr;
        mShutdownApp = nullptr;
    }
}

void Fingerprint::loadTrustZoneApp() {
    if (!mStartApp) {
        LOG(ERROR) << "QSEECom functions not loaded";
        return;
    }

    if (mTaHandle != nullptr) {
        LOG(INFO) << "TrustZone app already loaded";
        return;
    }

    int status = mStartApp(&mTaHandle, FP_TA_PATH, FP_TA_NAME, mBufferSize);
    if (status) {
        LOG(ERROR) << "Failed to load TrustZone app: " << status;
        return;
    }

    LOG(INFO) << "Successfully loaded TrustZone app";
}

void Fingerprint::unloadTrustZoneApp() {
    if (mTaHandle != nullptr && mShutdownApp != nullptr) {
        mShutdownApp(&mTaHandle);
        mTaHandle = nullptr;
        LOG(INFO) << "TrustZone app unloaded";
    }
}

ndk::ScopedAStatus Fingerprint::getSensorProps(std::vector<SensorProps>* out) {
    if (!mTaHandle) {
        LOG(ERROR) << "TrustZone app not loaded";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    // TODO: Implement getting sensor properties from TrustZone app
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Fingerprint::createSession(int32_t sensorId, int32_t userId,
                                            const std::shared_ptr<ISessionCallback>& cb,
                                            std::shared_ptr<ISession>* out) {
    if (!mTaHandle) {
        LOG(ERROR) << "TrustZone app not loaded";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    // TODO: Implement session creation with TrustZone app
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
