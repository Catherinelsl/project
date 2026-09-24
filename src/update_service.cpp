#include "update_service.h"

#include "json_parser.h"
#include "message_schema.h"
#include "openssl_signer.h"

#include <iostream>
#include <map>
#include <utility>

UpdateService::UpdateService(
    MqttClient& client,
    std::string publicKey,
    std::string privateKey,
    std::string requestTopic)
    : client_(client),
      publicKey_(std::move(publicKey)),
      privateKey_(std::move(privateKey)),
      requestTopic_(std::move(requestTopic))
{
}

bool UpdateService::start()
{
    client_.setOnMessageCallback([this](MQTTMessage& message) {
        handleMessage(message);
    });

    return client_.subscribe(requestTopic_);
}

void UpdateService::handleMessage(MQTTMessage& message)
{
    try
    {
        auto name = message.userProperties.find("name");
        if(name == message.userProperties.end())
        {
            std::cerr << "missing user property [name], drop message" << std::endl;
            return;
        }

        auto signature = message.userProperties.find("signature");
        if(signature == message.userProperties.end())
        {
            std::cerr << "missing user property [signature], drop message" << std::endl;
            return;
        }

        if(name->second == "update/start")
        {
            handleUpdateStart(message, signature->second);
        }
        else if(name->second == "update/finish")
        {
            handleUpdateFinish(message, signature->second);
        }
        else
        {
            std::cerr << "unknown message name: " << name->second << std::endl;
        }
    }
    catch(...)
    {
        std::cerr << "exception occurred when processing mqtt incoming message" << std::endl;
    }
}

void UpdateService::handleUpdateStart(MQTTMessage& message, const std::string& signature)
{
    if(!verifyPayload(message, signature))
    {
        return;
    }

    UpdateStartRequest request = JsonParser::parseUpdateStartRequest(message.payload);
    UpdateStartResponse response;
    response.requestId = request.requestId;
    response.errorCode = 0;
    response.msg = "accepted, update started";

    std::string responseTopic;
    if(!getResponseTopic(message, responseTopic))
    {
        return;
    }

    auto responseJson = JsonParser::serializeUpdateStartResponse(response);
    publishResponse(responseTopic, responseJson, "update/start_resp");
}

void UpdateService::handleUpdateFinish(MQTTMessage& message, const std::string& signature)
{
    if(!verifyPayload(message, signature))
    {
        return;
    }

    UpdateFinishRequest request = JsonParser::parseUpdateFinishRequest(message.payload);
    UpdateFinishResponse response;
    response.requestId = request.requestId;
    response.errorCode = 0;
    response.msg = "finish report received";

    std::string responseTopic;
    if(!getResponseTopic(message, responseTopic))
    {
        return;
    }

    auto responseJson = JsonParser::serializeUpdateFinishResponse(response);
    mqtt::properties properties{{mqtt::property::CONTENT_TYPE, std::string("application/json")}};
    publishResponse(responseTopic, responseJson, "update/finish_resp", properties);
}

bool UpdateService::verifyPayload(const MQTTMessage& message, const std::string& signature) const
{
    if(!OpenSSLSigner::verifyBase64(message.payload, signature, publicKey_))
    {
        std::cerr << "signature verification failed, drop message" << std::endl;
        return false;
    }

    return true;
}

bool UpdateService::getResponseTopic(MQTTMessage& message, std::string& responseTopic) const
{
    if(!message.properties.contains(mqtt::property::code::RESPONSE_TOPIC))
    {
        std::cerr << "missing response topic in message properties, drop message" << std::endl;
        return false;
    }

    mqtt::property property = message.properties.get(mqtt::property::code::RESPONSE_TOPIC);
    responseTopic = mqtt::get<std::string>(property);
    return true;
}

void UpdateService::publishResponse(
    const std::string& responseTopic,
    const std::string& payload,
    const std::string& messageName,
    mqtt::properties properties)
{
    std::string signatureBase64 = OpenSSLSigner::signBase64(payload, privateKey_);
    std::map<std::string, std::string> userProperties{{"name", messageName}};
    userProperties["signature"] = signatureBase64;

    client_.publish(MQTTMessage{
        responseTopic,
        payload,
        properties,
        userProperties
    });
}