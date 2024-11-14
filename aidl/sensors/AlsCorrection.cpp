/*
 * Copyright (C) 2021-2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AlsCorrection.h"

#include <android-base/properties.h>
#include <android/binder_manager.h>
#include <log/log.h>
#include <oplus/oplus_display_panel.h>
#include <oplus/oplus_display_panel_common.h>
#include <tinyxml2.h>
#include <utils/Timers.h>
#include <cmath>
#include <cstring>
#include <fstream>

using aidl::vendor::lineage::oplus_als::AreaRgbCaptureResult;
using aidl::vendor::lineage::oplus_als::IAreaCapture;
using android::base::GetBoolProperty;
using android::base::GetIntProperty;
using android::base::GetProperty;
using namespace tinyxml2;

#define ALS_CALI_DIR "/proc/sensor/als_cali/"
#define BRIGHTNESS_DIR "/sys/class/backlight/panel0-backlight/"
#define ALS_ARGS_DIR "/odm/etc/fusionlight_profile/"
#define DEFAULT_ARGS "oplus_fusion_light_args.xml"
#define STK32600_ARGS "oplus_fusion_light_args_2.xml"
#define TCS3701_ARGS "oplus_fusion_light_args.xml"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace implementation {

XMLElement* AlsCorrection::argsElement = nullptr;
bool AlsCorrection::mloadArgsFromXMLAlready = false;
struct panel_info p_info = {};

float AlsCorrection::RMax = 0.0f;
float AlsCorrection::RMaxCal = 0.0f;
float AlsCorrection::RComp1 = 0.0f;
float AlsCorrection::RComp2 = 0.0f;
float AlsCorrection::RComp3 = 0.0f;
float AlsCorrection::RCompDel = 0.0f;

float AlsCorrection::GMax = 0.0f;
float AlsCorrection::GMaxCal = 0.0f;
float AlsCorrection::GComp1 = 0.0f;
float AlsCorrection::GComp2 = 0.0f;
float AlsCorrection::GComp3 = 0.0f;
float AlsCorrection::GCompDel = 0.0f;

float AlsCorrection::BMax = 0.0f;
float AlsCorrection::BMaxCal = 0.0f;
float AlsCorrection::BComp1 = 0.0f;
float AlsCorrection::BComp2 = 0.0f;
float AlsCorrection::BComp3 = 0.0f;
float AlsCorrection::BCompDel = 0.0f;

float AlsCorrection::WMax = 0.0f;
float AlsCorrection::WMaxCal = 0.0f;
float AlsCorrection::WComp1 = 0.0f;
float AlsCorrection::WComp2 = 0.0f;
float AlsCorrection::WComp3 = 0.0f;
float AlsCorrection::WCompDel = 0.0f;

float AlsCorrection::Grayscale1 = 0.0f;
float AlsCorrection::Grayscale2 = 0.0f;
float AlsCorrection::Grayscale3 = 0.0f;

float AlsCorrection::LevelCalArg = 0.0f;
float AlsCorrection::RawRouCoeLevel1 = 0.0f;
float AlsCorrection::RawRouCoeLevel2 = 0.0f;
float AlsCorrection::RawRouCoeLevel3 = 0.0f;
float AlsCorrection::RawRouCoeLevel4 = 0.0f;
float AlsCorrection::CalCoe = 0.0f;

int AlsCorrection::RetType = 0;
int AlsCorrection::ParagraphCount = 0;
float AlsCorrection::SeperatePoint1 = 0.0f;
float AlsCorrection::SeperatePoint2 = 0.0f;
float AlsCorrection::SeperatePoint3 = 0.0f;
float AlsCorrection::SeperatePoint4 = 0.0f;
float AlsCorrection::SP1value1 = 0.0f;
float AlsCorrection::SP1value2 = 0.0f;
float AlsCorrection::SP2value1 = 0.0f;
float AlsCorrection::SP2value2 = 0.0f;
float AlsCorrection::SP3value1 = 0.0f;
float AlsCorrection::SP3value2 = 0.0f;
float AlsCorrection::SP4value1 = 0.0f;
float AlsCorrection::SP4value2 = 0.0f;
float AlsCorrection::SP5value1 = 0.0f;
float AlsCorrection::SP5value2 = 0.0f;

static const std::string rgbw_max_lux_paths[4] = {
        ALS_CALI_DIR "red_max_lux",
        ALS_CALI_DIR "green_max_lux",
        ALS_CALI_DIR "blue_max_lux",
        ALS_CALI_DIR "white_max_lux",
};

struct als_config {
    bool hbr;
    float rgbw_max_lux[4];
    float rgbw_max_lux_div[4];
    float rgbw_lux_postmul[4];
    float rgbw_poly[4][4];
    float grayscale_weights[3];
    float sensor_gaincal_points[4];
    float sensor_inverse_gain[4];
    float agc_threshold;
    float calib_gain;
    float max_brightness;
};

static struct {
    float middle;
    float min, max;
} hysteresis_ranges[] = {
        {0, 0, 4},
        {7, 1, 12},
        {15, 5, 30},
        {30, 10, 50},
        {360, 25, 700},
        {1200, 300, 1600},
        {2250, 1000, 2940},
        {4600, 2000, 5900},
        {10000, 4000, 80000},
        {HUGE_VALF, 8000, HUGE_VALF},
};

static struct {
    nsecs_t last_update, last_forced_update;
    bool force_update;
    float hyst_min, hyst_max;
    float last_corrected_value;
    float last_agc_gain;
} state = {
        .last_update = 0,
        .force_update = true,
        .hyst_min = -1.0,
        .hyst_max = -1.0,
        .last_agc_gain = 0.0,
};

static als_config conf;
static std::shared_ptr<IAreaCapture> service;

template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;

    file >> result;
    return file.fail() ? def : result;
}

void AlsCorrection::loadRGBW(XMLElement* argsElement) {
    const char* rgbwColors[] = {"R", "G", "B", "W"};
    float* maxValues[] = {&RMax, &GMax, &BMax, &WMax};
    float* maxCalValues[] = {&RMaxCal, &GMaxCal, &BMaxCal, &WMaxCal};
    float* comp1Values[] = {&RComp1, &GComp1, &BComp1, &WComp1};
    float* comp2Values[] = {&RComp2, &GComp2, &BComp2, &WComp2};
    float* comp3Values[] = {&RComp3, &GComp3, &BComp3, &WComp3};
    float* compDelValues[] = {&RCompDel, &GCompDel, &BCompDel, &WCompDel};

    for (int i = 0; i < 4; ++i) {
        XMLElement* colorElement = argsElement->FirstChildElement(rgbwColors[i]);
        XMLElement* currentElement = colorElement->FirstChildElement();

        if (currentElement) *maxValues[i] = std::stof(currentElement->GetText());
        currentElement = currentElement->NextSiblingElement();

        if (currentElement) *maxCalValues[i] = std::stof(currentElement->GetText());
        currentElement = currentElement->NextSiblingElement();

        if (currentElement) *comp1Values[i] = std::stof(currentElement->GetText());
        currentElement = currentElement->NextSiblingElement();

        if (currentElement) *comp2Values[i] = std::stof(currentElement->GetText());
        currentElement = currentElement->NextSiblingElement();

        if (currentElement) *comp3Values[i] = std::stof(currentElement->GetText());
        currentElement = currentElement->NextSiblingElement();

        if (currentElement) *compDelValues[i] = std::stof(currentElement->GetText());
    }
}

void AlsCorrection::loadGrayAndCal(XMLElement* argsElement) {
    XMLElement* grayElement = argsElement->FirstChildElement("Gray");
    XMLElement* calElement = argsElement->FirstChildElement("Cal");

    if (grayElement) {
        XMLElement* grayscale1Element = grayElement->FirstChildElement("Grayscale1");
        XMLElement* grayscale2Element = grayElement->FirstChildElement("Grayscale2");
        XMLElement* grayscale3Element = grayElement->FirstChildElement("Grayscale3");

        if (grayscale1Element && grayscale2Element && grayscale3Element) {
            Grayscale1 = std::stof(grayscale1Element->GetText());
            Grayscale2 = std::stof(grayscale2Element->GetText());
            Grayscale3 = std::stof(grayscale3Element->GetText());
        }
    }

    if (calElement) {
        XMLElement* levelCalArgElement = calElement->FirstChildElement("LevelCalArg");
        XMLElement* rawRouCoeLevel1Element = calElement->FirstChildElement("RawRouCoeLevel1");
        XMLElement* rawRouCoeLevel2Element = calElement->FirstChildElement("RawRouCoeLevel2");
        XMLElement* rawRouCoeLevel3Element = calElement->FirstChildElement("RawRouCoeLevel3");
        XMLElement* rawRouCoeLevel4Element = calElement->FirstChildElement("RawRouCoeLevel4");
        XMLElement* calCoeElement = calElement->FirstChildElement("CalCoe");

        if (levelCalArgElement && rawRouCoeLevel1Element && rawRouCoeLevel2Element &&
            rawRouCoeLevel3Element && rawRouCoeLevel4Element && calCoeElement) {
            LevelCalArg = std::stof(levelCalArgElement->GetText());
            RawRouCoeLevel1 = std::stof(rawRouCoeLevel1Element->GetText());
            RawRouCoeLevel2 = std::stof(rawRouCoeLevel2Element->GetText());
            RawRouCoeLevel3 = std::stof(rawRouCoeLevel3Element->GetText());
            RawRouCoeLevel4 = std::stof(rawRouCoeLevel4Element->GetText());
            CalCoe = std::stof(calCoeElement->GetText());
        }
    }
}

void AlsCorrection::loadSeperateLuxParameters(XMLElement* argsElement) {
    XMLElement* seperateLuxElement = argsElement->FirstChildElement("SeperateLux");
    if (!seperateLuxElement) {
        return;
    }

    seperateLuxElement->QueryIntAttribute("RetType", &RetType);
    seperateLuxElement->QueryIntAttribute("ParagraphCount", &ParagraphCount);
    seperateLuxElement->QueryFloatAttribute("SeperatePoint1", &SeperatePoint1);
    seperateLuxElement->QueryFloatAttribute("SeperatePoint2", &SeperatePoint2);
    seperateLuxElement->QueryFloatAttribute("SeperatePoint3", &SeperatePoint3);
    seperateLuxElement->QueryFloatAttribute("SeperatePoint4", &SeperatePoint4);
    seperateLuxElement->QueryFloatAttribute("SP1value1", &SP1value1);
    seperateLuxElement->QueryFloatAttribute("SP1value2", &SP1value2);
    seperateLuxElement->QueryFloatAttribute("SP2value1", &SP2value1);
    seperateLuxElement->QueryFloatAttribute("SP2value2", &SP2value2);
    seperateLuxElement->QueryFloatAttribute("SP3value1", &SP3value1);
    seperateLuxElement->QueryFloatAttribute("SP3value2", &SP3value2);
    seperateLuxElement->QueryFloatAttribute("SP4value1", &SP4value1);
    seperateLuxElement->QueryFloatAttribute("SP4value2", &SP4value2);
    seperateLuxElement->QueryFloatAttribute("SP5value1", &SP5value1);
    seperateLuxElement->QueryFloatAttribute("SP5value2", &SP5value2);
}

// Placeholder function for now
void AlsCorrection::initDisplayParams() {
    int mOplusDisplayFd = open("/dev/oplus_display", O_RDWR);
    if (mOplusDisplayFd < 0) {
        ALOGE("Failed to open /dev/oplus_display: %s", strerror(errno));
        return;
    }

    if (ioctl(mOplusDisplayFd, PANEL_IOCTL_GET_PANELINFO, &p_info) != 0) {
        ALOGE("Failed to retrieve panel information: %s", strerror(errno));
        close(mOplusDisplayFd);
        return;
    }

    ALOGI("Panel version: %s", p_info.version);
    ALOGI("Panel manufacture: %s", p_info.manufacture);

    close(mOplusDisplayFd);
}

void AlsCorrection::init(const char* sensorName) {
    const char* xmlPath = ALS_ARGS_DIR DEFAULT_ARGS;
    if (strstr(sensorName, "stk32600") != nullptr) {
        xmlPath = ALS_ARGS_DIR STK32600_ARGS;
    } else if (strstr(sensorName, "tcs3701") != nullptr) {
        xmlPath = ALS_ARGS_DIR TCS3701_ARGS;
    }
    ALOGI("Using XML Path: %s", xmlPath);

    XMLDocument xmlDoc;
    initDisplayParams();
    int loadResult = xmlDoc.LoadFile(xmlPath);
    if (loadResult != XML_SUCCESS) {
        ALOGE("loadArgsFromXML: XML loading failed");
        return;
    }
    XMLElement* root = xmlDoc.FirstChildElement("Attributes");
    XMLElement* argsElement = root->FirstChildElement("Args");
    loadGrayAndCal(argsElement);
    loadRGBW(argsElement);
    // loadArraysFromXML(argsElement);
    loadSeperateLuxParameters(argsElement);
    // loadFPAlphaFunctionParameters(argsElement);
    // loadDCFunctionParameters(argsElement);
    // loadPWMFunctionParameters(argsElement);
    // loadSpecialCustomParameters(argsElement);

    float rgbw_acc = 0.0;
    for (int i = 0; i < 4; i++) {
        float max_lux = get(rgbw_max_lux_paths[i], 0.0);
        if (max_lux != 0.0) {
            conf.rgbw_max_lux[i] = max_lux;
        }
        if (i < 3) {
            rgbw_acc += conf.rgbw_max_lux[i];
            conf.rgbw_lux_postmul[i] = conf.rgbw_max_lux[i] / conf.rgbw_max_lux_div[i];
        } else {
            rgbw_acc -= conf.rgbw_max_lux[i];
            conf.rgbw_lux_postmul[i] = rgbw_acc / conf.rgbw_max_lux_div[i];
        }
    }
    ALOGI("Display maximums: R=%.0f G=%.0f B=%.0f W=%.0f", conf.rgbw_max_lux[0],
          conf.rgbw_max_lux[1], conf.rgbw_max_lux[2], conf.rgbw_max_lux[3]);

    float row_coe = get(ALS_CALI_DIR "row_coe", 0.0);
    if (row_coe != 0.0) {
        conf.sensor_inverse_gain[0] = row_coe / 1000.0;
    }
    conf.agc_threshold = 800.0 / conf.sensor_inverse_gain[0];

    float cali_coe = get(ALS_CALI_DIR "cali_coe", 0.0);
    conf.calib_gain = cali_coe > 0.0 ? cali_coe / 1000.0 : 1.0;
    ALOGI("Calibrated sensor gain: %.2fx", 1.0 / (conf.calib_gain * conf.sensor_inverse_gain[0]));

    //conf.max_brightness = get(BRIGHTNESS_DIR "max_brightness", 2050.0);

    for (auto& range : hysteresis_ranges) {
        range.min /= conf.calib_gain * conf.sensor_inverse_gain[0];
        range.max /= conf.calib_gain * conf.sensor_inverse_gain[0];
    }
    hysteresis_ranges[0].min = -1.0;

    const auto instancename = std::string(IAreaCapture::descriptor) + "/default";

    if (AServiceManager_isDeclared(instancename.c_str())) {
        service = IAreaCapture::fromBinder(
                ::ndk::SpAIBinder(AServiceManager_waitForService(instancename.c_str())));
    } else {
        ALOGE("Service is not registered");
    }
}

// Things to keep in mind with ALS correction:
// Screenshotting does not account for how accurate the colors look on the display.
// Do not taint screenshotting and brightness before correction.
// Obtain RGB Brightness as float
// Brightness as alpha ?
float AlsCorrection::process(Event& event) {
    static AreaRgbCaptureResult screenshot = {0.0, 0.0, 0.0};
    static float cachedValue = 0.0;
    static float RComp = 0.0f;
    static float GComp = 0.0f;
    static float BComp = 0.0f;
    float brightness = get(BRIGHTNESS_DIR "brightness", 0.0);
    ALOGI("Raw sensor reading: %.0f", event.u.scalar);

    if (service == nullptr || !service->getAreaBrightness(&screenshot).isOk()) {
        ALOGE("Could not get area above sensor, returning raw sensor value");
        return event.u.scalar;
    } else {
        ALOGI("Screen color above sensor: %f %f %f", screenshot.r, screenshot.g, screenshot.b);
    }

    if (screenshot.r + screenshot.g + screenshot.b == 0) {
        ALOGI("Nothing is being displayed over the light sensor, returning raw sensor value");
        cachedValue = event.u.scalar;
        return event.u.scalar;
    }

    if (screenshot.r > 255 && screenshot.g > 255 && screenshot.b > 255) {
        ALOGI("Displaying white, calculate only white compensation");
    }

    if (screenshot.r) {
        // 982
        //screenshot.r = screenshot.r / 255.0f * RMax;
        RComp = std::pow(screenshot.r, 3.0) * 0.000028039 + std::pow(screenshot.r, 2.0) * 0.00706169 +
                       screenshot.r * 0.232423949;
        RComp = RComp * (brightness / 2050.0);
    } else {
        RComp = 0.0f;
    }
    if (RComp < 0.0f) {
        RComp = 0.0f;
    }
    ALOGI("Brightness: %f", brightness);
    if (screenshot.g) {
       //1340
        //screenshot.g = screenshot.g / 255.0f * GMax;
        GComp = std::pow(screenshot.g, 3.0) * 0.000009249 + std::pow(screenshot.g, 2.0) * 0.019134599 +
                       screenshot.r * -0.236240034;
        GComp = GComp * (brightness / 2050.0);
    } else {
        GComp = 0.0f;
    }
    if (GComp < 0.0f) {
        GComp = 0.0f;
    }
    if (screenshot.b) {
        //1130
        //screenshot.b = screenshot.b / 255.0f * BMax;
        BComp = std::pow(screenshot.b, 3.0) * -0.000003045 + std::pow(screenshot.b, 2.0) * 0.020133014 +
                       screenshot.b * -0.494757082;
        BComp = BComp * (brightness / 2050.0);
    } else {
        BComp = 0.0f;
    }
    if (BComp < 0.0f) {
        BComp = 0.0f;
    }
    ALOGI("Compensations: %f %f %f", RComp, GComp, BComp);
    float totalCompensation = RComp + GComp + BComp;

    float adjustedLux = event.u.scalar - totalCompensation;
    if (adjustedLux < 0.0f) {
        adjustedLux = 0.0f;
    }

    cachedValue = adjustedLux;

    return adjustedLux;
    return event.u.scalar;
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
