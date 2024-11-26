#include "fundamental/basic/utils.hpp"
#include "fundamental/basic/buffer.hpp"
#include <mutex>
#include <string>
#include <unordered_map>
namespace network
{
namespace proxy
{

using AgentSizeType=std::uint32_t;
using AgentDataType=Fundamental::Buffer<AgentSizeType>;
struct AgentEntryInfo
{
    std::int64_t timestamp;
    AgentDataType data;
};

using AgentItemMap = std::unordered_map<AgentDataType /*section*/, AgentEntryInfo,Fundamental::BufferHash<AgentSizeType>>;
using AgentMap     = std::unordered_map<AgentDataType /*id*/, AgentItemMap,Fundamental::BufferHash<AgentSizeType>>;
class AgentStorage : public Fundamental::Singleton<AgentStorage>
{
public:
    void UpdateAgentInfo(const AgentDataType& id, const AgentDataType& section, AgentDataType&& data);
    bool QueryAgentInfo(const AgentDataType& id, const AgentDataType& section, AgentEntryInfo& entry);
    void RemoveExpiredData(std::int64_t expiredSec=60);
private:
    std::mutex dataMutex;
    AgentMap storage;
};

} // namespace proxy
} // namespace network