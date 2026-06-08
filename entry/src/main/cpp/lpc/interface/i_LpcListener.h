#ifndef I_LPCLISTENER_H
#define I_LPCLISTENER_H

#include "LpcType.h"

namespace OHOS {
namespace Mhc {

class ILpcListenr {
public:
    ILpcListenr() = default;
    virtual ~ILpcListenr() = default;
    virtual int32_t OnEvent(const MhcLpcCognitionData &data) = 0;
    virtual int32_t OnError(const MhcLpcErrorCode errorType) = 0;
};

} // namespace Mhc
} // namespace OHOS

#endif // I_LPCLISTENER_H
