// 步骤1：从 index.html 抽取内联脚本，生成可 require 的模块文件（仅字符串处理，无执行）
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8');
const m = html.match(/<script>\n([\s\S]*?)<\/script>/);
if (!m) { console.error('未找到内联脚本'); process.exit(1); }
const src = m[1];

// 去掉末尾 init() 自动启动，追加模块导出
const modSrc = src.replace(/init\(\);\s*$/, '') +
  '\nmodule.exports = { genOnenetToken, parsePayload, parseSerialFrame, classifyMessage, pyQuote, md5Bytes, hmacMd5, b64FromBytes, bytesFromB64, parseTs, haversineM, bearingDeg };\n';
fs.writeFileSync(path.join(__dirname, '_app_module.js'), modSrc);
console.log('已生成 _app_module.js，共', modSrc.length, '字符');
