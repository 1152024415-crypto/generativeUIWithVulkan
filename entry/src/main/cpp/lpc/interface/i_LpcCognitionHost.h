#ifndef I_LPCCOGNITIONHOST_H
#define I_LPCCOGNITIONHOST_H

#include <memory>
#include <string>
#include <vector>
#include "LpcType.h"
#include "i_LpcListener.h"

namespace OHOS {
namespace Mhc {

class ILpcCognitionHost {
public:
    ILpcCognitionHost() = default;
    virtual ~ILpcCognitionHost() = default;
    virtual uint64_t GetHandle() = 0;
    virtual int32_t GetReportInfo(std::vector<MhcLpcReportInfo>& reportInfo) = 0;
    virtual int32_t SetReportOpt(MhcLpcReportTarget reportTarget, MhcLpcReportOpt reportOpt) = 0;
    virtual int32_t Start(MhcLpcReportTarget reportTarget, MhcLpcReportOpt reportOpt) = 0;
    virtual int32_t Stop(MhcLpcReportTarget reportTarget) = 0;
    virtual int32_t RegisterListener(const std::shared_ptr<ILpcListenr>& cognitionListener) = 0;
    virtual int32_t UnRegisterListener() = 0;
    virtual int32_t SetParameter(const std::string& k, const std::string& v) = 0;
    virtual int32_t GetParameter(const std::string& k, std::string& ret) = 0;
    virtual int32_t Wrtie(const void *data, uint32_t size) = 0;
    virtual int32_t Query(MhcLpcCognitionData& data) = 0;
};

} // namespace Mhc
} // namespace OHOS

#endif // I_LPCCOGNITIONHOST_H
