/* ============================================================
   ESP8285 开机卡 · 在线演示
   纯前端模拟，不连接任何真实设备
   ============================================================ */

// ---------- 模拟设备状态 ----------
const state = {
  power: false,          // 电脑是否开机
  wifi: 'MyHomeWiFi',
  rssi: -45,
  mqtt: true,
  uptime: 3600,
  heap: 32000,
  webPass: 'demo123',
  loggedIn: false,
  mode: 0,               // 0=巴法云 1=HA
  press: -1,             // -1 空闲，12 电源键，13 重启键
  logs: [],
  scanning: false,
  scanResults: null,
  paused: false
};

// ---------- 日志 ----------
function addLog(msg) {
  const now = new Date();
  const t = String(now.getHours()).padStart(2, '0') + ':' +
            String(now.getMinutes()).padStart(2, '0') + ':' +
            String(now.getSeconds()).padStart(2, '0');
  state.logs.push(t + ' ' + msg);
  if (state.logs.length > 200) state.logs.shift();
  renderLog();
}

function renderLog() {
  const el = document.getElementById('log');
  if (!el) return;
  if (state.paused) return;
  // 记下"离底部还有多远"，而不是离顶部多远。
  // 日志一直在增长，按顶部位置还原的话，用户正在看的那几行会往下漂走。
  const fromBottom = el.scrollHeight - el.scrollTop - el.clientHeight;
  const atBottom = fromBottom < 40;
  el.textContent = state.logs.length ? state.logs.join('\n') : '(暂无日志)';
  // 换掉文本内容会把滚动条打回顶部，正在往上翻的人会被弹走，这里还原回去
  el.scrollTop = atBottom ? el.scrollHeight : (el.scrollHeight - el.clientHeight - fromBottom);
}

function togglePause() {
  state.paused = !state.paused;
  document.getElementById('pauseBtn').textContent = state.paused ? '继续滚动' : '暂停滚动';
  if (!state.paused) renderLog();
}

function clearLog() {
  state.logs = [];
  const el = document.getElementById('log');
  if (el) el.textContent = '(暂无日志)';
  // 清完屏如果还继续往里面追加，日志立刻又长回来，等于白清。
  // 所以顺手暂停，想接着看就点"继续滚动"。
  state.paused = true;
  const btn = document.getElementById('pauseBtn');
  if (btn) btn.textContent = '继续滚动';
}

// ---------- 页面切换 ----------
function switchView(name) {
  document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
  document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
  const view = document.getElementById('view-' + name);
  if (view) view.classList.add('active');
  const btn = document.querySelector('.nav-btn[data-view="' + name + '"]');
  if (btn) btn.classList.add('active');

  if (name === 'status') {
    refreshStatus();
    renderLog();
  }
}

document.querySelectorAll('.nav-btn').forEach(btn => {
  btn.addEventListener('click', () => switchView(btn.dataset.view));
});

// ---------- 配网页面 ----------
const MOCK_WIFI = [
  { ssid: 'MyHomeWiFi', rssi: -45, enc: true },
  { ssid: 'TP-LINK_5G', rssi: -62, enc: true },
  { ssid: 'ChinaNet-8f2a', rssi: -71, enc: true },
  { ssid: 'Xiaomi_AX3000', rssi: -78, enc: true },
  { ssid: 'FreeWiFi', rssi: -85, enc: false }
];

function scanWifi() {
  const sel = document.getElementById('ssid');
  sel.innerHTML = '<option value="">扫描中...</option>';
  setTimeout(() => {
    sel.innerHTML = '<option value="">请选择网络</option>';
    MOCK_WIFI.forEach(w => {
      const opt = document.createElement('option');
      opt.value = w.ssid;
      opt.textContent = w.ssid + (w.enc ? ' 🔒' : '') + '  ' + w.rssi + 'dBm';
      sel.appendChild(opt);
    });
  }, 800);
}

let setupMode = 0;
function setMode(m) {
  setupMode = m;
  document.getElementById('segBemfa').classList.toggle('active', m === 0);
  document.getElementById('segHa').classList.toggle('active', m === 1);
  document.getElementById('bemfaFields').style.display = m === 0 ? 'block' : 'none';
  document.getElementById('haFields').style.display = m === 1 ? 'block' : 'none';
}

