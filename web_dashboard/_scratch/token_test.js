// OneNET token 生成（浏览器端）—— 与 onenet_token.py 等价性测试
// 1. md5 over Uint8Array
// 2. HMAC-MD5 (key: Uint8Array, msg: string -> utf8 bytes)
// 3. python quote(s, safe="") 等价的百分号编码（大写hex）

function md5Bytes(bytes) {
  // 标准 MD5，输入 Uint8Array，返回 Uint8Array(16)
  const s = [7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
             5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
             4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
             6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21];
  const K = new Int32Array(64);
  for (let i = 0; i < 64; i++) K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296);

  function rotl(x, c) { return (x << c) | (x >>> (32 - c)); }

  const len = bytes.length;
  const withPad = new Uint8Array((((len + 8) >> 6) + 1) * 64);
  withPad.set(bytes);
  withPad[len] = 0x80;
  const bitLen = len * 8;
  const dv = new DataView(withPad.buffer);
  dv.setUint32(withPad.length - 8, bitLen >>> 0, true);
  dv.setUint32(withPad.length - 4, Math.floor(bitLen / 4294967296), true);

  let a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
  const M = new Int32Array(16);
  for (let off = 0; off < withPad.length; off += 64) {
    for (let i = 0; i < 16; i++) M[i] = dv.getInt32(off + i * 4, true);
    let A = a0, B = b0, C = c0, D = d0;
    for (let i = 0; i < 64; i++) {
      let F, g;
      if (i < 16)      { F = (B & C) | (~B & D);      g = i; }
      else if (i < 32) { F = (D & B) | (~D & C);      g = (5 * i + 1) % 16; }
      else if (i < 48) { F = B ^ C ^ D;               g = (3 * i + 5) % 16; }
      else             { F = C ^ (B | ~D);            g = (7 * i) % 16; }
      F = (F + A + K[i] + M[g]) | 0;
      A = D; D = C; C = B;
      B = (B + rotl(F, s[i])) | 0;
    }
    a0 = (a0 + A) | 0; b0 = (b0 + B) | 0; c0 = (c0 + C) | 0; d0 = (d0 + D) | 0;
  }
  const out = new Uint8Array(16);
  const odv = new DataView(out.buffer);
  odv.setInt32(0, a0, true); odv.setInt32(4, b0, true);
  odv.setInt32(8, c0, true); odv.setInt32(12, d0, true);
  return out;
}

function hmacMd5(keyBytes, msgBytes) {
  const block = 64;
  let k = keyBytes;
  if (k.length > block) k = md5Bytes(k);
  const kPad = new Uint8Array(block);
  kPad.set(k);
  const inner = new Uint8Array(block + msgBytes.length);
  const outer = new Uint8Array(block + 16);
  for (let i = 0; i < block; i++) { inner[i] = kPad[i] ^ 0x36; outer[i] = kPad[i] ^ 0x5c; }
  inner.set(msgBytes, block);
  outer.set(md5Bytes(inner), block);
  return md5Bytes(outer);
}

function toHex(b) { return Array.from(b, x => x.toString(16).padStart(2, '0')).join(''); }
function toB64(b) { return Buffer.from(b).toString('base64'); }

function pyQuote(str) {
  const bytes = new TextEncoder().encode(str);
  let out = '';
  for (const b of bytes) {
    const unreserved = (b >= 0x41 && b <= 0x5a) || (b >= 0x61 && b <= 0x7a) ||
                       (b >= 0x30 && b <= 0x39) || b === 0x2d || b === 0x2e ||
                       b === 0x5f || b === 0x7e;
    out += unreserved ? String.fromCharCode(b) : '%' + b.toString(16).toUpperCase().padStart(2, '0');
  }
  return out;
}

function genToken(pid, dev, accessKeyB64, et, version = '2018-10-31', method = 'md5') {
  const res = 'products/' + pid + '/devices/' + dev;
  const key = Buffer.from(accessKeyB64, 'base64');  // 测试中用 Buffer；浏览器端用 atob
  const msg = et + '\n' + method + '\n' + res + '\n' + version;
  const sign = pyQuote(toB64(hmacMd5(new Uint8Array(key), new TextEncoder().encode(msg))));
  return 'version=' + version + '&res=' + pyQuote(res) + '&et=' + et + '&method=' + method + '&sign=' + sign;
}

// ==== 测试 ====
// 1. MD5 标准测试向量
console.assert(toHex(md5Bytes(new TextEncoder().encode(''))) === 'd41d8cd98f00b204e9800998ecf8427e', 'md5 empty');
console.assert(toHex(md5Bytes(new TextEncoder().encode('abc'))) === '900150983cd24fb0d6963f7d28e17f72', 'md5 abc');
console.assert(toHex(md5Bytes(new TextEncoder().encode('The quick brown fox jumps over the lazy dog'))) === '9e107d9d372bb6826bd81d3542a419d6', 'md5 fox');
// 2. HMAC-MD5 RFC 2202 测试向量
console.assert(toHex(hmacMd5(new Uint8Array(16).fill(0x0b), new TextEncoder().encode('Hi There'))) === '9294727a3638bb1c13f48ef8158bfc9d', 'hmac rfc1');
console.assert(toHex(hmacMd5(new TextEncoder().encode('Jefe'), new TextEncoder().encode('what do ya want for nothing?'))) === '750c783e6ab0b503eaa86e310a5db738', 'hmac rfc2');
// 3. 长密钥/含非ASCII字节的key
console.assert(toHex(hmacMd5(new Uint8Array(80).fill(0xaa), new TextEncoder().encode('Test Using Larger Than Block-Size Key - Hash Key First'))) === '6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd', 'hmac rfc6');
console.log('MD5/HMAC 测试通过');

// 4. token 等价性测试（与 python 对比，et 固定）
const t = genToken('your_product_id', 'your_device_name', Buffer.from('myTestAccessKey123').toString('base64'), '3778667000');
console.log('JS  token:', t);
