#ifndef WEB_PAGE_H
#define WEB_PAGE_H

// 配网页面：Material You 风格，纯色、大圆角、色块分层
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0067C0">
<title>电脑开机卡</title>
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
  }
}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei",sans-serif;
  background:var(--surface);
  color:var(--on-surface);
  min-height:100vh;
  padding:32px 16px 48px;
  display:flex;flex-direction:column;align-items:center;
  -webkit-font-smoothing:antialiased;
}
.wrap{width:100%;max-width:440px}
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
  margin-bottom:20px;
  letter-spacing:.1px;
}
.field{margin-bottom:20px}
.field:last-child{margin-bottom:0}
label{
  display:block;font-size:12px;font-weight:500;
  color:var(--on-surface-variant);
  margin-bottom:8px;letter-spacing:.4px;
}
input,select{
  width:100%;padding:16px;
  font-size:16px;
  background:transparent;
  color:var(--on-surface);
  border:1px solid var(--outline);
  border-radius:16px;
  outline:none;
  transition:border-color .15s;
  font-family:inherit;
  appearance:none;
}
select{
  background-image:url("data:image/svg+xml;charset=utf-8,%3Csvg xmlns='http://www.w3.org/2000/svg' width='24' height='24' viewBox='0 0 24 24' fill='none' stroke='%2379747E' stroke-width='2' stroke-linecap='round'%3E%3Cpath d='M6 9l6 6 6-6'/%3E%3C/svg%3E");
  background-repeat:no-repeat;
  background-position:right 14px center;
  background-size:20px;
  padding-right:44px;
}
input:focus,select:focus{
  border-color:var(--primary);
  border-width:2px;
  padding:15px;
}
select:focus{padding-right:43px}
input::placeholder{color:var(--on-surface-variant);opacity:.7}
.row{display:flex;gap:12px;align-items:stretch}
.row select{flex:1}
.btn{
  width:100%;height:56px;
  font-size:16px;font-weight:500;
  color:var(--on-primary);
  background:var(--primary);
  border:none;border-radius:20px;
  cursor:pointer;
  transition:opacity .15s;
  font-family:inherit;
  letter-spacing:.1px;
}
.btn:active{opacity:.82}
.btn:disabled{opacity:.38;cursor:not-allowed}
.btn-tonal{
  width:auto;height:56px;
  padding:0 24px;
  font-size:14px;font-weight:500;
  color:var(--on-primary-container);
  background:var(--primary-container);
  border:none;border-radius:20px;
  cursor:pointer;flex-shrink:0;
  font-family:inherit;
  transition:opacity .15s;
}
.btn-tonal:active{opacity:.82}
.hint{
  font-size:12px;line-height:1.6;
  color:var(--on-surface-variant);
  margin-top:10px;
}
.hint a{color:var(--primary);text-decoration:none;font-weight:500}
.segmented{
  display:flex;
  background:var(--surface-container-high);
  border-radius:20px;
  padding:4px;
  margin-bottom:20px;
}
.seg{
  flex:1;
  padding:12px 8px;
  border:none;
  background:transparent;
  color:var(--on-surface-variant);
  border-radius:16px;
  font-size:14px;
  font-weight:500;
  cursor:pointer;
  font-family:inherit;
  transition:background .15s,color .15s;
  outline:none;
  -webkit-tap-highlight-color:transparent;
}
.seg:focus{outline:none}
.seg:focus-visible{outline:2px solid var(--primary);outline-offset:2px}
.seg.active{
  background:var(--primary);
  color:var(--on-primary);
}
.status{
  margin-top:16px;padding:16px;
  border-radius:16px;
  font-size:14px;text-align:center;
  display:none;
}
.status.show{display:block}
.status.ok{background:var(--primary-container);color:var(--on-primary-container)}
.status.err{background:var(--surface-container-high);color:var(--on-surface)}
.spinner{
  display:inline-block;width:16px;height:16px;
  border:2px solid rgba(255,255,255,.35);
  border-top-color:currentColor;border-radius:50%;
  animation:spin .8s linear infinite;
  vertical-align:-3px;margin-right:8px;
}
@keyframes spin{to{transform:rotate(360deg)}}
.footer{
  text-align:center;font-size:12px;
  color:var(--on-surface-variant);
  margin-top:20px;
}
</style>
</head>
<body>
<div class="wrap">
  <div class="header">
    <h1>电脑开机卡</h1>
    <p>ESP8285 · 巴法云 / Home Assistant</p>
  </div>

  <div class="card">
    <h2>WiFi 连接</h2>
    <div class="field">
      <label for="ssid">网络名称</label>
      <div class="row">
        <select id="ssid">
          <option value="">正在扫描...</option>
        </select>
        <button class="btn-tonal" onclick="scan()">刷新</button>
      </div>
    </div>
    <div class="field">
      <label for="pass">WiFi 密码</label>
      <input type="password" id="pass" placeholder="输入密码" autocomplete="off">
    </div>
  </div>

  <div class="card">
    <h2>连接方式</h2>
    <div class="segmented">
      <button class="seg active" id="segBemfa" onclick="setMode(0)">巴法云</button>
      <button class="seg" id="segHa" onclick="setMode(1)">Home Assistant</button>
    </div>

    <div id="bemfaFields">
      <div class="field">
        <label for="uid">私钥（UID）</label>
        <input type="text" id="uid" placeholder="32 位私钥" maxlength="32" autocomplete="off">
        <div class="hint">
          登录 <a href="https://cloud.bemfa.com/mdevice.php" target="_blank">巴法云 MQTT 控制台</a>，
          首页即可看到私钥。两个主题会自动创建，不用手动建。
        </div>
      </div>
    </div>

    <div id="haFields" style="display:none">
      <div class="field">
        <label for="mqttHost">MQTT 服务器</label>
        <input type="text" id="mqttHost" placeholder="例如 192.168.1.100" autocomplete="off">
      </div>
      <div class="field">
        <label for="mqttPort">端口</label>
        <input type="number" id="mqttPort" placeholder="1883" value="1883" inputmode="numeric">
        <div class="hint">
          填 Home Assistant 所在机器的地址。设备会自动注册三个实体：电源按钮、重启按钮、状态传感器。
        </div>
      </div>
    </div>
  </div>

  <div class="card">
    <h2>网页安全</h2>
    <div class="field">
      <label for="webPass">网页访问密码</label>
      <input type="password" id="webPass" placeholder="留空则不设密码" autocomplete="new-password">
      <div class="hint">
        设置后，同一局域网打开设备网页需要先输入这个密码。留空表示不设密码。
      </div>
    </div>
  </div>

  <button class="btn" id="saveBtn" onclick="save()">保存并重启</button>
  <div class="status" id="status"></div>
  <div class="footer">保存后设备会自动重启并连接</div>