function saveSetup() {
  const ssid = document.getElementById('ssid').value;
  const uid = document.getElementById('uid').value.trim();
  const host = document.getElementById('mqttHost').value.trim();
  const st = document.getElementById('setupStatus');

  if (!ssid) { st.className = 'status err'; st.textContent = '请选择 WiFi'; return; }
  if (setupMode === 0 && uid.length !== 32) {
    st.className = 'status err'; st.textContent = '私钥应为 32 位'; return;
  }
  if (setupMode === 1 && !host) {
    st.className = 'status err'; st.textContent = '请填写 MQTT 服务器地址'; return;
  }

  const btn = document.getElementById('saveBtn');
  btn.disabled = true;
  btn.textContent = '正在保存...';
  setTimeout(() => {
    st.className = 'status ok';
    st.textContent = '保存成功，设备正在重启...';
    btn.textContent = '已保存';
    addLog('[CFG] 配置已保存');
    addLog('[WiFi] 连接 ' + ssid);
    addLog('[WiFi] 已连接 192.168.31.100');
    setTimeout(() => {
      switchView('status');
      btn.disabled = false;
      btn.textContent = '保存并重启';
      st.textContent = '';
    }, 1500);
  }, 1000);
}

// ---------- 登录页面 ----------
function doLogin() {
  const pw = document.getElementById('loginPass').value;
  const msg = document.getElementById('loginMsg');
  const btn = document.getElementById('loginBtn');

  if (!pw) { msg.textContent = '请输入密码'; return; }

  btn.disabled = true;
  btn.textContent = '验证中...';

  setTimeout(() => {
    if (pw === state.webPass) {
      state.loggedIn = true;
      msg.style.color = 'var(--ok)';
      msg.textContent = '登录成功，正在进入...';
      addLog('[WEB] 登录成功');
      setTimeout(() => {
        switchView('status');
        btn.disabled = false;
        btn.textContent = '进入';
        msg.textContent = '';
        msg.style.color = '';
      }, 600);
    } else {
      msg.style.color = 'var(--bad)';
      msg.textContent = '密码错误';
      addLog('[WEB] 登录失败：密码错误');
      btn.disabled = false;
      btn.textContent = '进入';
      document.getElementById('loginPass').select();
    }
  }, 600);
}

// ---------- 状态页面 ----------
function fmtUptime(s) {
  const d = Math.floor(s / 86400);
  const h = Math.floor(s % 86400 / 3600);
  const m = Math.floor(s % 3600 / 60);
  const x = s % 60;
  if (d) return d + '天' + h + '小时';
  if (h) return h + '小时' + m + '分';
  if (m) return m + '分' + x + '秒';
  return x + '秒';
}

function refreshStatus() {
  const set = (id, html) => {
    const el = document.getElementById(id);
    if (el) el.innerHTML = html;
  };
  set('stWifi', state.wifi ? '<span class="ok">已连接</span>' : '<span class="bad">未连接</span>');
  set('stRssi', state.rssi + ' dBm');
  set('stIp', '192.168.31.100');
  set('stMqtt', state.mqtt ? '<span class="ok">已连接</span>' : '<span class="bad">未连接</span>');
  set('stPower', state.power ? '<span class="ok">开机</span>' : '关机');
  set('stUptime', fmtUptime(state.uptime));
  set('stHeap', (state.heap / 1024).toFixed(1) + ' KB');
  set('stSsid', state.wifi || '-');
  set('curMode', state.mode === 0 ? '巴法云' : 'Home Assistant');
}

let pickedSsid = '';

function doScan() {
  const btn = document.getElementById('scanBtn');
  const msg = document.getElementById('wifiMsg');
  const list = document.getElementById('scanList');

  btn.disabled = true;
  btn.textContent = '扫描中...';
  msg.textContent = '扫描时 WiFi 会短暂中断，请稍候';
  list.style.display = 'none';
  document.getElementById('wifiForm').style.display = 'none';
  addLog('[WIFI] 开始扫描');

  setTimeout(() => {
    btn.disabled = false;
    btn.textContent = '重新扫描';
    renderScan(MOCK_WIFI);
    msg.textContent = '找到 ' + MOCK_WIFI.length + ' 个网络，点一个切换';
    addLog('[WIFI] 扫描完成，' + MOCK_WIFI.length + ' 个网络');
  }, 1500);
}

function renderScan(list) {
  const box = document.getElementById('scanItems');
  box.innerHTML = '';
  list.forEach(w => {
    const d = document.createElement('div');
    d.className = 'wifi-item';
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = w.ssid + (w.enc ? ' 🔒' : '');
    const meta = document.createElement('span');
    meta.className = 'meta';
    meta.textContent = w.rssi + ' dBm';
    d.appendChild(name);
    d.appendChild(meta);
    d.onclick = () => {
      document.querySelectorAll('.wifi-item').forEach(el => el.classList.remove('active'));
      d.classList.add('active');
      pickWifi(w.ssid, w.enc, w.rssi);
    };
    box.appendChild(d);
  });
  document.getElementById('scanList').style.display = 'block';
}

