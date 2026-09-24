#include "application_paths.h"
#include "key_loader.h"
#include "mqtt_client.h"
#include "update_service.h"

#include <iostream>
#include <thread>

const std::string broker_address = "tcp://127.0.0.1:1883";
const std::string sub_topic = "service/request";

int main()
{
    MqttClient client(broker_address, "service-sut");
    if(!client.connect())
    {
        std::cerr << "failed to connect mqtt broker" << std::endl;
        return -1;
    }

    std::string appDir = getExecutableDirectory();
    std::string publicKey = loadTextFile(appDir + "/keyPair/public.pem");
    if(publicKey.empty())
    {
        std::cerr << "failed to load public key" << std::endl;
        return -1;
    }
    std::cout << "public key loaded successfully" << std::endl;

    std::string privateKey = loadTextFile(appDir + "/keyPair/private.pem");
    if(privateKey.empty())
    {
        std::cerr << "failed to load private key" << std::endl;
        return -1;
    }
    std::cout << "private key loaded successfully" << std::endl;

    UpdateService updateService(client, publicKey, privateKey, sub_topic);
    if(!updateService.start())
    {
        std::cerr << "failed to subscribe mqtt topic: " << sub_topic << std::endl;
        return -1;
    }

    std::cout << "Firmware service running, subscribe on " << sub_topic << std::endl;
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    client.disconnect();
    return 0;
}
