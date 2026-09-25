import paho.mqtt.client as mqtt
import paho.mqtt.properties as properties
import paho.mqtt.packettypes as packettypes
import json
import time
import base64
import pytest
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

# === 配置项 ===
BROKER = "127.0.0.1"
PORT = 1883
REQUEST_TOPIC = "service/request"
PRIVATE_KEY_PATH = "build/bin/keyPair/private.pem"
PUBLIC_KEY_PATH = "build/bin/keyPair/public.pem"

# 全局变量用于接收响应
response_payload = None
response_name = None
response_signature = None

def on_message(client, userdata, msg):
    global response_payload, response_name, response_signature
    response_payload = msg.payload.decode('utf-8')
    if msg.properties and msg.properties.UserProperty:
        props_dict = dict(msg.properties.UserProperty)
        response_name = props_dict.get("name")
        response_signature = props_dict.get("signature")

@pytest.fixture
def mqtt_client():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, protocol=mqtt.MQTTv5)
    client.on_message = on_message
    client.connect(BROKER, PORT, 60)
    client.loop_start()
    yield client
    client.loop_stop()
    client.disconnect()

def generate_signature(payload_str: str) -> str:
    with open(PRIVATE_KEY_PATH, "rb") as key_file:
        private_key = serialization.load_pem_private_key(key_file.read(), password=None)
    signature = private_key.sign(
        payload_str.encode('utf-8'),
        padding.PKCS1v15(),
        hashes.SHA256()
    )
    return base64.b64encode(signature).decode('utf-8')

def verify_signature(payload_str: str, signature_b64: str) -> bool:
    with open(PUBLIC_KEY_PATH, "rb") as key_file:
        public_key = serialization.load_pem_public_key(key_file.read())
    try:
        public_key.verify(
            base64.b64decode(signature_b64),
            payload_str.encode('utf-8'),
            padding.PKCS1v15(),
            hashes.SHA256()
        )
        return True
    except Exception as e:
        print(f"验签失败: {e}")
        return False

def publish_request(client, payload_str: str, name: str = None, signature: str = None, response_topic: str = None):
    props = properties.Properties(packettypes.PacketTypes.PUBLISH)
    if response_topic is not None:
        props.ResponseTopic = response_topic
    
    user_props = []
    if name is not None:
        user_props.append(("name", name))
    if signature is not None:
        user_props.append(("signature", signature))
    if user_props:
        props.UserProperty = user_props
    
    client.publish(REQUEST_TOPIC, payload_str, properties=props)
    return payload_str

# ================= Task 2.2: 正常消息 =================

def test_update_start_success(mqtt_client):
    global response_payload, response_name, response_signature
    response_payload = response_name = response_signature = None
    
    resp_topic = "test/resp/update_start"
    mqtt_client.subscribe(resp_topic)
    
    # 提取 req_id 变量
    req_id = "req-001"
    
    req_data = {"requestId": req_id, "version": "1.2.3", "fileSize": 1048576}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    publish_request(mqtt_client, payload_str=payload_str, name="update/start", signature=signature, response_topic=resp_topic)
    time.sleep(2)
    
    assert response_payload is not None, "未能收到 update/start 的响应"
    assert response_name == "update/start_resp", f"响应 name 错误: {response_name}"
    assert verify_signature(response_payload, response_signature), "服务端响应签名验证失败！"
    
    resp_json = json.loads(response_payload)
    # 断言时使用变量进行比对
    assert resp_json["requestId"] == req_id, f"响应 requestId 错误: {resp_json['requestId']}, 期望: {req_id}"
    assert resp_json["errorCode"] == 0, f"响应 errorCode 错误: {resp_json['errorCode']}"
    assert "msg" in resp_json, "响应缺少 msg 字段"
    assert "accepted, update started" in resp_json["msg"], "响应 msg 内容不匹配"
    
    print(f"✅ Task 2.2 update/start 测试通过 (req_id: {req_id})")

def test_update_finish_success(mqtt_client):
    global response_payload, response_name, response_signature
    response_payload = response_name = response_signature = None
    
    resp_topic = "test/resp/update_finish"
    mqtt_client.subscribe(resp_topic)
    
    # 提取 req_id 变量
    req_id = "req-001"
    
    req_data = {"requestId": req_id, "version": "1.2.3", "success": True, "errorCode": 0}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    publish_request(mqtt_client, payload_str=payload_str, name="update/finish", signature=signature, response_topic=resp_topic)
    time.sleep(2)
    
    assert response_payload is not None, "未能收到 update/finish 的响应"
    assert response_name == "update/finish_resp", f"响应 name 错误: {response_name}"
    assert verify_signature(response_payload, response_signature), "服务端响应签名验证失败！"
    
    resp_json = json.loads(response_payload)
    assert resp_json["requestId"] == req_id, f"响应 requestId 错误: {resp_json['requestId']}, 期望: {req_id}"
    assert resp_json["errorCode"] == 0, f"响应 errorCode 错误: {resp_json['errorCode']}"
    assert "msg" in resp_json, "响应缺少 msg 字段"
    assert "finish report received" in resp_json["msg"], "响应 msg 内容不匹配"
    
    print(f"✅ Task 2.2 update/finish 测试通过 (req_id: {req_id})")

