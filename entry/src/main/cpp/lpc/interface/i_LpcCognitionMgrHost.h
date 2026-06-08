#ifndef I_LPCCOGNITIONMGRHOST_H
#define I_LPCCOGNITIONMGRHOST_H

#include <functional>
#include <memory>
#include <vector>
#include "LpcType.h"
#include "i_LpcCognitionHost.h"

namespace OHOS {
namespace Mhc {

class ILpcCognitionMgrHost {
public:
    ILpcCognitionMgrHost() = default;
    virtual ~ILpcCognitionMgrHost() = default;
    virtual bool Init() = 0;
    virtual bool RegisterCognitionHDIDeathRecipient(std::function<void()> callback) = 0;
    virtual void DeregisterCognitionHDIDeathRecipient() = 0;
    virtual int32_t GetSupportedCognition(std::vector<MhcLpcCognitionType>& type) = 0;
    virtual std::shared_ptr<ILpcCognitionHost> CreateCognition(const MhcLpcCognitionDesc &desc) = 0;
    virtual int32_t ReleaseCognition(uint64_t handle) = 0;
};

} // namespace Mhc
} // namespace OHOS

#endif // I_LPCCOGNITIONMGRHOST_H
