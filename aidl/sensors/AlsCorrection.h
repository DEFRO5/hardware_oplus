#pragma once

#include <tinyxml2.h>
#include <aidl/vendor/lineage/oplus_als/BnAreaCapture.h>
#include <android/hardware/sensors/2.1/types.h>

using tinyxml2::XMLElement;

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace implementation {

static constexpr int SENSOR_TYPE_QTI_WISE_LIGHT = 33171103;

class AlsCorrection {
  public:
    static void loadRGBW(XMLElement* argsElement);
    static void loadGrayAndCal(XMLElement* argsElement);
    static void loadSeperateLuxParameters(XMLElement* argsElement);
    static void init(const char* sensorName);
    static void initDisplayParams();
    static float process(Event& event);

  private:
    static bool mloadArgsFromXMLAlready;
    static XMLElement* argsElement;

    static float RMax, GMax, BMax, WMax;
    static float RMaxCal, GMaxCal, BMaxCal, WMaxCal;
    static float RComp1, RComp2, RComp3, RCompDel;
    static float GComp1, GComp2, GComp3, GCompDel;
    static float BComp1, BComp2, BComp3, BCompDel;
    static float WComp1, WComp2, WComp3, WCompDel;

    static float Grayscale1, Grayscale2, Grayscale3;

    static float LevelCalArg, RawRouCoeLevel1, RawRouCoeLevel2, RawRouCoeLevel3, RawRouCoeLevel4, CalCoe;

    static int RetType, ParagraphCount;
    static float SeperatePoint1, SeperatePoint2, SeperatePoint3, SeperatePoint4;
    static float SP1value1, SP1value2, SP2value1, SP2value2;
    static float SP3value1, SP3value2, SP4value1, SP4value2;
    static float SP5value1, SP5value2;
    static int SeperateLuxThreshold;

    int mOplusDisplayFd;
};

}  // namespace implementation
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
