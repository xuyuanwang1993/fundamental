#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Fundamental
{

// NOTE: This buffer owns the life time of a block of raw memory.
template <typename _SizeType = std::size_t>
class Buffer
{
public:
    using SizeType = _SizeType;

public:
    Buffer() = default;

    explicit Buffer(_SizeType sizeInBytes);

    // NOTE: this method is same as std::vector::assign,
    // so data will be copied.
    explicit Buffer(std::uint8_t* pData,
                    _SizeType sizeInBytes);

    ~Buffer();

    // overwrite copy
    Buffer(const Buffer&);
    Buffer& operator=(const Buffer&);

    // Enable move
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;

    // when it allocated failed,an runtime error exception will be thrown
    // when you AttachRawMemory into buffer and you want to Reallocate for new size
    // you need to decide freeOriginalData to be true all false
    template <size_t AlignSize_ = 4, typename = std::enable_if_t<(AlignSize_ % 4 == 0 || AlignSize_ % 8 == 0), int>>
    void Reallocate(_SizeType sizeInBytes, bool freeOriginalData = true)
    {
        if (m_byteSize == sizeInBytes)
            return;
        if (sizeInBytes == 0)
        {
            FreeBuffer();
            return;
        }
        if (freeOriginalData)
            FreeBuffer();
        auto fixedSize       = AlignSize_ - (sizeInBytes % AlignSize_);
        std::uint8_t* newPtr = reinterpret_cast<std::uint8_t*>(std::realloc(m_pRaw, sizeInBytes + fixedSize));
        if (!newPtr)
            throw std::runtime_error(std::string("bad alloc for ") + std::to_string(sizeInBytes) + " bytes");
        if (fixedSize > 0)
            std::memset(newPtr + sizeInBytes, 0, fixedSize);
        m_pRaw     = newPtr;
        m_byteSize = sizeInBytes;
    }

    // NOTE: this method is same as std::vector::assign,
    // so data will be copied.
    // if you do not wish a copy, consider to use MemoryOwner instead
    void AssignBuffer(std::uint8_t* pData, _SizeType sizeInBytes);

    // This method will free current buffer
    // and assign the raw pointer and size directly without copy
    // NOTE: Assume pData is capable to be released by std::free
    void AttachRawMemory(std::uint8_t* pData,
                         _SizeType sizeInBytes);

    // You detach the raw memory's ownership by this method
    // raw memory is allocated by std::malloc
    // so you need to free the memory by std::free
    void DetachRawMemory(std::uint8_t** ppOutRawData,
                         _SizeType& outSizeInBytes);

    const SizeType& GetSize() const
    {
        return m_byteSize;
    }

    std::uintptr_t GetAddress() const;
    std::uint8_t* GetData() const;

    void FreeBuffer();

    operator bool() const;

    bool operator==(const std::string& str)
    {
        if (m_byteSize != str.size())
            return false;
        return 0 == std::memcmp(m_pRaw, str.data(), str.size());
    }

private:
    void Reset();

private:
    _SizeType m_byteSize = 0;
    std::uint8_t* m_pRaw = nullptr;
};

template <typename _SizeType>
Buffer<_SizeType>::Buffer(Buffer&& other) noexcept :
m_pRaw(other.m_pRaw),
m_byteSize(other.m_byteSize)
{
    if (&other == this)
        return;
    other.Reset();
}

template <typename _SizeType>
inline Buffer<_SizeType>& Buffer<_SizeType>::operator=(Buffer<_SizeType>&& other) noexcept
{
    if (&other == this)
        return *this;
    FreeBuffer();

    m_pRaw     = other.m_pRaw;
    m_byteSize = other.m_byteSize;

    other.Reset();
    return *this;
}

template <typename _SizeType>
Buffer<_SizeType>::Buffer(_SizeType sizeInBytes)
{
    Reallocate(sizeInBytes);
}

template <typename _SizeType>
Buffer<_SizeType>::Buffer(std::uint8_t* pData,
                          _SizeType sizeInBytes)
{
    AssignBuffer(pData, sizeInBytes);
}

template <typename _SizeType>
Buffer<_SizeType>::~Buffer()
{
    FreeBuffer();
}

template <typename _SizeType>
inline Buffer<_SizeType>::Buffer(const Buffer& other)
{
    if (&other == this)
        return;
    AssignBuffer(other.m_pRaw, other.m_byteSize);
}

template <typename _SizeType>
inline Buffer<_SizeType>& Buffer<_SizeType>::operator=(const Buffer& other)
{
    if (&other == this)
        return *this;
    AssignBuffer(other.m_pRaw, other.m_byteSize);
    return *this;
}

template <typename _SizeType>
void Buffer<_SizeType>::AssignBuffer(std::uint8_t* pData,
                                     _SizeType sizeInBytes)
{
    Reallocate(sizeInBytes);
    std::memcpy(m_pRaw, pData, m_byteSize);
}

template <typename _SizeType>
void Buffer<_SizeType>::AttachRawMemory(std::uint8_t* pData,
                                        _SizeType sizeInBytes)
{
    FreeBuffer();

    m_pRaw     = pData;
    m_byteSize = sizeInBytes;
}

template <typename _SizeType>
void Buffer<_SizeType>::DetachRawMemory(std::uint8_t** ppOutRawData,
                                        _SizeType& outSizeInBytes)
{
    if (ppOutRawData == nullptr)
        return;

    *ppOutRawData  = m_pRaw;
    outSizeInBytes = m_byteSize;
    Reset();
}

template <typename _SizeType>
std::uintptr_t Buffer<_SizeType>::GetAddress() const
{
    return reinterpret_cast<std::uintptr_t>(m_pRaw);
}

template <typename _SizeType>
std::uint8_t* Buffer<_SizeType>::GetData() const
{
    return m_pRaw;
}

template <typename _SizeType>
void Buffer<_SizeType>::FreeBuffer()
{
    if (operator bool())
    {
        std::free(m_pRaw);
    }
    Reset();
}

template <typename _SizeType>
Fundamental::Buffer<_SizeType>::operator bool() const
{
    return m_pRaw && m_byteSize > 0;
}

template <typename _SizeType>
void Buffer<_SizeType>::Reset()
{
    m_pRaw     = nullptr;
    m_byteSize = 0;
}

template <typename T>
struct BufferHash
{
    std::size_t operator()(const Buffer<T>& buf) const noexcept
    {
        std::size_t seed = 0;
        auto size        = buf.GetSize();
        auto ptr         = buf.GetData();
        for (typename Buffer<T>::SizeType i = 0; i < size; ++i)
        {
            seed ^= static_cast<std::size_t>(ptr[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
} // namespace Fundamental
