/*
 * 网页登录页：设备设了密码才需要过这一关
 * 配色与配网页面一致：浅色 #0067C0 / 深色 #60CDFF
 */
#pragma once

const char LOGIN_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0067C0">
<title>开机卡 · 登录</title>
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
  --err:#C5221F;
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
    --err:#F28B82;
  }
}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei",sans-serif;
  background:var(--surface);color:var(--on-surface);
  min-height:100vh;display:flex;align-items:center;justify-content:center;
  padding:24px;-webkit-font-smoothing:antialiased;line-height:1.5;
}
.box{
  width:100%;max-width:380px;
  background:var(--surface-container);
  border-radius:28px;padding:32px 24px;
}
h1{font-size:24px;font-weight:500;letter-spacing:-.2px;margin-bottom:6px}
p.sub{font-size:14px;color:var(--on-surface-variant);margin-bottom:24px}
label{
  display:block;font-size:12px;font-weight:500;
  color:var(--on-surface-variant);margin-bottom:8px;letter-spacing:.4px;
}
input{
  width:100%;padding:16px;font-size:16px;font-family:inherit;
  background:transparent;color:var(--on-surface);
  border:1px solid var(--outline);border-radius:16px;outline:none;
  transition:border-color .15s;
}
input:focus{border-color:var(--primary);border-width:2px;padding:15px}
input::placeholder{color:var(--on-surface-variant);opacity:.7}
button{
  width:100%;height:56px;margin-top:20px;
  font-size:16px;font-weight:500;font-family:inherit;
  border:none;border-radius:20px;letter-spacing:.1px;
  background:var(--primary);color:var(--on-primary);cursor:pointer;
  transition:opacity .15s;
}
button:active{opacity:.82}
button:disabled{opacity:.38;cursor:not-allowed}
.msg{font-size:13px;color:var(--err);margin-top:14px;min-height:18px;text-align:center}
</style>
</head>
<body>
<div class="box">
  <h1>开机卡设置</h1>
  <p class="sub">请输入网页访问密码</p>
  <label for="pw">密码</label>
  <input type="password" id="pw" autocomplete="current-password" autofocus>
  <button id="btn" onclick="doLogin()">进入</button>
  <div class="msg" id="msg"></div>
</div>
<script>
async function doLogin(){
  const pw=document.getElementById('pw').value;
  const msg=document.getElementById('msg');
  const btn=document.getElementById('btn');
  if(!pw){msg.textContent='请输入密码';return}
  btn.disabled=true;btn.textContent='验证中...';
  try{
    const r=await fetch('/login',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({pass:pw})
    });
    const j=await r.json();
    if(j.ok){
      location.href='/';
    }else{
      msg.textContent=j.msg||'密码错误';
      btn.disabled=false;btn.textContent='进入';
      document.getElementById('pw').select();
    }
  }catch(e){
    msg.textContent='请求失败，请重试';
    btn.disabled=false;btn.textContent='进入';
  }
}
document.getElementById('pw').addEventListener('keydown',function(e){
  if(e.key==='Enter')doLogin();
});
</script>
</body>
</html>)rawliteral";
