// 步骤2：加载抽取出的应用模块，跑核心逻辑单元测试（浏览器 API 打桩，静态 require）
'use strict';

// 打桩浏览器 API（必须在 require 应用模块前就绪）
function stubEl() {
  return { style: {}, textContent: '', value: '', checked: false, className: '',
    children: { length: 0 }, firstChild: null, rows: { length: 0, deleteRow() {} },
    insertBefore() {}, removeChild() {}, appendChild() {}, showModal() {}, close() {},
    onclick: null, onchange: null, bindPopup() { return this; }, querySelector() { return null; },
    classList: { add() {}, remove() {}, toggle() {}, contains() { return false; } } };
}
global.document = {
  getElementById: () => stubEl(),
  createElement: () => Object.assign(stubEl(), { setAttribute() {}, click() {}, href: '', download: '' }),
};
global.window = { addEventListener() {} };
global.localStorage = { getItem: () => null, setItem() {} };
global.btoa = s => Buffer.from(s, 'binary').toString('base64');
global.atob = s => Buffer.from(s, 'base64').toString('binary');
global.alert = () => {};

const { genOnenetToken, parsePayload, parseSerialFrame, classifyMessage, pyQuote, md5Bytes, hmacMd5, b64FromBytes, bytesFromB64, parseTs, haversineM, bearingDeg } = require('./_app_module.js');

let pass = 0, fail = 0;
function t(name, cond) { if (cond) { pass++; } else { fail++; console.error('FAIL:', name); } }

// 1. token 公式与 python 参考实现等价（固定 et 手工构造，与 onenet_token.py 输出比对）
const keyB64 = Buffer.from('myTestAccessKey123').toString('base64');
const version = '2018-10-31', method = 'md5', res = 'products/your_product_id/devices/your_device_name', et = '3778667000';
const msg = `${et}\n${method}\n${res}\n${version}`;
const sign = pyQuote(b64FromBytes(hmacMd5(bytesFromB64(keyB64), new TextEncoder().encode(msg))));
t('token与python一致',
  `version=${version}&res=${pyQuote(res)}&et=${et}&method=${method}&sign=${sign}` ===
  'version=2018-10-31&res=products%2Fyour_product_id%2Fdevices%2Fyour_device_name&et=0000000000&method=md5&sign=YOUR_SIGN_HERE&et=3778667000&method=md5&sign=SsB%2BhL2DWSX1tdrDQDlUUw%3D%3D');
const tok = genOnenetToken('p1', 'd1', keyB64, 30);
t('genOnenetToken格式', /^version=2018-10-31&res=products%2Fyour_product_id%2Fdevices%2Fyour_device_name&et=0000000000&method=md5&sign=YOUR_SIGN_HERE&et=\d+&method=md5&sign=.+$/.test(tok));

// 2. md5/hmac 标准向量
const h = b => Buffer.from(b).toString('hex');
t('md5(abc)', h(md5Bytes(new TextEncoder().encode('abc'))) === '900150983cd24fb0d6963f7d28e17f72');
t('md5(empty)', h(md5Bytes(new TextEncoder().encode(''))) === 'd41d8cd98f00b204e9800998ecf8427e');
t('hmac-md5(Jefe)', h(hmacMd5(new TextEncoder().encode('Jefe'), new TextEncoder().encode('what do ya want for nothing?'))) === '750c783e6ab0b503eaa86e310a5db738');
t('hmac-md5(80字节key)', h(hmacMd5(new Uint8Array(80).fill(0xaa), new TextEncoder().encode('Test Using Larger Than Block-Size Key - Hash Key First'))) === '6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd');

// 3. OneNET 物模型解析（与固件 MakeOnenetPayload 输出一致）
const r1 = parsePayload('{"id":"123","params":{"temp":{"value":25},"humi":{"value":60},"latitude":{"value":30.123456},"longitude":{"value":120.654321},"altitude":{"value":10.5},"speed":{"value":1.25},"direction":{"value":90.0}}}');
t('物模型 temp/humi', r1.temp === 25 && r1.humi === 60);
t('物模型 lat/lon', r1.lat === 30.123456 && r1.lon === 120.654321);
t('物模型 speed/dir/alt', r1.speed === 1.25 && r1.dir === 90 && r1.alt === 10.5);
t('物模型 ts 缺省为当前毫秒', r1.ts >= Date.now() - 5000);

