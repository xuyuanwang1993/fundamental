#include "traffic_proxy_codec.hpp"
#include "fundamental/basic/log.h"
namespace network
{
namespace proxy
{
// encode
template <>
void TrafficEncoder::EncodeCommandFrame<TrafficProxyRequest>(TrafficProxyDataType& dstBuffer, const TrafficProxyRequest& command_frame)
{
    using SizeType = decltype(dstBuffer.GetSize());
    SizeType size  = sizeof(command_frame.op) + sizeof(SizeType) * 3 +
                    command_frame.field.GetSize() +
                    command_frame.proxyServiceName.GetSize() +
                    command_frame.token.GetSize();
    Fundamental::BufferWriter<SizeType> writer;
    dstBuffer.Reallocate(size);
    writer.SetBuffer(dstBuffer.GetData(), dstBuffer.GetSize());
    writer.WriteValue(&command_frame.op);
    writer.WriteRawMemory(command_frame.proxyServiceName);
    writer.WriteRawMemory(command_frame.field);
    writer.WriteRawMemory(command_frame.token);
}

// decode
template <>
bool TrafficDecoder::DecodeCommandFrame<TrafficProxyRequest>(const TrafficProxyDataType& srcBuffer, TrafficProxyRequest& dst_command_frame)
{
    try
    {
        using SizeType = decltype(srcBuffer.GetSize());
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(srcBuffer.GetData() + sizeof(TrafficProxyOperation),
                         srcBuffer.GetSize() - sizeof(TrafficProxyOperation));
        reader.ReadRawMemory(dst_command_frame.proxyServiceName);
        reader.ReadRawMemory(dst_command_frame.field);
        reader.ReadRawMemory(dst_command_frame.token);
    }
    catch (const std::exception& e)
    {
        FERR("decode command_frame failed for {}", e.what());
        return false;
    }
    return true;
}



} // namespace proxy
} // namespace network