# ================= Task 2.3: 篡改签名 =================

def test_tampered_signature(mqtt_client):
    global response_payload
    response_payload = None
    
    resp_topic = "test/resp/tampered_sig"
    mqtt_client.subscribe(resp_topic)
    
    req_id = "req-001"
    req_data = {"requestId": req_id, "version": "1.2.3", "fileSize": 1048576}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    # 篡改签名
    tampered_sig = signature[:-2] + ("AA" if not signature.endswith("AA") else "BB")
    
    publish_request(mqtt_client, payload_str=payload_str, name="update/start", signature=tampered_sig, response_topic=resp_topic)
    time.sleep(2)
    
    assert response_payload is None, "签名被篡改，服务端不应发送响应！"
    print("✅ Task 2.3 篡改签名成功拦截")

# ================= Task 2.4: 错误 Payload 结构 =================

def test_invalid_payload_structure(mqtt_client):
    global response_payload
    response_payload = None
    
    resp_topic = "test/resp/bad_payload"
    mqtt_client.subscribe(resp_topic)
    
    req_id = "req-001"
    # 故意缺少必要字段，但签名有效
    req_data = {"requestId": req_id, "invalid_key": "wrong_data"}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    publish_request(mqtt_client, payload_str=payload_str, name="update/start", signature=signature, response_topic=resp_topic)
    time.sleep(2)
    
    assert response_payload is None, "结构错误的 Payload，服务端不应发送响应！"
    print("✅ Task 2.4 错误结构成功拦截")

# ================= Task 2.5: 其他错误处理 =================

def test_missing_name_property(mqtt_client):
    global response_payload
    response_payload = None
    
    resp_topic = "test/resp/missing_name"
    mqtt_client.subscribe(resp_topic)
    
    req_id = "req-001"
    req_data = {"requestId": req_id, "version": "1.2.3", "fileSize": 1048576}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    publish_request(mqtt_client, payload_str=payload_str, name=None, signature=signature, response_topic=resp_topic)
    time.sleep(2)
    
    assert response_payload is None, "缺少 name 属性，服务端不应发送响应！"
    print("✅ 缺少 name 属性成功拦截")

def test_missing_signature_property(mqtt_client):
    global response_payload
    response_payload = None
    
    resp_topic = "test/resp/missing_sig"
    mqtt_client.subscribe(resp_topic)
    
    req_id = "req-001"
    req_data = {"requestId": req_id, "version": "1.2.3", "fileSize": 1048576}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    
    publish_request(mqtt_client, payload_str=payload_str, name="update/start", signature=None, response_topic=resp_topic)
    time.sleep(2)
    
    assert response_payload is None, "缺少 signature 属性，服务端不应发送响应！"
    print("✅ 缺少 signature 属性成功拦截")

def test_missing_response_topic_property(mqtt_client):
    global response_payload
    response_payload = None
    
    mqtt_client.subscribe("test/resp/dummy")
    
    req_id = "req-001"
    req_data = {"requestId": req_id, "version": "1.2.3", "fileSize": 1048576}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    publish_request(mqtt_client, payload_str=payload_str, name="update/start", signature=signature, response_topic=None)
    time.sleep(2)
    
    assert response_payload is None, "缺少 Response Topic 属性，服务端不应发送响应！"
    print("✅ 缺少 Response Topic 属性成功拦截")

def test_unknown_name_property(mqtt_client):
    global response_payload
    response_payload = None
    
    resp_topic = "test/resp/unknown_name"
    mqtt_client.subscribe(resp_topic)
    
    req_id = "req-001"
    req_data = {"requestId": req_id, "version": "1.2.3", "fileSize": 1048576}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    publish_request(mqtt_client, payload_str=payload_str, name="unknown/command", signature=signature, response_topic=resp_topic)
    time.sleep(2)
    
    assert response_payload is None, "未知的 name 属性，服务端不应发送响应！"
    print("✅ 未知 name 属性成功拦截")

def test_unparseable_json_payload(mqtt_client):
    global response_payload
    response_payload = None
    
    resp_topic = "test/resp/unparseable_json"
    mqtt_client.subscribe(resp_topic)
    
    # 构造损坏的 JSON（缺少右括号）
    invalid_json_str = '{"requestId": "req-001", "version": "1.2.3"'
    signature = generate_signature(invalid_json_str)
    
    publish_request(mqtt_client, payload_str=invalid_json_str, name="update/start", signature=signature, response_topic=resp_topic)
    time.sleep(2)
    
    assert response_payload is None, "无法解析的 JSON，服务端不应发送响应！"
    print("✅ 无法解析的 JSON 成功拦截")

if __name__ == "__main__":
    pytest.main(["-v", "-s", __file__])