/*
 * 运行状态页面：查看状态、看日志、OTA 升级
 * 配色与配网页面一致：浅色 #0067C0 / 深色 #60CDFF
 */
#pragma once

const char STATUS_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>开机卡设置</title>
<style>
:root{
  --primary:#0067C0;
  --on-primary:#FFFFFF;
  --primary-container:#D3E3FD;
  --on-primary-container:#001D36;
  --surface:#F8F9FF;
  --on-surface:#1A1C1E;
  --surface-container:#EEF3FA;
  --surface-container-high:#E3E9F0;
  --on-surface-variant:#44474E;
  --outline:#74777F;
  --outline-variant:#C4C6D0;
  --ok:#1E8E3E;
  --bad:#C5221F;
}
@media(prefers-color-scheme:dark){
  :root{
    --primary:#60CDFF;
    --on-primary:#003353;
    --primary-container:#004A77;
    --on-primary-container:#CBE6FF;
    --surface:#101418;
    --on-surface:#E2E2E9;
    --surface-container:#1D2024;
    --surface-container-high:#282A2F;
    --on-surface-variant:#C4C6D0;
    --outline:#8E9099;
    --outline-variant:#44474E;
    --ok:#81C995;
    --bad:#F28B82;
  }
}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei",sans-serif;
  background:var(--surface);color:var(--on-surface);
  min-height:100vh;padding:32px 16px 48px;
  -webkit-font-smoothing:antialiased;line-height:1.5;
}
.wrap{width:100%;max-width:520px;margin:0 auto}
.header{margin-bottom:28px;padding:0 4px}
.header h1{font-size:28px;font-weight:500;letter-spacing:-.2px;line-height:1.3}
.header p{font-size:14px;color:var(--on-surface-variant);margin-top:6px}
.card{
  background:var(--surface-container);
  border-radius:28px;
  padding:24px 20px;
  margin-bottom:16px;
}
.card h2{
  font-size:14px;font-weight:500;
  color:var(--on-surface-variant);
  margin-bottom:20px;letter-spacing:.1px;
}
.row{
  display:flex;justify-content:space-between;align-items:center;
  padding:10px 0;font-size:15px;
}
.row span:first-child{color:var(--on-surface-variant)}
.ok{color:var(--ok);font-weight:500}
.bad{color:var(--bad);font-weight:500}
#log{
  background:#0B0E10;color:#C8E6C9;
  font-family:Consolas,"SF Mono",monospace;font-size:12px;
  border-radius:20px;padding:16px;height:260px;overflow-y:auto;
  white-space:pre-wrap;word-break:break-all;line-height:1.5;
}
.btn{
  display:inline-flex;align-items:center;justify-content:center;
  height:48px;padding:0 24px;
  font-size:15px;font-weight:500;font-family:inherit;
  color:var(--on-primary);background:var(--primary);
  border:none;border-radius:20px;cursor:pointer;
  transition:opacity .15s;letter-spacing:.1px;
}
.btn:active{opacity:.82}
.btn:disabled{opacity:.38;cursor:not-allowed}
.btn.ghost{
  background:var(--primary-container);
  color:var(--on-primary-container);
}
.btns{display:flex;gap:10px;flex-wrap:wrap;margin-top:16px}
input[type=file]{
  width:100%;padding:16px;
  border:1px dashed var(--outline);border-radius:16px;
  background:transparent;color:var(--on-surface);
  font-family:inherit;font-size:14px;
}
#bar{height:8px;background:var(--surface-container-high);border-radius:4px;overflow:hidden;margin-top:16px;display:none}
#bar div{height:100%;width:0;background:var(--primary);transition:width .2s;border-radius:4px}
#msg{font-size:13px;color:var(--on-surface-variant);margin-top:10px;min-height:20px}

/* WiFi 列表 */
.wifi-item{
  display:flex;justify-content:space-between;align-items:center;
  padding:14px 16px;border-radius:16px;cursor:pointer;
  transition:background .15s;
}
.wifi-item:hover{background:var(--surface-container-high)}
.wifi-item.active{background:var(--primary);color:var(--on-primary)}
.wifi-item .name{font-size:15px;font-weight:500}
.wifi-item .meta{font-size:12px;color:var(--on-surface-variant)}
.wifi-item.active .meta{color:var(--on-primary);opacity:.85}

