#include "traffic_proxy_codec.hpp"
#include "fundamental/basic/log.h"
namespace network
{
namespace proxy
{
void TrafficEncoder::EncodeProxyRequest(TrafficProxyDataType& dstBuffer, const TrafficProxyRequest& request)
{
    using SizeType = decltype(dstBuffer.GetSize());
    SizeType size  = sizeof(request.op)+sizeof(SizeType) * 3 +
                    request.field.GetSize() +
                    request.proxyServiceName.GetSize() +
                    request.token.GetSize();
    Fundamental::BufferWriter<SizeType> writer;
    dstBuffer.Reallocate(size);
    writer.SetBuffer(dstBuffer.GetData(), dstBuffer.GetSize());
    writer.WriteValue(&request.op);
    writer.WriteRawMemory(request.proxyServiceName);
    writer.WriteRawMemory(request.field);
    writer.WriteRawMemory(request.token);
}

bool TrafficDecoder::DecodeProxyRequest(const TrafficProxyDataType& srcBuffer, TrafficProxyRequest& dstRequest)
{
    try
    {
        using SizeType = decltype(srcBuffer.GetSize());
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(srcBuffer.GetData(), srcBuffer.GetSize());
        reader.ReadValue(&dstRequest.op);
        reader.ReadRawMemory(dstRequest.proxyServiceName);
        reader.ReadRawMemory(dstRequest.field);
        reader.ReadRawMemory(dstRequest.token);
    }
    catch (const std::exception& e)
    {
        FERR("decode request failed for {}", e.what());
        return false;
    }
    return true;
}
} // namespace proxy
} // namespace network