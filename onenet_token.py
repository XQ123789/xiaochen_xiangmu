# -*- coding: utf-8 -*-
"""
OneNET 设备安全 token 生成脚本（version=2018-10-31，MQTT 接入用）

用法：
  1. 在 OneNET 控制台拿到：产品ID、设备名称、AccessKey（产品级 master-key
     或设备级 key，设备详情页的 key）
  2. 修改下面 PRODUCT_ID / DEVICE_NAME / ACCESS_KEY / 有效期
  3. 运行：python onenet_token.py
  4. 把输出的整串 token 粘贴到 STM32 工程 HARDWARE\4G\4G.c 第 15 行
     的 DEVICESECRET 中（替换整行字符串内容）

连接参数对照（用于 MQTTX 等工具手动验证）：
  地址   mqtts.heclouds.com:1883
  ClientID = 设备名称
  Username = 产品ID
  Password = 本脚本生成的 token
"""

import base64
import hmac
import time
from urllib.parse import quote

# ============ 按实际情况修改这四项 ============
PRODUCT_ID  = "你的产品ID"        # 形如 your_product_id
DEVICE_NAME = "你的设备名称"      # 形如 your_device_name
ACCESS_KEY  = "你的AccessKey"     # 产品/设备详情页的 key（base64 字符串）
VALID_DAYS  = 365 * 30            # token 有效期（天），默认约 30 年
# ============================================


def gen_token(product_id: str, device_name: str, access_key: str,
              valid_days: int = VALID_DAYS) -> str:
    version = "2018-10-31"
    res = "products/%s/devices/%s" % (product_id, device_name)
    et = str(int(time.time()) + valid_days * 86400)
    method = "md5"

    key = base64.b64decode(access_key)
    string_for_sign = "%s\n%s\n%s\n%s" % (et, method, res, version)
    sign_b = hmac.new(key=key, msg=string_for_sign.encode("utf-8"),
                      digestmod="md5").digest()
    sign = quote(base64.b64encode(sign_b).decode(), safe="")

    return "version=%s&res=%s&et=%s&method=%s&sign=%s" % (
        version, quote(res, safe=""), et, method, sign)


if __name__ == "__main__":
    if "你的" in PRODUCT_ID + DEVICE_NAME + ACCESS_KEY:
        print("请先修改脚本顶部的 PRODUCT_ID / DEVICE_NAME / ACCESS_KEY 再运行。")
        raise SystemExit(1)

    token = gen_token(PRODUCT_ID, DEVICE_NAME, ACCESS_KEY)
    print("OneNET token 生成成功，粘贴到 4G.c 第 15 行 DEVICESECRET：\n")
    print('#define DEVICESECRET   "%s"' % token)
