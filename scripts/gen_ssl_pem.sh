#!/bin/bash
####################CA##########################

#生成 ca 私钥
openssl genrsa -out ca_root.key 2048

#生成 ca 自签名证书
openssl req -x509 -new -nodes -key ca_root.key -sha256 -days 3650 -out ca_root.crt -config openssl_ca.cnf



####################Server##########################
#生成服务器私钥
openssl genrsa -out server.key 2048


# 生成服务器证书请求
openssl req -new -key server.key -out server.csr -config openssl_server.cnf

# 使用ca签署服务器证书
openssl x509 -req -in server.csr -CA ca_root.crt -CAkey ca_root.key -CAcreateserial -out server.crt -days 3650 -sha256 -extfile openssl_server.cnf -extensions v3_req



####################Client##########################
#生成客户端私钥
openssl genrsa -out client.key 2048


# 生成客户端证书请求
openssl req -new -key client.key -out client.csr -config openssl_client.cnf

# 使用ca签署客户端证书
openssl x509 -req -in client.csr -CA ca_root.crt -CAkey ca_root.key -CAcreateserial -out client.crt -days 3650 -sha256


#生成dh密钥文件
openssl dhparam -out dh2048.pem 2048

#查看证书内容
# openssl x509 -in server.crt -text -noout

#查看dh参数
# openssl dhparam -in dh2048.pem -text -noout