</div>

<script>
let scanning=false;
let mode=0;

function setMode(m){
  mode=m;
  document.getElementById('segBemfa').classList.toggle('active',m===0);
  document.getElementById('segHa').classList.toggle('active',m===1);
  document.getElementById('bemfaFields').style.display=m===0?'block':'none';
  document.getElementById('haFields').style.display=m===1?'block':'none';
}

async function scan(){
  if(scanning)return;
  scanning=true;
  const sel=document.getElementById('ssid');
  sel.innerHTML='<option value="">正在扫描...</option>';
  try{
    const r=await fetch('/scan');
    const list=await r.json();
    sel.innerHTML='';
    if(!list.length){
      sel.innerHTML='<option value="">未发现网络</option>';
    }else{
      list.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
        const o=document.createElement('option');
        o.value=n.ssid;
        o.textContent=n.ssid+(n.enc?' 🔒':'');
        sel.appendChild(o);
      });
    }
  }catch(e){
    sel.innerHTML='<option value="">扫描失败</option>';
  }
  scanning=false;
}

function showStatus(msg,type){
  const el=document.getElementById('status');
  el.textContent=msg;
  el.className='status show '+type;
}

async function save(){
  const ssid=document.getElementById('ssid').value.trim();
  const pass=document.getElementById('pass').value;
  const uid=document.getElementById('uid').value.trim();
  const mqttHost=document.getElementById('mqttHost').value.trim();
  const mqttPort=parseInt(document.getElementById('mqttPort').value)||1883;
  const webPass=document.getElementById('webPass').value;

  if(!ssid){showStatus('请选择 WiFi','err');return}
  if(mode===0&&(!uid||uid.length!==32)){showStatus('私钥应为 32 位','err');return}
  if(mode===1&&!mqttHost){showStatus('请填写 MQTT 服务器地址','err');return}

  const btn=document.getElementById('saveBtn');
  btn.disabled=true;
  btn.innerHTML='<span class="spinner"></span>正在保存...';
  try{
    const r=await fetch('/save',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({ssid,pass,uid,mode,mqttHost,mqttPort,webPass})
    });
    const j=await r.json();
    if(j.ok){
      showStatus('保存成功，设备正在重启...','ok');
      btn.innerHTML='已保存';
    }else{
      showStatus(j.msg||'保存失败','err');
      btn.disabled=false;
      btn.textContent='保存并重启';
    }
  }catch(e){
    showStatus('请求失败，请重试','err');
    btn.disabled=false;
    btn.textContent='保存并重启';
  }
}

scan();
</script>
</body>
</html>
)rawliteral";

#endif
