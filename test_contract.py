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
    """接收服务端响应的回调函数"""
    global response_payload, response_name, response_signature
    response_payload = msg.payload.decode('utf-8')
    
    # 解析 MQTT v5 User Property
    if msg.properties and msg.properties.UserProperty:
        # UserProperty 是一个元组列表，如 [('name', 'update/start_resp'), ('signature', '...')]
        props_dict = dict(msg.properties.UserProperty)
        response_name = props_dict.get("name")
        response_signature = props_dict.get("signature")

@pytest.fixture
def mqtt_client():
    # 注意：必须使用 MQTTv5 协议，paho-mqtt 版本需 >= 2.0
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, protocol=mqtt.MQTTv5)
    client.on_message = on_message
    client.connect(BROKER, PORT, 60)
    client.loop_start()
    yield client
    client.loop_stop()
    client.disconnect()

def generate_signature(payload_str: str) -> str:
    """读取私钥，对载荷字符串进行 RSA SHA256 签名 (PKCS1v15填充)"""
    with open(PRIVATE_KEY_PATH, "rb") as key_file:
        private_key = serialization.load_pem_private_key(key_file.read(), password=None)
    signature = private_key.sign(
        payload_str.encode('utf-8'),
        padding.PKCS1v15(),
        hashes.SHA256()
    )
    return base64.b64encode(signature).decode('utf-8')

def verify_signature(payload_str: str, signature_b64: str) -> bool:
    """读取公钥，验证服务端返回的签名"""
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

def publish_request(client, name: str, payload_dict: dict, response_topic: str, sign: str = None):
    """发布 MQTT v5 请求的封装函数"""
    # 强制使用紧凑 JSON 格式 (separators=(',', ':'))，去掉所有空格
    payload_str = json.dumps(payload_dict, separators=(',', ':'))
    
    props = properties.Properties(packettypes.PacketTypes.PUBLISH)
    props.ResponseTopic = response_topic
    
    user_props = [("name", name)]
    if sign is not None:
        user_props.append(("signature", sign))
    props.UserProperty = user_props
    
    client.publish(REQUEST_TOPIC, payload_str, properties=props)
    return payload_str

# === Task 2.2: 验证正常消息 (update/start 和 update/finish) ===
def test_update_start_success(mqtt_client):
    global response_payload, response_name, response_signature
    response_payload = response_name = response_signature = None
    
    # 每次测试使用独立的响应 Topic，避免干扰
    resp_topic = "test/resp/update_start"
    mqtt_client.subscribe(resp_topic)
    
    # 构造请求
    req_data = {"requestId": "req-001", "version": "1.2.3", "fileSize": 1048576}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    publish_request(mqtt_client, "update/start", req_data, resp_topic, signature)
    time.sleep(2)
    
    assert response_payload is not None, "未能收到 update/start 的响应"
    assert response_name == "update/start_resp", f"响应 name 错误: {response_name}"
    assert verify_signature(response_payload, response_signature), "服务端响应签名验证失败！"
    
    # 校验 JSON 内容
    resp_json = json.loads(response_payload)
    assert resp_json["requestId"] == "req-001"
    assert resp_json["errorCode"] == 0
    assert "msg" in resp_json
    print("✅ Task 2.2 update/start 测试通过")

def test_update_finish_success(mqtt_client):
    global response_payload, response_name, response_signature
    response_payload = response_name = response_signature = None
    
    resp_topic = "test/resp/update_finish"
    mqtt_client.subscribe(resp_topic)
    
    req_data = {"requestId": "req-002", "version": "1.2.3", "success": True, "errorCode": 0}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    publish_request(mqtt_client, "update/finish", req_data, resp_topic, signature)
    time.sleep(2)
    
    assert response_payload is not None, "未能收到 update/finish 的响应"
    assert response_name == "update/finish_resp", f"响应 name 错误: {response_name}"
    assert verify_signature(response_payload, response_signature), "服务端响应签名验证失败！"
    print("✅ Task 2.2 update/finish 测试通过")

# === Task 2.3: 篡改签名验证 ===
def test_tampered_signature(mqtt_client):
    global response_payload
    response_payload = None
    
    resp_topic = "test/resp/tampered_sig"
    mqtt_client.subscribe(resp_topic)
    
    req_data = {"requestId": "req-003", "version": "1.2.3", "fileSize": 1048576}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    # 篡改签名（破坏最后几个字符）
    tampered_sig = signature[:-2] + ("AA" if not signature.endswith("AA") else "BB")
    
    publish_request(mqtt_client, "update/start", req_data, resp_topic, tampered_sig)
    time.sleep(2)
    
    assert response_payload is None, "签名被篡改，服务端不应发送响应！"
    print("✅ Task 2.3 篡改签名成功拦截")

# === Task 2.4: 错误 Payload 结构验证 ===
def test_invalid_payload_structure(mqtt_client):
    global response_payload
    response_payload = None
    
    resp_topic = "test/resp/bad_payload"
    mqtt_client.subscribe(resp_topic)
    
    # 错误的 JSON 结构 (缺少必要字段，但签名有效)
    req_data = {"invalid_key": "wrong_data"}
    payload_str = json.dumps(req_data, separators=(',', ':'))
    signature = generate_signature(payload_str)
    
    publish_request(mqtt_client, "update/start", req_data, resp_topic, signature)
    time.sleep(2)
    
    assert response_payload is None, "结构错误的 Payload，服务端不应发送响应！"
    print("✅ Task 2.4 错误结构成功拦截")

if __name__ == "__main__":
    pytest.main(["-v", "-s", __file__])