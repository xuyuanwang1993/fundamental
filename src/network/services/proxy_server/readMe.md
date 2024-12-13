## proxy_server
## 通讯
```
//数字采用小端字节序

struct Binary
{
    uint32_t len;
    uint8_t * data;
};

struct Frame{
    uint16_t fixed=0x6668;
    std::uint8_t checkSum=0;
    uint8_t version=0x01;
    uint8_t op=0x00;
    uint8_t mask[4];
    Binary payload;
}
ps:
checkSum=payload[0]^payload[1]^.....^payload[payloadLen-1]
负载部分见各op对应的功能描述

负责部分最大大小是512K
```
## 功能描述
### 1.信息存储和查询
#### 描述
```
op=kAgentOpcode
//数字采用小端字节序
enum AgentCode : std::int32_t
{
    AgentSuccess = 0,
    AgentFailed  = 1,
};

struct AgentRequestFrame
{
    Binary cmd;
    std::int32_t version = 0;
    Binary payload;
};

struct AgentResponseFrame
{
    Binary cmd;
    AgentCode code = AgentSuccess;
    Binary msg;
    Binary payload;
};
```
#### 1) 信息存储
```
cmd="update"

requet payload:
{
    Binary id;//数据字段一级key
    Binary section;//数据字段二级key
    Binary data;//数据
}

response paylod:
{

}
```

#### 2）信息查询
```
cmd="query"
requet payload:
{
    Binary id;//数据字段一级key
    Binary section;//数据字段二级key
}

response paylod:
{
    std::int64_t timestamp = 0;//数据时间戳
    Binary data;
}
```

### 2.聚合流量代理
```
//数字采用小端字节序
op=kTrafficProxyOpcode

enum TrafficProxyOperation : std::int32_t
{
    TrafficProxyDataOp = 0,
};

struct TrafficProxyHost
{
    Binary host;//eg，192.168.0.1
    Binary service;//eg, "http" or "80"
};

struct TrafficProxyHostInfo
{
    Binary token;
    map<Binary/*filed*/,TrafficProxyHost> hosts;
};
转发服务器会以这种格式存储相应的host信息 
map<Binary/*proxyServiceName*/,TrafficProxyHostInfo>;
```
#### 1) 请求流量代理访问
```
requet payload:
{
    TrafficProxyOperation op=TrafficProxyDataOp;
    Binary proxyServiceName;//服务名
    Binary field;//待访问的服务的域
    Binary token;//访问token
}
流量代理流程:
1.客户端在连接成功后在正常数据流程前发送相应的流量代理的数据帧至代理服务器
2.代理服务器收到请求后，查找服务，通过域和token获取到连接host，向对应host发起连接，建立两个连接流量中转映射，持续转发流量
ps:若当前域不存在对应的host信息，会尝试查找default域下的host
若不存在任何可用连接信息，连接会被断开


例：
代理服务器现在添加了以下的转发映射关系
服务名: test_http
{
    "token":"test_http_token",
    "hosts":[
        "github":{
            "host":"github.com",
            "service":"http"
        },
        "baidu":{
            "host":"www.baidu.com",
            "service":"80"
        },
        "default":{
            "host":"www.default.com",
            "service":"8080"
        }
    ]
}

case1:
现在我们客户端要通过代理
访问github.com
则需构造请求
{
    op=TrafficProxyDataOp;
    proxyServiceName="test_http";
    field="github";
    token="test_http_token";
}
在连接后先发送相应的转发帧至代理服务器

访问www.baidu.com
则请求改变为
{
    op=TrafficProxyDataOp;
    proxyServiceName="test_http";
    field="baidu";
    token="test_http_token";
}
case2:
若field不存在，请求如下
{
    op=TrafficProxyDataOp;
    proxyServiceName="test_http";
    field="not_existed";
    token="test_http_token";
}
此时代理服务会尝试去访问www.default.com:8080
若没有default项，连接被断开
case3:
若token不匹配，连接断开
若服务端token为空，则不校验转发请求的token
case4:
若proxyServiceName不存在，连接断开
```