#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@file    receive.py
@brief   GPS坐标接收脚本 - 改进版
@note    改进自原项目 发送和接收GPS坐标python代码/receive.py.txt

主要改进：
  1. 修正文件扩展名为 .py（原为 .py.txt）
  2. 添加缺失的 import serial（原代码缺少此导入，运行会报错）
  3. 添加完善的异常处理（串口错误、解码错误等）
  4. 添加日志记录（同时输出到控制台和文件）
  5. 支持命令行参数和配置文件
  6. 添加串口重连机制
"""

import serial
import time
import json
import logging
import argparse
import sys
import os
from collections import deque

# ============ 日志配置 ============
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler('receive.log', encoding='utf-8')
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
        'serial': {
            'port': '/dev/ttyUSB0',
            'baudrate': 115200
        }
    }

    try:
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


def turbo_receive(port, baudrate):
    """
    高速接收GPS数据并解析坐标
    :param port: 串口端口号
    :param baudrate: 波特率
    """
    ser = None
    retry_count = 0
    max_retry = 5

    while retry_count < max_retry:
        try:
            # 打开串口
            ser = serial.Serial(port, baudrate=baudrate, timeout=0)
            logger.info(f'串口 {port} 已打开，波特率 {baudrate}')
            retry_count = 0  # 成功打开，重置计数

            buffer = bytearray()  # 二进制接收缓冲区
            frame_count = 0       # 已接收帧数

            while True:
                try:
                    # 大块读取二进制数据（高效接收）
                    raw = ser.read(ser.in_waiting or 1)
                    if raw:
                        buffer += raw

                        # 处理所有完整帧（按\r\n分割）
                        while b'\r\n' in buffer:
                            line, _, buffer = buffer.partition(b'\r\n')
                            try:
                                decoded = line.decode('ascii', errors='ignore').strip()

                                # 解析GPS帧格式：GPS,纬度,经度*校验和
                                if decoded.startswith('GPS') and '*' in decoded:
                                    # 提取坐标部分（忽略校验和）
                                    coords = decoded.split('*')[0]
                                    parts = coords.split(',')

                                    if len(parts) >= 3:
                                        lat = parts[1]
                                        lon = parts[2]
                                        frame_count += 1
                                        logger.info(
                                            f'帧#{frame_count} | '
                                            f'纬度: {lat}, 经度: {lon}'
                                        )
                                    else:
                                        logger.warning(f'GPS帧格式异常: {decoded}')

                            except UnicodeError:
                                # 忽略解码错误，继续处理下一帧
                                continue

                except serial.SerialException as e:
                    logger.error(f'串口读取错误: {e}')
                    break  # 跳出内层循环，尝试重连

                except OSError as e:
                    logger.error(f'系统错误: {e}')
                    break

        except serial.SerialException as e:
            retry_count += 1
            logger.error(
                f'无法打开串口 {port} (第{retry_count}次): {e}'
            )
            if retry_count < max_retry:
                wait_time = min(retry_count * 2, 10)
                logger.info(f'等待 {wait_time} 秒后重试...')
                time.sleep(wait_time)
            else:
                logger.error(f'串口打开失败，已达最大重试次数({max_retry})')
                return

        except KeyboardInterrupt:
            logger.info('用户终止 (Ctrl+C)')
            break

        finally:
            if ser is not None and ser.is_open:
                ser.close()
                logger.info('串口已关闭')

        # 如果是因为串口错误跳出，尝试重连
        logger.info('尝试重新连接串口...')
        time.sleep(2)

    logger.error('接收脚本退出')


def main():
    """主函数：解析参数并启动接收"""
    parser = argparse.ArgumentParser(
        description='GPS坐标接收脚本 - 从串口接收GPS数据并解析坐标'
    )
    parser.add_argument('-p', '--port', default=None,
                        help='串口端口号（默认从config.json读取）')
    parser.add_argument('-b', '--baudrate', type=int, default=None,
                        help='波特率（默认从config.json读取）')
    parser.add_argument('-c', '--config', default='config.json',
                        help='配置文件路径（默认: config.json）')

    args = parser.parse_args()

    # 加载配置
    config = load_config(args.config)

    # 命令行参数优先
    port = args.port or config['serial']['port']
    baudrate = args.baudrate or config['serial']['baudrate']

    logger.info('=== GPS坐标接收脚本启动 ===')
    logger.info(f'串口: {port}, 波特率: {baudrate}')

    turbo_receive(port, baudrate)


if __name__ == "__main__":
    main()
