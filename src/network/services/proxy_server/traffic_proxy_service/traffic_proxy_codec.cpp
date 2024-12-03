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

template <>
void TrafficEncoder::EncodeCommandFrame<UpdateTrafficProxyRequest>(TrafficProxyDataType& dstBuffer, const UpdateTrafficProxyRequest& command_frame)
{
    using SizeType = decltype(dstBuffer.GetSize());
    SizeType size  = sizeof(command_frame.op) + sizeof(SizeType) * 3 +
                    +command_frame.hostInfo.token.GetSize() +
                    command_frame.proxyServiceName.GetSize() + sizeof(command_frame.req);

    for (const auto& item : command_frame.hostInfo.hosts)
    {
        size += sizeof(SizeType) * 3 +
                item.first.GetSize() +
                item.second.host.GetSize() +
                item.second.service.GetSize();
    }
    Fundamental::BufferWriter<SizeType> writer;
    dstBuffer.Reallocate(size);
    writer.SetBuffer(dstBuffer.GetData(), dstBuffer.GetSize());
    writer.WriteValue(&command_frame.op);
    writer.WriteValue(&command_frame.req);
    writer.WriteRawMemory(command_frame.proxyServiceName);
    writer.WriteRawMemory(command_frame.hostInfo.token);
    SizeType hostMapSize = static_cast<SizeType>(command_frame.hostInfo.hosts.size());
    writer.WriteValue(&hostMapSize);
    for (const auto& item : command_frame.hostInfo.hosts)
    {
        writer.WriteRawMemory(item.first);
        writer.WriteRawMemory(item.second.host);
        writer.WriteRawMemory(item.second.service);
    }
}

template <>
void TrafficEncoder::EncodeCommandFrame<UpdateTrafficProxyResponse>(TrafficProxyDataType& dstBuffer, const UpdateTrafficProxyResponse& command_frame)
{
    using SizeType = decltype(dstBuffer.GetSize());
    SizeType size  = sizeof(command_frame.op) + sizeof(command_frame.req);
    Fundamental::BufferWriter<SizeType> writer;
    dstBuffer.Reallocate(size);
    writer.SetBuffer(dstBuffer.GetData(), dstBuffer.GetSize());
    writer.WriteValue(&command_frame.op);
    writer.WriteValue(&command_frame.req);
}

template <>
void TrafficEncoder::EncodeCommandFrame<RemoveTrafficProxyRequest>(TrafficProxyDataType& dstBuffer, const RemoveTrafficProxyRequest& command_frame)
{
    using SizeType = decltype(dstBuffer.GetSize());
    SizeType size  = sizeof(command_frame.op) + sizeof(SizeType) +
                    command_frame.proxyServiceName.GetSize() +
                    sizeof(command_frame.req);
    Fundamental::BufferWriter<SizeType> writer;
    dstBuffer.Reallocate(size);
    writer.SetBuffer(dstBuffer.GetData(), dstBuffer.GetSize());
    writer.WriteValue(&command_frame.op);
    writer.WriteValue(&command_frame.req);
    writer.WriteRawMemory(command_frame.proxyServiceName);
}

template <>
void TrafficEncoder::EncodeCommandFrame<RemoveTrafficProxyResponse>(TrafficProxyDataType& dstBuffer, const RemoveTrafficProxyResponse& command_frame)
{
    using SizeType = decltype(dstBuffer.GetSize());
    SizeType size  = sizeof(command_frame.op) + sizeof(command_frame.req);
    Fundamental::BufferWriter<SizeType> writer;
    dstBuffer.Reallocate(size);
    writer.SetBuffer(dstBuffer.GetData(), dstBuffer.GetSize());
    writer.WriteValue(&command_frame.op);
    writer.WriteValue(&command_frame.req);
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

template <>
bool TrafficDecoder::DecodeCommandFrame<UpdateTrafficProxyRequest>(const TrafficProxyDataType& srcBuffer, UpdateTrafficProxyRequest& dst_command_frame)
{
    try
    {
        using SizeType = decltype(srcBuffer.GetSize());
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(srcBuffer.GetData() + sizeof(TrafficProxyOperation),
                         srcBuffer.GetSize() - sizeof(TrafficProxyOperation));
        reader.ReadValue(&dst_command_frame.req);
        reader.ReadRawMemory(dst_command_frame.proxyServiceName);
        reader.ReadRawMemory(dst_command_frame.hostInfo.token);
        SizeType mapSize = 0;
        reader.ReadValue(&mapSize);
        for (SizeType i = 0; i < mapSize; ++i)
        {
            decltype(dst_command_frame.hostInfo.hosts)::key_type key;
            decltype(decltype(dst_command_frame.hostInfo.hosts)::value_type::second) value;
            reader.ReadRawMemory(key);
            reader.ReadRawMemory(value.host);
            reader.ReadRawMemory(value.service);
            dst_command_frame.hostInfo.hosts.emplace(std::move(key), std::move(value));
        }
    }
    catch (const std::exception& e)
    {
        FERR("decode command_frame failed for {}", e.what());
        return false;
    }
    return true;
}

template <>
bool TrafficDecoder::DecodeCommandFrame<UpdateTrafficProxyResponse>(const TrafficProxyDataType& srcBuffer, UpdateTrafficProxyResponse& dst_command_frame)
{
    try
    {
        using SizeType = decltype(srcBuffer.GetSize());
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(srcBuffer.GetData() + sizeof(TrafficProxyOperation),
                         srcBuffer.GetSize() - sizeof(TrafficProxyOperation));
        reader.ReadValue(&dst_command_frame.req);
    }
    catch (const std::exception& e)
    {
        FERR("decode command_frame failed for {}", e.what());
        return false;
    }
    return true;
}

template <>
bool TrafficDecoder::DecodeCommandFrame<RemoveTrafficProxyRequest>(const TrafficProxyDataType& srcBuffer, RemoveTrafficProxyRequest& dst_command_frame)
{
    try
    {
        using SizeType = decltype(srcBuffer.GetSize());
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(srcBuffer.GetData() + sizeof(TrafficProxyOperation),
                         srcBuffer.GetSize() - sizeof(TrafficProxyOperation));
        reader.ReadValue(&dst_command_frame.req);
        reader.ReadRawMemory(dst_command_frame.proxyServiceName);
    }
    catch (const std::exception& e)
    {
        FERR("decode command_frame failed for {}", e.what());
        return false;
    }
    return true;
}

template <>
bool TrafficDecoder::DecodeCommandFrame<RemoveTrafficProxyResponse>(const TrafficProxyDataType& srcBuffer, RemoveTrafficProxyResponse& dst_command_frame)
{
    try
    {
        using SizeType = decltype(srcBuffer.GetSize());
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(srcBuffer.GetData() + sizeof(TrafficProxyOperation),
                         srcBuffer.GetSize() - sizeof(TrafficProxyOperation));
        reader.ReadValue(&dst_command_frame.req);
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