function pickWifi(ssid, enc, rssi) {
  pickedSsid = ssid;
  document.getElementById('pickSsid').textContent = ssid;
  document.getElementById('pickRssi').textContent = rssi + ' dBm';
  const p = document.getElementById('newWifiPass');
  p.value = '';
  p.placeholder = enc ? '输入 WiFi 密码' : '开放网络，无需密码';
  document.getElementById('wifiForm').style.display = 'block';
  document.getElementById('wifiMsg').textContent = '';
}

function cancelWifi() {
  pickedSsid = '';
  document.getElementById('wifiForm').style.display = 'none';
  document.getElementById('wifiMsg').textContent = '';
  document.querySelectorAll('.wifi-item').forEach(el => el.classList.remove('active'));
}

function doSaveWifi() {
  const msg = document.getElementById('wifiMsg');
  if (!pickedSsid) { msg.textContent = '请先扫描并选择一个网络'; return; }
  msg.textContent = '正在保存...';
  addLog('[WIFI] 切换到 ' + pickedSsid + '，准备重启');
  setTimeout(() => {
    state.wifi = pickedSsid;
    msg.textContent = '已保存，设备正在重启并连接新网络';
    refreshStatus();
  }, 1000);
}

function doChangePass() {
  const oldPass = document.getElementById('oldPass').value;
  const newPass = document.getElementById('newWebPass').value;
  const msg = document.getElementById('passMsg');

  if (!newPass && !oldPass) {
    msg.textContent = '请填写新密码；新密码留空表示取消密码';
    return;
  }
  if (oldPass !== state.webPass) {
    msg.textContent = '当前密码错误';
    addLog('[WEB] 改密码失败：当前密码不对');
    return;
  }

  msg.textContent = '正在保存...';
  setTimeout(() => {
    state.webPass = newPass;
    msg.textContent = newPass ? '已保存，正在重新登录...' : '已保存，密码已取消';
    addLog('[WEB] 网页密码已更新，长度=' + newPass.length);
    setTimeout(() => {
      state.loggedIn = false;
      switchView('login');
      msg.textContent = '';
    }, 1200);
  }, 800);
}

function doUpload() {
  const f = document.getElementById('fw').files[0];
  const msg = document.getElementById('msg');
  if (!f) { msg.textContent = '请先选择 .bin 固件文件'; return; }

  const bar = document.getElementById('bar');
  const inner = document.getElementById('barInner');
  bar.style.display = 'block';
  document.getElementById('upBtn').disabled = true;
  msg.textContent = '正在上传...';
  addLog('[OTA] 开始接收 ' + f.name);

  let p = 0;
  const timer = setInterval(() => {
    p += Math.random() * 15;
    if (p >= 100) {
      p = 100;
      clearInterval(timer);
      inner.style.width = '100%';
      msg.textContent = '升级成功，设备正在重启，约 10 秒后刷新页面';
      addLog('[OTA] 成功，准备重启');
      setTimeout(() => {
        bar.style.display = 'none';
        inner.style.width = '0';
        document.getElementById('upBtn').disabled = false;
      }, 3000);
    } else {
      inner.style.width = p + '%';
      msg.textContent = '正在上传 ' + Math.round(p) + '%';
    }
  }, 200);
}

function doRestart() {
  if (!confirm('确定重启设备？')) return;
  addLog('[SYS] 设备重启');
  document.getElementById('msg').textContent = '设备正在重启，稍等几秒刷新页面';
  setTimeout(() => {
    state.uptime = 0;
    refreshStatus();
  }, 2000);
}

// ---------- 切换连接方式 ----------
let targetMode = -1;

function toggleModeForm() {
  if (targetMode !== -1) { cancelMode(); return; }
  targetMode = state.mode === 0 ? 1 : 0;
  document.getElementById('modeTarget').textContent =
    targetMode === 0 ? '切换到巴法云' : '切换到 Home Assistant';
  document.getElementById('modeBemfaFields').style.display = targetMode === 0 ? 'block' : 'none';
  document.getElementById('modeHaFields').style.display = targetMode === 1 ? 'block' : 'none';
  document.getElementById('modeForm').style.display = 'block';
  document.getElementById('modeMsg').textContent = '';
  document.getElementById('modeBtn').textContent = '取消切换';
}

function cancelMode() {
  targetMode = -1;
  document.getElementById('modeForm').style.display = 'none';
  document.getElementById('modeMsg').textContent = '';
  document.getElementById('modeBtn').textContent = '切换模式';
}

