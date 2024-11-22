//
// @author xuyuanwang <469953258@qq.com> 2024/05
//
#pragma once
#include <rttr/type>
#include <set>
#include <string>
#include <string_view>

namespace Fundamental
{
using RttrControlMetaDataType = std::set<std::string>;
struct RttrMetaControlOption
{
    RttrControlMetaDataType excludeDatas;
    RttrControlMetaDataType includeDatas;
    static std::string ExcludeMetaDataKey();
    static std::string IncludeMetaDataKey();
    static bool HasInterSection(const RttrControlMetaDataType& data1, const RttrControlMetaDataType& data2);
    bool ValidateSerialize(const rttr::property &prop) const;
};

using RttrSerializeOption   = RttrMetaControlOption;
using RttrDeserializeOption = RttrMetaControlOption;

} // namespace Fundamental