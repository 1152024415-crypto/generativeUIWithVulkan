#ifndef LPCTYPE_H
#define LPCTYPE_H

#include <cstdint>
#include <string>

namespace OHOS {
namespace Mhc {

enum MhcLpcCognitionType : int32_t {
    COGNITION_TYPE_BLOW = 0,
    COGNITION_TYPE_EMOTION = 1,
    COGNITION_TYPE_ENV_SOUND = 2,
    COGNITION_TYPE_NOISE_SOUND = 3,
    COGNITION_TYPE_FAN_SOUND = 4,
    COGNITION_TYPE_MUSIC = 5,
};

enum MhcLpcCognitionSubAttr : int32_t {
    NORMAL = 0,
    FOR_CALL,
};

enum MhcLpcReportMode : int32_t {
    REPORT_MODE_TRIGGED = 0,
    REPORT_MODE_ONCE = 1,
    REPORT_MODE_PERIOD = 2,
    REPORT_MODE_NONE = 3,
};

enum MhcLpcReportTarget : int32_t {
    AOD = 0,
    APP = 1,
};

enum MhcLpcErrorCode : int32_t {
    ERROR_FATAL = 0,
    ERROR_SERVICE_DISCONNECT = 1,
    ERROR_TIMEOUT = 2,
};

struct blow_cognition_res_msg {
    unsigned short sound_intensity;
    unsigned short sound_direction;
    unsigned short face_x_coord_min;
    unsigned short face_x_coord_max;
    unsigned short face_y_coord_min;
    unsigned short face_y_coord_max;
    unsigned int time_stamp_face_msg;
    unsigned int time_stamp_blow_res;
};

struct emotion_cognition_res_msg {
    unsigned int tag;
    unsigned int score;
};

struct env_sound_cognition_res_msg {
    unsigned int detect_result;
};

struct noise_sound_cognition_res_msg {
    unsigned short detect_result;
    unsigned short reserve[9];
};

struct fan_sound_cognition_res_msg {
    unsigned int detect_result;
};

const uint32_t MUSIC_SAMPLE_ID_LEN = 40;
struct music_cognition_res_msg {
    char sampleId[MUSIC_SAMPLE_ID_LEN];
    unsigned int detect_result;
} __attribute__((aligned(8)));

const uint32_t EMO_CLS_NUM_MAX = 19;
const uint32_t EMO_CACHE_NUM_MAX = 240;

struct EmotionRes {
    int8_t emotion;
    int8_t emoConfidence[EMO_CLS_NUM_MAX];
};

struct EmotionData {
    uint32_t modalType;
    uint32_t emoNum;
    EmotionRes emoRes[0];
};

struct MhcLpcCognitionDesc {
    MhcLpcCognitionType type;
    MhcLpcCognitionSubAttr subAttr;
};

struct MhcLpcReportOpt {
    MhcLpcReportMode reportMode;
    int32_t period;
} __attribute__((aligned(8)));

struct MhcLpcReportInfo {
    MhcLpcReportTarget reportTarget;
    MhcLpcReportOpt reportOpt;
} __attribute__((aligned(8)));

struct MhcLpcCognitionData {
    MhcLpcCognitionType eventId;
    int64_t timeStamp;
    const void *data;
    uint32_t dataSize;
};

} // namespace Mhc
} // namespace OHOS

#endif // LPCTYPE_H
