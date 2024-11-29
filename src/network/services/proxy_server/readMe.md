## proxy_server
## 通讯
```
struct Frame{
    uint32_t fixed:8;
    uint32_t version:8;
    uint32_t checkSum:8;
    uint32_t op:8;
    uint8_t mask[4];
    uint32_t payloadLen;
    uint8_t * payload;
}
//checkSum=payload[0]^payload[1]^.....^payload[payloadLen-1]
// finally reverse checkSum[bit 0-7]= checksum[bit 7-0]
```
## 功能描述
### 1.信息存储和查询
#### 描述
```
op=0
struct Header
{
    uint32_t cmdLen;
    uint32_t bodyLen;
    uint8_t * cmd;
    uint8_t * body;
};

struct CmdRequestBase
{
    std::string cmd;
    std::int32_t version;
    std::string id;
    std::int32_t seq;
};

struct CmdResponseBase
{
    std::string cmd;
    std::int32_t seq;
    std::int32_t code;
    std::string description;
};
```
#### 1) 信息存储
```
cmd=store
max_body_len=32k
```

#### 2）信息查询
```
cmd=query
```

### 2.聚合流量代理
```
op=0
struct proxyBody
{
    std::string serviceName;
    std::string id;
    std::string field;
};

enum AccessType
{
    Internal,
    External
};

struct ProxyInfo
{
    AccessType type;
    std::string host;
    std::string port;
};

struct RegisterService
{
    std::string serviceName;
    std::string id;
    std::string field;
    std::vector<ProxyInfo> proxyInfoList;
}
```