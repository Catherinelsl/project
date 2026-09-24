#pragma once

#include <cstdint>
#include <functional>
#include <mqtt/client.h>
#include <mqtt/properties.h>
#include <string>
#include <unordered_map>
#include <map>

struct MQTTUserProperty
{
    std::string name;
    std::string value;
};

struct MQTTMessage
{
    std::string topic;
    std::string payload;
    mqtt::properties properties;
    std::map<std::string, std::string> userProperties;
    int qos = 1;
    bool retained = false;
};

class MqttClient
{
public:
    using MessageCallback = std::function<void(MQTTMessage& msg)>;

    MqttClient(const std::string& brokerAddr, const std::string& clientId);
    ~MqttClient();

    // connect
    bool connect(const std::string& username = "", const std::string& password = "");
    void disconnect();

    // 发送：直接传入MQTTMessage
    bool publish(const MQTTMessage& msg);

    bool subscribe(const std::string& topic, int qos =0);

    // 设置消息接收回调，底层paho消息转换为MQTTMessage
    void setOnMessageCallback(MessageCallback cb);

private:
    std::unique_ptr<mqtt::async_client> m_asyncClient;
    MessageCallback m_msgCallback;

    // 内部静态转发回调
    static void on_message_arrived(void* context, mqtt::const_message_ptr msg_ptr);

    // paho原生properties → 填充 userProperties（把paho里的user property拷贝进map）
    static void copyUserProperties(const mqtt::properties& srcProps, std::map<std::string,std::string>& outUserProps);

    // MQTTMessage → 构建paho::message
    static mqtt::message_ptr buildPahoMessage(const MQTTMessage& msg);
};
