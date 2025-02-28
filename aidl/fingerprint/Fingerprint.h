/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <dlfcn.h>

using ::aidl::android::hardware::biometrics::fingerprint::ISession;
using ::aidl::android::hardware::biometrics::fingerprint::ISessionCallback;
using ::aidl::android::hardware::biometrics::fingerprint::SensorProps;

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

// QSEECom function types
typedef int (*start_app_fn)(void** handle, const char* path, const char* name, uint32_t size);
typedef int (*shutdown_app_fn)(void** handle);

class Fingerprint : public BnFingerprint {
public:
    Fingerprint();
    ~Fingerprint();
    ndk::ScopedAStatus getSensorProps(std::vector<SensorProps>* out) override;
    ndk::ScopedAStatus createSession(int32_t sensorId, int32_t userId,
                                   const std::shared_ptr<ISessionCallback>& cb,
                                   std::shared_ptr<ISession>* out) override;

private:
    void* mLibHandle;
    void* mTaHandle;
    uint32_t mBufferSize;
    
    // Function pointers for QSEECom APIs
    start_app_fn mStartApp;
    shutdown_app_fn mShutdownApp;
    
    bool loadQseecom();
    void unloadQseecom();
    void loadTrustZoneApp();
    void unloadTrustZoneApp();
};

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
