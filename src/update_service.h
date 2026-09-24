#pragma once

#include "mqtt_client.h"

#include <string>

class UpdateService
{
public:
    UpdateService(
        MqttClient& client,
        std::string publicKey,
        std::string privateKey,
        std::string requestTopic);

    bool start();

private:
    void handleMessage(MQTTMessage& message);
    void handleUpdateStart(MQTTMessage& message, const std::string& signature);
    void handleUpdateFinish(MQTTMessage& message, const std::string& signature);
    bool verifyPayload(const MQTTMessage& message, const std::string& signature) const;
    bool getResponseTopic(MQTTMessage& message, std::string& responseTopic) const;
    void publishResponse(
        const std::string& responseTopic,
        const std::string& payload,
        const std::string& messageName,
        mqtt::properties properties = mqtt::properties{});

    MqttClient& client_;
    std::string publicKey_;
    std::string privateKey_;
    std::string requestTopic_;
};