/* 选中网络后的面板 */
.picker{
  background:var(--surface-container-high);
  border-radius:20px;padding:20px;margin-top:16px;
}
.picker-head{
  display:flex;justify-content:space-between;align-items:baseline;
  margin-bottom:16px;
}
.picker-ssid{font-size:16px;font-weight:500}
.picker-rssi{font-size:12px;color:var(--on-surface-variant)}
.picker input{
  width:100%;padding:16px;font-size:16px;font-family:inherit;
  background:transparent;color:var(--on-surface);
  border:1px solid var(--outline);border-radius:16px;outline:none;
  transition:border-color .15s;
}
.picker input:focus{border-color:var(--primary);border-width:2px;padding:15px}

/* 表单字段 */
.field{margin-bottom:20px}
.field:last-child{margin-bottom:0}
.field label{
  display:block;font-size:12px;font-weight:500;
  color:var(--on-surface-variant);margin-bottom:8px;letter-spacing:.4px;
}
.field input{
  width:100%;padding:16px;font-size:16px;font-family:inherit;
  background:transparent;color:var(--on-surface);
  border:1px solid var(--outline);border-radius:16px;outline:none;
  transition:border-color .15s;
}
.field input:focus{border-color:var(--primary);border-width:2px;padding:15px}
.field input::placeholder{color:var(--on-surface-variant);opacity:.7}
</style>
</head>
<body>
<div class="wrap">
<div class="header">
  <h1>开机卡设置</h1>
  <p>ESP8285 · 状态与维护</p>
</div>

<div class="card">
  <h2>设备状态</h2>
  <div class="row"><span>WiFi</span><span id="wifi">-</span></div>
  <div class="row"><span>信号强度</span><span id="rssi">-</span></div>
  <div class="row"><span>IP 地址</span><span id="ip">-</span></div>
  <div class="row"><span>MQTT</span><span id="mqtt">-</span></div>
  <div class="row"><span>电脑状态</span><span id="power">-</span></div>
  <div class="row"><span>运行时间</span><span id="uptime">-</span></div>
  <div class="row"><span>剩余内存</span><span id="heap">-</span></div>
</div>

<div class="card">
  <h2>网络设置</h2>
  <div class="row"><span>当前网络</span><span id="curSsid">-</span></div>
  <div class="btns">
    <button class="btn ghost" id="scanBtn" onclick="doScan()">扫描周围 WiFi</button>
  </div>
  <div id="scanList" style="display:none;margin-top:12px">
    <div id="scanItems"></div>
  </div>
  <div id="wifiForm" class="picker" style="display:none">
    <div class="picker-head">
      <div class="picker-ssid" id="pickSsid">-</div>
      <div class="picker-rssi" id="pickRssi">-</div>
    </div>
    <input type="password" id="newPass" placeholder="WiFi 密码">
    <div class="btns">
      <button class="btn" onclick="doSaveWifi()">连接</button>
      <button class="btn ghost" onclick="cancelWifi()">取消</button>
    </div>
  </div>
  <div id="wifiMsg" class="msg"></div>
</div>

<div class="card">
  <h2>网页密码</h2>
  <div class="field">
    <label for="oldPass">当前密码</label>
    <input type="password" id="oldPass" placeholder="没设密码就留空">
  </div>
  <div class="field">
    <label for="newWebPass">新密码</label>
    <input type="password" id="newWebPass" placeholder="留空表示取消密码">
  </div>
  <div class="btns">
    <button class="btn" onclick="doChangePass()">保存密码</button>
  </div>
  <div id="passMsg" class="msg"></div>
</div>

<div class="card">
  <h2>运行日志</h2>
  <div id="log">加载中...</div>
  <div class="btns">
    <button class="btn ghost" id="pauseBtn" onclick="togglePause()">暂停滚动</button>
    <button class="btn ghost" onclick="clearLog()">清屏</button>
  </div>
</div>

<div class="card">
  <h2>固件升级</h2>
  <input type="file" id="fw" accept=".bin">
  <div id="bar"><div id="barInner"></div></div>
  <div id="msg"></div>
  <div class="btns">
    <button class="btn" id="upBtn" onclick="doUpload()">上传并升级</button>
    <button class="btn ghost" onclick="doRestart()">重启设备</button>
    <button class="btn ghost" onclick="doLogout()">退出登录</button>
  </div>
</div>

<script>
let paused=false, timer=null;

