/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "OPlusSensorPropsShim"

#include <aidl/android/hardware/biometrics/fingerprint/SensorProps.h>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include <dlfcn.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DynamicDisplayInfo.h>

using android::base::GetProperty;
using android::base::ParseInt;
using android::base::Tokenize;
using android::SurfaceComposerClient;

using aidl::android::hardware::biometrics::fingerprint::FingerprintSensorType;
using aidl::android::hardware::biometrics::fingerprint::SensorProps;

namespace {
SensorProps SensorPropsInit(SensorProps props) {
    auto type = GetProperty("persist.vendor.fingerprint.sensor_type", "");
    if (!type.empty()) {
        if (type == "back")
            props.sensorType = FingerprintSensorType::REAR;
        else if (type == "ultrasonic")
            props.sensorType = FingerprintSensorType::UNDER_DISPLAY_ULTRASONIC;
        else if (type == "optical")
            props.sensorType = FingerprintSensorType::UNDER_DISPLAY_OPTICAL;
        else if (type == "side")
            props.sensorType = FingerprintSensorType::POWER_BUTTON;
        else if (type == "front")
            props.sensorType = FingerprintSensorType::HOME_BUTTON;
    }

    auto size = GetProperty("persist.vendor.fingerprint.optical.iconsize", "");
    if (!size.empty()) {
        if (ParseInt(size, &props.sensorLocations[0].sensorRadius)) {
            props.sensorLocations[0].sensorRadius /= 2;
        } else {
            LOG(WARNING) << "Invalid sensor size input: " << size;
        }
    }

    android::ui::DynamicDisplayInfo info;
    const auto displayIds = SurfaceComposerClient::getPhysicalDisplayIds();
    
    if (displayIds.empty()) {
        LOG(ERROR) << "No physical displays found";
        return props;
    }
    
    if (SurfaceComposerClient::getDynamicDisplayInfoFromId(displayIds[0].value, &info) == 0) {
        if (info.supportedDisplayModes.empty()) {
            LOG(ERROR) << "No valid display modes found";
            return props;
        }

        const auto& mode = info.supportedDisplayModes.back();
        const int32_t width = mode.resolution.getWidth();
        const int32_t height = mode.resolution.getHeight();

        props.sensorLocations[0].sensorLocationX = width / 2;

        auto iconLocation = GetProperty("persist.vendor.fingerprint.optical.iconlocation", "");
        if (!iconLocation.empty()) {
            int32_t locationY;
            if (ParseInt(iconLocation, &locationY)) {
                props.sensorLocations[0].sensorLocationY = height - locationY;
            } else {
                LOG(WARNING) << "Invalid icon location input: " << iconLocation;
            }
        }
    } else {
        LOG(ERROR) << "Failed to get display info";
    }
    
    return props;
}
}  // anonymous namespace

extern "C" void
_ZNK4aidl7android8hardware10biometrics11fingerprint11SensorProps13writeToParcelEP7AParcel(
        SensorProps* thisptr, AParcel* parcel) {
    static auto props = SensorPropsInit(*thisptr);
    static auto writeToParcel = reinterpret_cast<
            typeof(_ZNK4aidl7android8hardware10biometrics11fingerprint11SensorProps13writeToParcelEP7AParcel)*>(
            dlsym(RTLD_NEXT, __func__));

    LOG(DEBUG) << "Original props=" << thisptr->toString() << ", new props=" << props.toString();
    writeToParcel(&props, parcel);
}