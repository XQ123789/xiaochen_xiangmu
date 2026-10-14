#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@file    send.py
@brief   GPS坐标发送脚本 - 改进版
@note    改进自原项目 发送和接收GPS坐标python代码/send.py.txt

主要改进：
  1. 修正文件扩展名为 .py（原为 .py.txt）
  2. 从 config.json 读取服务器地址和串口配置
  3. 添加异常处理和自动重试机制
  4. 添加日志记录（同时输出到控制台和文件）
  5. 支持命令行参数覆盖配置文件
"""

import serial
import time
import json
import logging
import argparse
import sys
import os

# ============ 日志配置 ============
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout),              # 输出到控制台
        logging.FileHandler('send.log', encoding='utf-8')  # 同时写入日志文件
    ]
)
logger = logging.getLogger(__name__)


def load_config(config_path='config.json'):
    """
    从JSON配置文件加载配置
    :param config_path: 配置文件路径
    :return: 配置字典
    """
    default_config = {
        'server': {
            'host': 'mqtts.heclouds.com',
            'port': 1883,
            'product_key': 'your_product_key',
            'device_name': 'your_device_name',
            'device_secret': 'your_device_secret'
        },
        'serial': {
            'port': '/dev/ttyUSB0',
            'baudrate': 115200
        },
        'gps': {
            'update_interval': 3,
            'coordinate_system': 'gcj02'
        }
    }

    try:
        # 尝试从脚本所在目录加载配置文件
        script_dir = os.path.dirname(os.path.abspath(__file__))
        full_path = os.path.join(script_dir, config_path)

        with open(full_path, 'r', encoding='utf-8') as f:
            config = json.load(f)
        logger.info(f'配置文件加载成功: {full_path}')
        return config
    except FileNotFoundError:
        logger.warning(f'配置文件 {config_path} 未找到，使用默认配置')
        return default_config
    except json.JSONDecodeError as e:
        logger.error(f'配置文件格式错误: {e}，使用默认配置')
        return default_config


def send_to_stm32(port, baudrate, interval):
    """
    向STM32发送GPS坐标数据
    :param port: 串口端口号
    :param baudrate: 波特率
    :param interval: 发送间隔（秒）
    """
    ser = None
    retry_count = 0
    max_retry = 5  # 最大连续错误重试次数

    try:
        # 打开串口
        ser = serial.Serial(port, baudrate=baudrate, timeout=1)
        logger.info(f'串口 {port} 已打开，波特率 {baudrate}')
        retry_count = 0  # 打开成功，重置重试计数

        while True:
            try:
                # 示例GPS数据：经度116.404，纬度39.915
                # 实际使用时应从GPS模块或文件读取真实数据
                message = "NAV,116.404,39.915"

                ser.write((message + "\n").encode())
                logger.info(f'已发送: {message}')

                # 等待指定间隔
                time.sleep(interval)
                retry_count = 0  # 发送成功，重置重试计数

            except serial.SerialException as e:
                retry_count += 1
                logger.error(f'串口写入错误 (第{retry_count}次): {e}')

                if retry_count >= max_retry:
                    logger.error(f'连续错误达到{max_retry}次，退出')
                    break

                # 等待后重试
                wait_time = min(retry_count * 2, 10)  # 指数退避，最多等10秒
                logger.info(f'等待 {wait_time} 秒后重试...')
                time.sleep(wait_time)

            except OSError as e:
                retry_count += 1
                logger.error(f'系统错误 (第{retry_count}次): {e}')
                if retry_count >= max_retry:
                    break
                time.sleep(5)

    except serial.SerialException as e:
        logger.error(f'无法打开串口 {port}: {e}')
        logger.info('请检查：')
        logger.info('  1. 设备是否已连接')
        logger.info('  2. 端口号是否正确（Linux: /dev/ttyUSB0, Windows: COM3）')
        logger.info('  3. 是否有其他程序占用该串口')

    except KeyboardInterrupt:
        logger.info('用户终止 (Ctrl+C)')

    finally:
        if ser is not None and ser.is_open:
            ser.close()
            logger.info('串口已关闭')


def main():
    """主函数：解析命令行参数并启动发送"""
    parser = argparse.ArgumentParser(
        description='GPS坐标发送脚本 - 向STM32发送导航坐标数据'
    )
    parser.add_argument('-p', '--port', default=None,
                        help='串口端口号（默认从config.json读取）')
    parser.add_argument('-b', '--baudrate', type=int, default=None,
                        help='波特率（默认从config.json读取）')
    parser.add_argument('-i', '--interval', type=float, default=None,
                        help='发送间隔秒数（默认从config.json读取）')
    parser.add_argument('-c', '--config', default='config.json',
                        help='配置文件路径（默认: config.json）')

    args = parser.parse_args()

    # 加载配置
    config = load_config(args.config)

    # 命令行参数优先，其次配置文件
    port = args.port or config['serial']['port']
    baudrate = args.baudrate or config['serial']['baudrate']
    interval = args.interval or config['gps']['update_interval']

    logger.info('=== GPS坐标发送脚本启动 ===')
    logger.info(f'串口: {port}, 波特率: {baudrate}, 间隔: {interval}秒')

    send_to_stm32(port, baudrate, interval)


if __name__ == "__main__":
    main()