function fmt(s){
  const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60),x=s%60;
  if(d)return d+'天'+h+'小时';
  if(h)return h+'小时'+m+'分';
  if(m)return m+'分'+x+'秒';
  return x+'秒';
}

async function refresh(){
  try{
    const r=await fetch('/api/status');
    if(r.status===401){location.reload();return;}
    const d=await r.json();
    document.getElementById('wifi').innerHTML=
      d.wifi?'<span class="ok">已连接</span>':'<span class="bad">未连接</span>';
    document.getElementById('rssi').textContent=d.rssi+' dBm';
    document.getElementById('ip').textContent=d.ip;
    document.getElementById('mqtt').innerHTML=
      d.mqtt?'<span class="ok">已连接</span>':'<span class="bad">未连接</span>';
    document.getElementById('power').innerHTML=
      d.power?'<span class="ok">开机</span>':'关机';
    document.getElementById('uptime').textContent=fmt(d.uptime);
    document.getElementById('heap').textContent=(d.heap/1024).toFixed(1)+' KB';
    document.getElementById('curSsid').textContent=d.ssid||'-';
  }catch(e){}
}

async function loadLog(){
  if(paused)return;
  try{
    const r=await fetch('/api/log');
    if(r.status===401){location.reload();return;}
    const t=await r.text();
    const el=document.getElementById('log');
    const atBottom=el.scrollHeight-el.scrollTop-el.clientHeight<40;
    el.textContent=t||'(暂无日志)';
    if(atBottom)el.scrollTop=el.scrollHeight;
  }catch(e){}
}

async function doLogout(){
  await fetch('/logout');
  location.reload();
}

// ---------- 扫描并切换 WiFi ----------
let scanTimer=null;

async function doScan(){
  const btn=document.getElementById('scanBtn');
  const msg=document.getElementById('wifiMsg');
  const list=document.getElementById('scanList');
  if(scanTimer){clearInterval(scanTimer);scanTimer=null;}
  btn.disabled=true;btn.textContent='扫描中...';
  msg.textContent='扫描时 WiFi 会短暂中断，请稍候';
  list.style.display='none';
  document.getElementById('wifiForm').style.display='none';

  try{ await fetch('/api/scan'); }catch(e){}

  let tries=0;
  scanTimer=setInterval(async function(){
    tries++;
    if(tries>25){
      clearInterval(scanTimer);scanTimer=null;
      btn.disabled=false;btn.textContent='重新扫描';
      msg.textContent='扫描超时，请重试';
      return;
    }
    try{
      const r=await fetch('/api/scanResult');
      if(r.status===401){location.reload();return;}
      const j=await r.json();
      if(j.running)return;
      clearInterval(scanTimer);scanTimer=null;
      btn.disabled=false;btn.textContent='重新扫描';
      if(!Array.isArray(j)){
        msg.textContent=j.msg||'扫描失败';
        return;
      }
      renderScan(j);
      msg.textContent='找到 '+j.length+' 个网络，点一个切换';
    }catch(e){}
  },1000);
}

let pickedSsid='';

function renderScan(list){
  const box=document.getElementById('scanItems');
  box.innerHTML='';
  if(!list.length){
    const d=document.createElement('div');
    d.className='wifi-item';
    const s=document.createElement('span');
    s.className='name';
    s.textContent='没找到网络';
    d.appendChild(s);
    box.appendChild(d);
  }
  list.forEach(function(w){
    const d=document.createElement('div');
    d.className='wifi-item';
    // 用 textContent 而不是拼 HTML：WiFi 名字里可能带尖括号之类的字符，
    // 拼进去会把页面结构搞乱。
    const name=document.createElement('span');
    name.className='name';
    name.textContent=w.ssid+(w.enc?' 🔒':'');
    const meta=document.createElement('span');
    meta.className='meta';
    meta.textContent=w.rssi+' dBm';
    d.appendChild(name);
    d.appendChild(meta);
    d.onclick=function(){
      document.querySelectorAll('.wifi-item').forEach(function(el){el.classList.remove('active');});
      d.classList.add('active');
      pickWifi(w.ssid,w.enc,w.rssi);
    };
    box.appendChild(d);
  });
  document.getElementById('scanList').style.display='block';
}

