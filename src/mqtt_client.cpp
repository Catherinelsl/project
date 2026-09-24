#include "mqtt_client.h"
#include <mqtt/connect_options.h>

#include <tuple>

MqttClient::MqttClient(const std::string& brokerAddr, const std::string& clientId)
{
    // User properties (and every other MQTT v5 property) only exist in MQTT v5.
    // Paho defaults to MQTT 3.1.1, and a broker must not put any property into a
    // packet that it sends to a 3.1.1 client, so the properties would silently
    // arrive empty no matter what the publisher sent.
    mqtt::create_options createOpts(MQTTVERSION_5);
    m_asyncClient = std::make_unique<mqtt::async_client>(brokerAddr, clientId, createOpts);
}

MqttClient::~MqttClient()
{
    if(m_asyncClient->is_connected())
    {
        disconnect();
    }
}

bool MqttClient::connect(const std::string &username, const std::string &password)
{
    mqtt::connect_options opts;
    opts.set_mqtt_version(MQTTVERSION_5);
    opts.set_keep_alive_interval(20);
    if(!username.empty())
    {
        opts.set_user_name(username);
        opts.set_password(password);
    }

    try
    {
        m_asyncClient->connect(opts)->wait();
        m_asyncClient->set_message_callback(
            [this](mqtt::const_message_ptr m){
                on_message_arrived(this, m);
            });
        return true;
    }
    catch(std::exception &e)
    {
        return false;
    }
}

void MqttClient::disconnect()
{
    try
    {
        m_asyncClient->disconnect()->wait();
    }
    catch(...)
    {}
}

bool MqttClient::subscribe(const std::string &topic, int qos)
{
    try
    {
        m_asyncClient->subscribe(topic, qos)->wait();
        return true;
    }
    catch(...)
    {
        return false;
    }
}

void MqttClient::setOnMessageCallback(MqttClient::MessageCallback cb)
{
    m_msgCallback = std::move(cb);
}

void MqttClient::copyUserProperties(const mqtt::properties& srcProps, std::map<std::string, std::string>& outUserProps)
{
    outUserProps.clear();
    if(!srcProps.contains(mqtt::property::USER_PROPERTY))
        return;

    auto count = srcProps.count(mqtt::property::USER_PROPERTY);
    for(std::size_t i = 0; i < count; ++i)
    {
        auto userProperty = mqtt::get<mqtt::string_pair>(srcProps, mqtt::property::USER_PROPERTY, i);
        outUserProps[std::get<0>(userProperty)] = std::get<1>(userProperty);
    }
}

mqtt::message_ptr MqttClient::buildPahoMessage(const MQTTMessage &msg)
{
    mqtt::properties properties = msg.properties;

    for(auto &kv : msg.userProperties)
    {
        properties.add(mqtt::property(mqtt::property::USER_PROPERTY, kv.first, kv.second));
    }
    auto pahoMsg = mqtt::make_message(msg.topic, msg.payload, msg.qos, msg.retained);
    pahoMsg->set_properties(properties);
    return pahoMsg;
}

bool MqttClient::publish(const MQTTMessage &msg)
{
    if(!m_asyncClient->is_connected())
        return false;
    try
    {
        auto pahoMsg = buildPahoMessage(msg);
        // Deliberately do NOT wait on the delivery token here. publish() is
        // called from the message-arrived callback, which paho runs on its
        // receiver thread, and that same thread delivers the PUBACK that
        // completes the token: waiting for it there blocks forever. Returning
        // true therefore means "handed to the client", not "acknowledged by the
        // broker".
        m_asyncClient->publish(pahoMsg);
        return true;
    }
    catch(...)
    {
        return false;
    }
}

void MqttClient::on_message_arrived(void *context, mqtt::const_message_ptr msg_ptr)
{
    MqttClient* pThis = reinterpret_cast<MqttClient*>(context);
    if(!pThis || !pThis->m_msgCallback)
        return;

    MQTTMessage outMsg;
    outMsg.topic = msg_ptr->get_topic();
    outMsg.payload = msg_ptr->get_payload_str();
    outMsg.properties = msg_ptr->get_properties();

    copyUserProperties(outMsg.properties, outMsg.userProperties);

    pThis->m_msgCallback(outMsg);
}