function doSwitchMode() {
  const msg = document.getElementById('modeMsg');
  if (targetMode === -1) return;

  if (targetMode === 0) {
    const uid = document.getElementById('modeUid').value.trim();
    if (uid.length !== 32) { msg.textContent = '私钥应为 32 位'; return; }
  } else {
    const host = document.getElementById('modeHost').value.trim();
    if (!host) { msg.textContent = '请填写 MQTT 服务器地址'; return; }
  }

  const tip = targetMode === 0
    ? '确定切换到巴法云并重启？切换后设备会断开当前连接。'
    : '确定切换到 Home Assistant 并重启？切换后设备会断开当前连接。';
  if (!confirm(tip)) return;

  msg.textContent = '正在保存...';
  const newMode = targetMode;
  setTimeout(() => {
    state.mode = newMode;
    state.mqtt = true;
    msg.textContent = '已保存，设备正在重启并切换到新模式';
    addLog('[CFG] 切换到 ' + (newMode === 0 ? '巴法云' : 'HA') + ' 模式，准备重启');
    setTimeout(() => {
      cancelMode();
      refreshStatus();
    }, 1500);
  }, 800);
}

function doLogout() {
  state.loggedIn = false;
  addLog('[WEB] 已退出登录');
  switchView('login');
}

// ---------- 模拟设备面板 ----------
function toggleSim() {
  document.getElementById('simPanel').classList.toggle('collapsed');
}

function updateSim() {
  const dot = document.getElementById('simDot');
  const power = document.getElementById('simPower');
  const press = document.getElementById('simPress');
  if (dot) dot.classList.toggle('on', state.power);
  if (power) power.textContent = state.power ? '开机' : '关机';
  if (press) {
    press.textContent = state.press === -1 ? '空闲' :
                        state.press === 12 ? '电源键按下' : '重启键按下';
  }
}

function simAction(action) {
  if (state.press !== -1) {
    addLog('[KEY] 按键进行中，忽略本次指令');
    return;
  }

  if (action === 'on') {
    if (state.power) { addLog('>> 已经开机了，忽略'); return; }
    addLog('>> 开机');
    addLog('[KEY] 按下电源键 500ms');
    state.press = 12;
    updateSim();
    setTimeout(() => {
      state.press = -1;
      state.power = true;
      addLog('[KEY] 松开电源键');
      addLog('[STATE] 上报 on');
      updateSim();
      refreshStatus();
    }, 500);
  } else if (action === 'off') {
    if (!state.power) { addLog('>> 已经关机了，忽略'); return; }
    addLog('>> 关机');
    addLog('[KEY] 按下电源键 1000ms');
    state.press = 12;
    updateSim();
    setTimeout(() => {
      state.press = -1;
      state.power = false;
      addLog('[KEY] 松开电源键');
      addLog('[STATE] 上报 off');
      updateSim();
      refreshStatus();
    }, 1000);
  } else if (action === 'reset') {
    addLog('>> 重启');
    addLog('[KEY] 按下重启键 500ms');
    state.press = 13;
    updateSim();
    setTimeout(() => {
      state.press = -1;
      addLog('[KEY] 松开重启键');
      addLog('[STATE] 电脑正在重启');
      updateSim();
    }, 500);
  }
}

function simReset() {
  state.power = false;
  state.press = -1;
  state.uptime = 3600;
  state.wifi = 'MyHomeWiFi';
  state.rssi = -45;
  state.mqtt = true;
  state.heap = 32000;
  state.webPass = 'demo123';
  state.loggedIn = false;
  state.logs = [];
  state.paused = false;
  updateSim();
  refreshStatus();
  renderLog();
  switchView('setup');
  addLog('[SYS] 演示已重置');
}

// ---------- 定时模拟 ----------
setInterval(() => {
  state.uptime++;
  // 每 30 秒打一条心跳
  if (state.uptime % 30 === 0) {
    addLog('[HB] 运行' + state.uptime + 's mqtt=' + (state.mqtt ? 1 : 0) +
           ' press=' + state.press + ' 状态=' + (state.power ? 1 : 0));
  }
  // 内存小幅波动
  state.heap = 31500 + Math.floor(Math.random() * 800);
  const active = document.querySelector('.view.active');
  if (active && active.id === 'view-status') refreshStatus();
}, 1000);

// ---------- 初始化 ----------
(function init() {
  addLog('=== 电脑开机卡 v2 ===');
  addLog('[CFG] SSID=MyHomeWiFi UID=(空) mode=0');
  addLog('[WIFI] 已连接 192.168.31.100');
  addLog('[WEB] http://pcboot.local 或 http://192.168.31.100');
  addLog('[WEB] 网页密码：已设置');
  addLog('[MQTT] 连接巴法云...成功');
  addLog('[MQTT] 订阅 pcpwswb2c3006 / pcpwrstb2c3006 成功');
  addLog('[HB] 运行3600s mqtt=1 press=-1 状态=0');
  updateSim();
  refreshStatus();
  renderLog();
})();