function pickWifi(ssid,enc,rssi){
  pickedSsid=ssid;
  document.getElementById('pickSsid').textContent=ssid;
  document.getElementById('pickRssi').textContent=rssi+' dBm';
  const p=document.getElementById('newPass');
  p.value='';
  p.placeholder=enc?'输入 WiFi 密码':'开放网络，无需密码';
  document.getElementById('wifiForm').style.display='block';
  document.getElementById('wifiMsg').textContent='';
}

function cancelWifi(){
  pickedSsid='';
  document.getElementById('wifiForm').style.display='none';
  document.getElementById('wifiMsg').textContent='';
  document.querySelectorAll('.wifi-item').forEach(function(el){el.classList.remove('active');});
}

async function doSaveWifi(){
  const pass=document.getElementById('newPass').value;
  const msg=document.getElementById('wifiMsg');
  if(!pickedSsid){msg.textContent='请先扫描并选择一个网络';return;}
  if(!confirm('确定切换到「'+pickedSsid+'」并重启？\n切换后设备会断开当前连接，需要连上新网络才能再访问。'))return;
  msg.textContent='正在保存...';
  try{
    const r=await fetch('/api/wifi',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({ssid:pickedSsid,pass:pass})
    });
    const j=await r.json();
    if(j.ok){
      msg.textContent='已保存，设备正在重启并连接新网络';
    }else{
      msg.textContent=j.msg||'保存失败';
    }
  }catch(e){
    msg.textContent='请求失败，请重试';
  }
}

// ---------- 修改网页密码 ----------
async function doChangePass(){
  const oldPass=document.getElementById('oldPass').value;
  const newPass=document.getElementById('newWebPass').value;
  const msg=document.getElementById('passMsg');
  if(!newPass && !oldPass){
    msg.textContent='请填写新密码；新密码留空表示取消密码';
    return;
  }
  const tip=newPass?'确定修改网页密码？修改后需要重新登录。':'确定取消网页密码？之后任何人都能访问。';
  if(!confirm(tip))return;
  msg.textContent='正在保存...';
  try{
    const r=await fetch('/api/pass',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({oldPass:oldPass,newPass:newPass})
    });
    const j=await r.json();
    if(j.ok){
      msg.textContent='已保存，正在重新登录...';
      setTimeout(async function(){
        try{ await fetch('/logout'); }catch(e){}
        location.href='/';
      },1200);
    }else{
      msg.textContent=j.msg||'保存失败';
    }
  }catch(e){
    msg.textContent='请求失败，请重试';
  }
}

function togglePause(){
  paused=!paused;
  document.getElementById('pauseBtn').textContent=paused?'继续滚动':'暂停滚动';
}

function clearLog(){document.getElementById('log').textContent='';}

async function doRestart(){
  if(!confirm('确定重启设备？'))return;
  await fetch('/api/restart');
  document.getElementById('msg').textContent='设备正在重启，稍等几秒刷新页面';
}

function doUpload(){
  const f=document.getElementById('fw').files[0];
  const msg=document.getElementById('msg');
  if(!f){msg.textContent='请先选择 .bin 固件文件';return;}
  if(!confirm('确定上传 '+f.name+' 并升级？升级过程中不要断电。'))return;

  const fd=new FormData();
  fd.append('firmware',f);
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/update');
  const bar=document.getElementById('bar');
  const inner=document.getElementById('barInner');
  bar.style.display='block';
  document.getElementById('upBtn').disabled=true;
  msg.textContent='正在上传...';

  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      const p=Math.round(e.loaded/e.total*100);
      inner.style.width=p+'%';
      msg.textContent='正在上传 '+p+'%';
    }
  };
  xhr.onload=function(){
    // 光看状态码不够，还要确认返回内容里确实有 OK
    if(xhr.status===200 && xhr.responseText.indexOf('OK')>=0){
      inner.style.width='100%';
      msg.textContent='升级成功，设备正在重启，约 10 秒后刷新页面';
    }else{
      msg.textContent='升级失败：'+(xhr.responseText||('HTTP '+xhr.status));
      document.getElementById('upBtn').disabled=false;
    }
  };
  xhr.onerror=function(){
    msg.textContent='上传中断，请检查网络后重试';
    document.getElementById('upBtn').disabled=false;
  };
  xhr.send(fd);
}

refresh();loadLog();
timer=setInterval(function(){refresh();loadLog();},2000);
</script>
</div>
</body>
</html>)rawliteral";
