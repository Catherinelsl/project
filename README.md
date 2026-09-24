# MQTT Message Contract

This service uses MQTT v5 and exchanges JSON payloads on top of Paho MQTT.

## Broker

- Broker address: `tcp://127.0.0.1:1883`
- Client ID: `service-sut`
- Request topic: `service/request`
- Response topic: provided by the request message through the MQTT v5 `Response Topic` property

## Common MQTT Requirements

Every request message published to `service/request` must include:

- MQTT v5 `Response Topic` property: the topic where the service should publish the response
- User property `name`: logical message name
- User property `signature`: Base64 signature of the exact payload string

The service verifies the request payload with `keyPair/public.pem` before parsing it. The signature is generated and verified with OpenSSL EVP, using `SHA256` by default.

Every response message includes:

- Payload: JSON response body
- User property `name`: response message name
- User property `signature`: Base64 signature of the exact response payload string

The response payload is signed with `keyPair/private.pem`.

## `update/start` Request

Publish this message to `service/request`.

### MQTT Properties

| Type | Name | Value |
| --- | --- | --- |
| Property | Response Topic | Response topic chosen by the requester |
| User Property | `name` | `update/start` |
| User Property | `signature` | Base64 signature of the payload |

### Payload

```json
{
    "requestId": "req-001",
    "version": "1.2.3",
    "fileSize": 1048576
}
```

### Response

The response is published to the request message's MQTT v5 `Response Topic`.

#### User Properties

| Type | Name | Value |
| --- | --- | --- |
| User Property | `name` | `update/start_resp` |
| User Property | `signature` | Base64 signature of the response payload |

#### Payload

```json
{
    "requestId": "req-001",
    "errorCode": 0,
    "msg": "accepted, update started"
}
```

## `update/finish` Request

Publish this message to `service/request`.

### MQTT Properties

| Type | Name | Value |
| --- | --- | --- |
| Property | Response Topic | Response topic chosen by the requester |
| User Property | `name` | `update/finish` |
| User Property | `signature` | Base64 signature of the payload |

### Payload

```json
{
    "requestId": "req-001",
    "version": "1.2.3",
    "success": true,
    "errorCode": 0
}
```

### Response

The response is published to the request message's MQTT v5 `Response Topic`.

#### User Properties

| Type | Name | Value |
| --- | --- | --- |
| User Property | `name` | `update/finish_resp` |
| User Property | `signature` | Base64 signature of the response payload |

#### Payload

```json
{
    "requestId": "req-001",
    "errorCode": 0,
    "msg": "finish report received"
}
```

## Error Handling

The service drops a request without publishing a response when any of these conditions is true:

- User property `name` is missing
- User property `signature` is missing
- Payload signature verification fails
- MQTT v5 `Response Topic` property is missing
- Payload JSON cannot be parsed
- User property `name` is unknown

## Build

### Prerequisites

Install these tools before building the project:

- CMake 3.16 or newer
- A C++17 compiler
- Conan 2.x

On Windows, use a Visual Studio developer shell or another shell where the MSVC compiler is available.

### Configure and Build

From the repository root, run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

The CMake configure step runs Conan automatically and installs the dependencies declared in `conanfile.txt`:

- `paho-mqtt-cpp/1.4.1`
- `nlohmann_json/3.11.3`
- `openssl/3.3.2`

### Build Output

The executable is generated under:

```text
build/bin/service.exe
```

The post-build step copies the `keyPair` directory to:

```text
build/bin/keyPair
```

### Run

Before starting the service, make sure an MQTT v5 broker is running at:

```text
tcp://127.0.0.1:1883
```

Then run the service executable from the build output directory:

```sh
./build/bin/service.exe
```

The service expects these key files to exist next to the executable:

```text
keyPair/public.pem
keyPair/private.pem
```