// 4. 普通 JSON 解析（别名 + 秒级时间戳）
const r2 = parsePayload('{"temperature":26.5,"humidity":58,"lat":31.1,"lng":121.5,"speed":2.5,"heading":180,"ts":1760000000}');
t('别名解析', r2.temp === 26.5 && r2.humi === 58 && r2.lat === 31.1 && r2.lon === 121.5 && r2.dir === 180);
t('秒级ts转毫秒', r2.ts === 1760000000000);

// 5. 边界
t('毫秒ts直通', parseTs(1760000000000) === 1760000000000);
t('数字字符串ts', parseTs('1760000000') === 1760000000000);
t('非JSON返回null', parsePayload('AT+CGNSINF 1,1,...') === null);
t('空对象返回null', parsePayload('{}') === null);
t('null安全', parsePayload('null') === null);
t('reply应答不产生记录', parsePayload('{"id":"123","code":200,"msg":"succ"}') === null);

// 6. pyQuote 对照 python quote(safe="")
t('pyQuote斜杠', pyQuote('products/1/d') === 'products%2F1%2Fd');
t('pyQuote保留字符', pyQuote('aZ09-._~') === 'aZ09-._~');
t('pyQuote中文', pyQuote('设') === '%E8%AE%BE');

// 7. 串口帧解析（树莓派 ↔ STM32 链路；注意 GPS 帧纬度在前、NAV 帧经度在前）
const g1 = parseSerialFrame('GPS,30.123456,120.654321*7A');
t('GPS帧：纬度在前', g1 && g1.kind === 'record' && g1.rec.lat === 30.123456 && g1.rec.lon === 120.654321);
t('GPS帧无校验和也认', parseSerialFrame('GPS,30.1,120.2').kind === 'record');
const n1 = parseSerialFrame('NAV,116.404,39.915');
t('NAV帧：经度在前', n1 && n1.kind === 'nav' && n1.lng === 116.404 && n1.lat === 39.915);
t('AT指令不是帧', parseSerialFrame('AT+CGNSINF 1,1') === null);
t('GPS帧负数坐标', parseSerialFrame('GPS,-33.87,151.21*00').rec.lat === -33.87);

// 8. classifyMessage 分类（JSON 数据 / OneNET 应答 / 串口帧 / 未知）
const c1 = classifyMessage('{"temp":25,"lat":30,"lon":120}');
t('分类：普通JSON→record', c1.kind === 'record' && c1.rec.temp === 25);
const c2 = classifyMessage('{"id":"123","code":200,"msg":"succ"}');
t('分类：post/reply应答', c2.kind === 'reply' && c2.reply.code === 200);
const c3 = classifyMessage('GPS,39.9,116.4*1F');
t('分类：GPS帧', c3.kind === 'record' && c3.rec.lat === 39.9);
const c4 = classifyMessage('NAV,116.4,39.9');
t('分类：NAV帧', c4.kind === 'nav' && c4.lat === 39.9);
const c5 = classifyMessage('hello world');
t('分类：未知', c5.kind === 'unknown');
const c6 = classifyMessage('{"params":{"temp":{"value":25}}}');
t('分类：物模型', c6.kind === 'record' && c6.rec.temp === 25);

// 9. 距离与方位角（已知值对照）
const dist = haversineM(39.9042, 116.4074, 39.9042, 116.4174);   // 同纬度约 851 m
t('haversine同纬度~850m', Math.abs(dist - 851) < 15);
t('haversine同点为0', haversineM(30, 120, 30, 120) === 0);
t('方位角正东=90', Math.abs(bearingDeg(39.9, 116.4, 39.9, 116.5) - 90) < 0.5);
t('方位角正北=0', Math.abs(bearingDeg(39.9, 116.4, 40.0, 116.4) - 0) < 0.5);
t('方位角正南=180', Math.abs(bearingDeg(40.0, 116.4, 39.9, 116.4) - 180) < 0.5);

console.log(`\n${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);
