#pragma once
#include <Arduino.h>

// Everything is served from the ESP32; no fonts, scripts or assets need Internet.
const char WEB_PAGE[] PROGMEM = R"ICWEB(
<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#f5f6f8">
<title>ICCard · 卡片备份</title>
<style>
:root{color-scheme:light;--ink:#17243c;--muted:#718096;--line:#e5e9f0;--blue:#265ce3;--green:#147d62;--paper:#fff;--bg:#f5f6f8}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.65 -apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei",sans-serif}button,a{-webkit-tap-highlight-color:transparent}button{font:inherit;cursor:pointer}button:disabled{cursor:default;opacity:.42}button:focus-visible,a:focus-visible,summary:focus-visible{outline:3px solid #8baaff;outline-offset:4px}button,dialog{border:0}button{touch-action:manipulation}[hidden]{display:none!important}
.wrap{max-width:820px;margin:auto;padding:24px 24px 40px}.top{display:flex;align-items:center;justify-content:space-between;gap:8px}.brand{display:flex;align-items:center;gap:10px;font-size:19px;font-weight:750;letter-spacing:-.5px}.mark{height:34px;width:34px;border-radius:11px;background:var(--ink);display:grid;place-items:center;color:#fff}.mark svg{width:22px}.badge{display:flex;align-items:center;gap:7px;color:var(--green);background:#e7f4ed;padding:5px 10px;border-radius:20px;font-size:11px;white-space:nowrap}.dot{width:6px;height:6px;border-radius:50%;background:currentColor}.badge.off{background:#eceff4;color:var(--muted)}h1{font-size:32px;letter-spacing:-1px;margin:29px 0 4px;line-height:1.35}.intro{color:var(--muted);margin:0 0 23px}.eyebrow{font-size:10px;letter-spacing:1.8px;color:var(--muted);font-weight:650}.card{background:#fff;border:1px solid var(--line);border-radius:18px;padding:20px}.status{margin-bottom:16px}.status-head{display:flex;align-items:center;justify-content:space-between;gap:10px}.status-title{display:flex;align-items:center;gap:9px;font-weight:650}.status-icon{height:26px;width:26px;display:grid;place-items:center;border-radius:50%;background:#eaf1ff;color:var(--blue);font-size:16px}.message{margin:9px 0 0;color:#56647a;font-size:13px;overflow-wrap:anywhere}.detail{font:11px/1.5 ui-monospace,monospace;color:#a44637;margin:9px 0 0;overflow-wrap:anywhere}.counts{font:12px ui-monospace,monospace;color:var(--muted)}.track{height:5px;background:#edf0f7;border-radius:5px;margin-top:16px;overflow:hidden}.bar{height:100%;background:var(--blue);width:0;transition:width .2s}.status.error{border-color:#ebccc3;background:#fffaf8}.status.error .status-icon{color:#ab5033;background:#ffebe4}.status.success .status-icon{color:var(--green);background:#e5f4ec}.hardware{display:flex;gap:16px;margin-top:15px;font-size:11px;color:var(--muted)}.hardware span{display:flex;gap:6px;align-items:center}.hardware .dot{color:#adc0cf}.hardware .ready .dot{color:#29a27a}
.cards{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:24px}.tag-card{min-height:155px;position:relative;overflow:hidden;border-radius:16px;padding:18px;background:#eaf0f8;border:1px solid #e0e6ef}.tag-card.source{background:#1b2e4d;color:#fff;border-color:#1b2e4d}.tag-card:after{content:"";position:absolute;right:-40px;bottom:-60px;width:170px;height:170px;border:1px solid #ffffff22;border-radius:50%;box-shadow:0 0 0 22px #ffffff09,0 0 0 44px #ffffff06;pointer-events:none}.tag-card.source .eyebrow{color:#9eb3d3}.tag-label{font-size:12px;display:flex;justify-content:space-between;align-items:center}.tag-label svg{width:19px;opacity:.65}.uid{display:block;font:600 23px/1.3 ui-monospace,SFMono-Regular,Consolas,monospace;letter-spacing:1px;margin:18px 0 9px;overflow-wrap:anywhere}.tag-meta{font-size:10px;color:#65768b;position:relative;z-index:1}.source .tag-meta{color:#acc0dc}.section-title{margin:0;font-size:16px;font-weight:650}.section-heading{display:flex;align-items:center;justify-content:space-between;margin-bottom:13px}.section-hint{font-size:11px;color:var(--muted)}.actions{display:grid;grid-template-columns:1fr 1fr;gap:12px}.action{padding:16px;text-align:left;border-radius:14px;border:1px solid var(--line);background:#fff;color:var(--ink);display:flex;align-items:flex-start;gap:12px;min-height:88px}.action:hover:not(:disabled){border-color:#91abe3;background:#fafdff}.action.primary{background:var(--blue);border-color:var(--blue);color:#fff}.action.primary:hover:not(:disabled){background:#1e4fc7}.action svg{width:21px;height:21px;flex-shrink:0;margin-top:3px}.action strong{display:block;font-size:14px;font-weight:600}.action small{display:block;color:var(--muted);font-size:10px;margin-top:3px}.action.primary small{color:#d3dfff}.export{width:100%;display:flex;align-items:center;justify-content:space-between;padding:14px 0;background:none;color:var(--blue);font-size:12px;margin-top:5px}.export svg{width:16px;height:16px}.notice{font-size:11px;line-height:1.8;color:var(--muted);background:#eaf0f5;padding:12px 14px;border-radius:11px;margin:4px 0 18px}.notice strong{font-weight:600;color:#53637a}.confirm{margin:0 0 22px;border-color:#c3d3f4;box-shadow:0 6px 22px #1e51a90b;background:#f9fbff}.confirm h2{font-size:16px;margin:0}.confirm p{font-size:12px;color:#61708a;margin:8px 0}.confirm-target{font:650 27px ui-monospace,monospace;color:var(--blue);letter-spacing:2px;margin:17px 0 12px}.confirm-actions{display:flex;gap:10px;margin-top:16px}.solid,.ghost{padding:11px 16px;border-radius:10px;font-weight:600;font-size:13px}.solid{background:var(--blue);color:#fff;flex:1}.ghost{background:#eaf0f7;color:#52627a}.timer{font:12px ui-monospace,monospace;color:var(--blue)}details{border-top:1px solid var(--line);padding-top:15px;font-size:12px;color:var(--muted)}summary{cursor:pointer}.delete{color:#a95043;background:none;padding:12px 0 5px;font-size:12px}.foot{margin:26px 0 0;text-align:center;font-size:10px;color:#99a3b2;letter-spacing:.4px}.toast{border-radius:11px;padding:12px 14px;margin:0 0 15px;background:#fff0e8;color:#994728;font-size:12px;overflow-wrap:anywhere}.connection-tip{font-size:12px;color:#7b643c;background:#fff6df;border-radius:12px;padding:14px;margin:15px 0}.connection-tip a{color:inherit}dialog{width:calc(100% - 36px);max-width:390px;border-radius:18px;padding:24px;color:var(--ink);box-shadow:0 18px 70px #0b1f4140}dialog::backdrop{background:#11244180}dialog h2{font-size:18px;margin:0 0 12px}dialog p{font-size:13px;color:#63718a;white-space:pre-line}dialog .confirm-actions{justify-content:flex-end}.danger{background:#ad5041}
@media(max-width:480px){.wrap{padding:19px 18px 30px}h1{font-size:29px;margin-top:25px}.intro{font-size:12px}.card{padding:16px}.cards{gap:10px}.tag-card{padding:15px 13px;min-height:146px}.uid{font-size:20px;letter-spacing:.6px}.tag-meta{font-size:9px}.action{padding:14px 11px;gap:9px}.action strong{font-size:13px}.action small{font-size:9px}.section-hint{font-size:10px}.hardware{gap:13px}}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
.block0{margin-bottom:18px}.block0 h2{margin:0;font-size:15px}.block0 p{margin:8px 0 13px;font-size:12px;color:#61708a}.block0 .action{width:100%;min-height:0}.replacement{font-size:12px;color:#61708a}.replacement strong{display:block;margin:5px 0;font:650 27px ui-monospace,monospace;color:var(--blue);letter-spacing:2px}.confirm-target{overflow-wrap:anywhere}
.diagnostics{margin:0 0 18px;padding:13px 16px;border:1px solid var(--line);border-radius:12px;background:#fff}.diagnostics p{font-size:11px;margin:8px 0;color:var(--muted)}.diagnostics pre{font:11px/1.7 ui-monospace,monospace;color:#43536b;white-space:pre-wrap;overflow-wrap:anywhere;max-height:260px;overflow:auto;user-select:text;-webkit-user-select:text;margin:9px 0 0}
.method-label{display:block;font-size:12px;color:#61708a;margin:12px 0 6px}.method-select{width:100%;font:inherit;color:var(--ink);background:#fff;border:1px solid var(--line);border-radius:10px;padding:10px;margin-bottom:12px}.method-select:disabled{opacity:.55}
</style>
</head>
<body>
<svg style="display:none" xmlns="http://www.w3.org/2000/svg"><symbol id="i-card" viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="14" rx="3"/><path d="M7 10h4M7 14h2m7-5c2 1 2 5 0 6"/></symbol><symbol id="i-scan" viewBox="0 0 24 24"><path d="M8 3H4v4m12-4h4v4M4 17v4h4m12-4v4h-4M3 12h18"/></symbol><symbol id="i-save" viewBox="0 0 24 24"><path d="M5 3h12l4 4v14H3V3h2Z"/><path d="M7 3v6h9V3M7 21v-8h10v8"/></symbol><symbol id="i-write" viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="15" rx="3"/><path d="M12 2v11m-4-4 4 4 4-4M7 16h3"/></symbol><symbol id="i-check" viewBox="0 0 24 24"><path d="M12 3 3 7v5c0 5 9 9 9 9s9-4 9-9V7l-9-4Z"/><path d="m8 12 3 3 5-6"/></symbol><symbol id="i-arrow" viewBox="0 0 24 24"><path d="M12 3v13m-5-5 5 5 5-5M4 16v5h16v-5"/></symbol></svg>
<main class="wrap">
<header class="top"><div class="brand"><span class="mark"><svg fill="none" stroke="currentColor" stroke-width="1.5"><use href="#i-card"/></svg></span>ICCard<span style="font-size:10px;font-weight:500;color:#99a3b2;letter-spacing:1px;margin-left:2px">S3</span></div><span id="connection" class="badge off"><i class="dot"></i><span id="connectionText">正在连接</span></span></header>
<h1>卡片备份，触手可及。</h1><p class="intro">把卡片贴近读卡器，在这里完成备份与恢复。</p>
<div id="offlineTip" class="connection-tip" hidden>暂时无法连接设备。请保持连接 <strong id="networkName">ICCard-S3</strong> 热点；如果手机提示无互联网，请选择继续使用此网络。浏览器地址为 <a href="http://192.168.4.1/">http://192.168.4.1</a>。</div>
<div id="toast" class="toast" role="alert" hidden></div>
<section id="statusPanel" class="card status" aria-live="polite"><div class="status-head"><div class="status-title"><span id="statusIcon" class="status-icon">·</span><span id="statusTitle">连接设备中</span></div><span id="counts" class="counts"></span></div><p id="message" class="message">正在获取读卡器与备份状态…</p><p id="detail" class="detail" hidden></p><div id="track" class="track" hidden><div id="bar" class="bar"></div></div><div class="hardware"><span id="readerState"><i class="dot"></i><span>读卡器待检查</span></span><span id="storageState"><i class="dot"></i><span>存储待检查</span></span></div></section>
<details id="diagnosticsPanel" class="diagnostics" hidden><summary>查看最近操作的详细诊断</summary><p>只保留最近一次操作的检查步骤，可长按或选中文字复制。</p><pre id="diagnostics" tabindex="0"></pre></details>
<div class="cards"><section class="tag-card source"><div class="tag-label"><span>已保存的原卡备份</span><svg fill="none" stroke="currentColor" stroke-width="1.5"><use href="#i-save"/></svg></div><span id="sourceUid" class="uid">暂无备份</span><div id="sourceMeta" class="tag-meta">备份原卡后，断电仍可保留</div></section><section class="tag-card"><div class="tag-label"><span>最近读取的卡片</span><svg fill="none" stroke="currentColor" stroke-width="1.5"><use href="#i-card"/></svg></div><span id="cardUid" class="uid">等待放卡</span><div id="cardType" class="tag-meta">点击「识别卡片」重新读取卡号</div></section></div>
<section id="confirmPanel" class="card confirm" hidden><div class="status-head"><h2 id="confirmTitle">核对目标卡</h2><span id="countdown" class="timer"></span></div><p id="confirmIntro">将备份数据写入以下卡片：</p><div id="targetUid" class="confirm-target"></div><div id="replacementRow" class="replacement" hidden>写入后卡号<strong id="replacementUid"></strong></div><p id="confirmWarning">会覆盖目标卡的 47 个用户数据块。请只保留这张目标卡，并在写入完成前保持不动。</p><p id="block0Bytes" class="detail" hidden></p><div class="confirm-actions"><button id="cancelWrite" class="ghost" type="button">取消</button><button id="confirmWrite" class="solid" type="button">确认写入这张卡</button></div></section>
<section aria-labelledby="operations"><div class="section-heading"><h2 id="operations" class="section-title">开始操作</h2><span class="section-hint">一次放置一张卡片</span></div><div class="actions">
<button id="info" class="action" type="button" disabled><svg fill="none" stroke="currentColor" stroke-width="1.6"><use href="#i-scan"/></svg><span><strong>识别卡片</strong><small>查看卡号与卡片类型</small></span></button>
<button id="backup" class="action primary" type="button" disabled><svg fill="none" stroke="currentColor" stroke-width="1.6"><use href="#i-save"/></svg><span><strong>备份原卡</strong><small>读取并保存到设备</small></span></button>
<button id="restore" class="action" type="button" disabled><svg fill="none" stroke="currentColor" stroke-width="1.6"><use href="#i-write"/></svg><span><strong>检查目标卡</strong><small>换上空白卡，准备恢复</small></span></button>
<button id="verify" class="action" type="button" disabled><svg fill="none" stroke="currentColor" stroke-width="1.6"><use href="#i-check"/></svg><span><strong>校验目标卡</strong><small>与板上备份逐块比较</small></span></button>
</div></section>
<button id="download" class="export" type="button" disabled><span>下载已保存的备份 · JSON</span><svg fill="none" stroke="currentColor" stroke-width="1.6"><use href="#i-arrow"/></svg></button>
<p class="notice"><strong>普通恢复包含 47 个用户数据块。</strong>不修改卡号、密钥和访问权限。下载的是板上保存的原卡备份，不会重新读取当前卡片；本版本暂不支持从文件导入。</p>
<section class="card block0" aria-labelledby="block0Title"><h2 id="block0Title">单独复制卡号与第 0 块</h2><p>适用于 4 字节 UID 的 M1 / S50 可改卡号卡。将复制备份第 0 块的全部 16 字节，包含卡号和厂家信息。</p><p>先恢复并校验 47 块，最后再写第 0 块。成功后点击「识别卡片」查看新卡号；卡号与原卡相同后，普通恢复会按原卡保护规则拒绝再次写入。</p><label for="block0Method" class="method-label">第 0 块写入方式</label><select id="block0Method" class="method-select"><option value="auto">自动选择：先 UID / Gen1A，再普通认证</option><option value="gen1a">仅 UID / Gen1A 后门方式</option><option value="cuid">仅 CUID / Gen2 标准认证方式</option></select><button id="prepareBlock0" class="action" type="button" disabled><svg fill="none" stroke="currentColor" stroke-width="1.6"><use href="#i-scan"/></svg><span><strong>检查第 0 块写入</strong><small>显示所选写法，确认前不写入</small></span></button><p>自动模式只选择待尝试的写法，不鉴定芯片。普通认证和读取成功不能证明第 0 块可写；一次性卡可能在实际写入后锁定。</p><p id="block0Hint">需要已保存的有效 4 字节 UID 原卡备份。</p></section>
<details><summary>更多操作与连接信息</summary><p>热点：<span id="ssid">ICCard-S3</span><br>操作地址：http://192.168.4.1<br>设备不需要连接互联网，USB 供电即可使用。</p><button id="erase" class="delete" type="button" disabled>删除板上备份</button></details>
<footer class="foot">ESP32-S3 × RC522 · 本地连接，离线操作</footer>
</main>
<dialog id="dialog"><h2 id="dialogTitle"></h2><p id="dialogText"></p><div class="confirm-actions"><button id="dialogCancel" class="ghost" type="button">取消</button><button id="dialogOk" class="solid" type="button">确认</button></div></dialog>
<script>
'use strict';
const $=id=>document.getElementById(id);
let state=null,online=false,sending=false,receivedAt=0,pollTimer,pollVersion=0;
const phases={idle:'准备就绪',queued:'操作已接收',working:'正在处理',waiting:'等待放卡',reading:'正在备份',checking:'正在检查',confirm:'等待写入确认',writing:'正在恢复',verifying:'正在校验',success:'操作完成',error:'操作未完成',expired:'确认已过期'};
function showError(message){$('toast').textContent=message;$('toast').hidden=false;}
function remaining(){return state?Math.max(0,state.remainingMs-(Date.now()-receivedAt)):0;}
function render(){
 $('connection').className='badge'+(online?'':' off');$('connectionText').textContent=online?'设备已连接':'连接已断开';$('offlineTip').hidden=online;
 const blocked=!online||sending||!state||state.busy;
 for(const id of ['info','backup','restore','verify'])$(id).disabled=blocked||!state?.readerReady||state?.armed||(id!=='info'&&!state?.storageReady)||(id==='restore'||id==='verify')&&!state?.hasBackup;
 $('download').disabled=blocked||!state?.hasBackup;$('erase').disabled=blocked||!state?.hasBackup;
 $('prepareBlock0').disabled=blocked||!state?.readerReady||!state?.storageReady||!state?.hasBackup||!state?.block0Eligible||state?.armed;
 $('block0Method').disabled=$('prepareBlock0').disabled;
 const isCuid=state?.confirmationKind==='cuid',isBlock0=state?.confirmationKind==='block0'||isCuid,knownConfirmation=isBlock0||state?.confirmationKind==='data';
 $('confirmWrite').disabled=blocked||!state?.armed||!knownConfirmation||remaining()<=0;$('cancelWrite').disabled=blocked||!state?.armed;
 if(!state)return;
 $('statusTitle').textContent=phases[state.phase]||'准备就绪';$('message').textContent=state.message;
 $('statusPanel').className='card status '+state.phase;$('statusIcon').textContent=state.phase==='success'?'✓':state.phase==='error'?'!':state.busy?'↻':'·';
 $('detail').textContent=state.detail;$('detail').hidden=!state.detail;
 $('diagnostics').textContent=state.diagnostics||'';$('diagnosticsPanel').hidden=!state.diagnostics;
 $('counts').textContent=state.total?state.progress+' / '+state.total:'';$('track').hidden=!state.total;$('bar').style.width=(state.total?Math.min(100,100*state.progress/state.total):0)+'%';
 $('readerState').classList.toggle('ready',state.readerReady);$('readerState').lastElementChild.textContent=state.readerReady?'读卡器已就绪':'读卡器未就绪';
 $('storageState').classList.toggle('ready',state.storageReady);$('storageState').lastElementChild.textContent=state.storageReady?'板上存储正常':'板上存储异常';
 $('sourceUid').textContent=state.hasBackup?state.sourceUid:'暂无备份';$('sourceMeta').textContent=state.hasBackup?'47 块 · CRC '+state.crc:'备份原卡后，断电仍可保留';
 $('cardUid').textContent=state.cardUid||'等待放卡';$('cardType').textContent=state.cardType||'点击「识别卡片」重新读取卡号';
 $('confirmPanel').hidden=!state.armed;$('targetUid').textContent=state.targetUid;
 $('confirmTitle').textContent=isCuid?'核对 CUID 标准写入':isBlock0?'核对第 0 块写入':'核对目标卡';
 $('confirmIntro').textContent=isBlock0?'目标卡当前卡号：':'将备份数据写入以下卡片：';
 $('replacementRow').hidden=!isBlock0;$('replacementUid').textContent=isBlock0?state.replacementUid:'';
 $('confirmWarning').textContent=isCuid?'当前仅完成普通认证和读取，尚未验证第 0 块可写。确认后将真实尝试写入全部 16 字节；普通固定 UID 卡可能拒绝，一次性 / OTP 卡可能写后锁定。请先恢复 47 块，只保留这张目标卡。':isBlock0?'Gen1A 读取验证已通过。会覆盖第 0 块全部 16 字节，包括卡号与厂家信息。请先恢复 47 个用户数据块，并只保留这张目标卡，写入期间保持不动。':'会覆盖目标卡的 47 个用户数据块。请只保留这张目标卡，并在写入完成前保持不动。';
 $('block0Bytes').hidden=!isBlock0;$('block0Bytes').textContent=isBlock0?'待写入的 16 字节：'+state.sourceBlock0:'';
 $('confirmWrite').textContent=isCuid?'确认标准写入第 0 块':isBlock0?'确认写入第 0 块':'确认写入这张卡';
 $('block0Hint').hidden=!!state.block0Eligible;
 $('ssid').textContent=state.ssid;$('networkName').textContent=state.ssid;
 $('countdown').textContent=remaining()>0?Math.ceil(remaining()/1000)+' 秒内确认':'已过期，请重新检查';
}
async function request(url,options={}){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),6500);
 try{return await fetch(url,{cache:'no-store',...options,signal:controller.signal});}finally{clearTimeout(timer);}
}
async function poll(){
 clearTimeout(pollTimer);const version=++pollVersion;
 try{const response=await request('/api/state');if(!response.ok)throw new Error();const next=await response.json();if(version!==pollVersion)return;state=next;receivedAt=Date.now();online=true;}
 catch(error){if(version!==pollVersion)return;online=false;}
 render();pollTimer=setTimeout(poll,online?700:2000);
}
async function action(name,extra={}){
 if(!online||sending||!state||state.busy)return;
 sending=true;$('toast').hidden=true;render();
 try{
  const response=await request('/api/action',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-ICCard-Token':state.token},body:new URLSearchParams({action:name,...extra}).toString()});
  const result=await response.json();if(!response.ok)throw new Error(result.error||'操作未被接受。');
 }catch(error){showError(error.name==='AbortError'?'请求超时。设备可能已经开始操作，请等待状态更新；不要重复确认写入。':error.message||'连接中断，请等待状态更新。');}
 finally{await poll();sending=false;render();}
}
function confirmDialog(title,message,label,danger=false){
 return new Promise(resolve=>{
  $('dialogTitle').textContent=title;$('dialogText').textContent=message;$('dialogOk').textContent=label;$('dialogOk').className='solid'+(danger?' danger':'');
  const close=value=>{$('dialog').close();resolve(value);};
  $('dialogOk').onclick=()=>close(true);$('dialogCancel').onclick=()=>close(false);$('dialog').oncancel=event=>{event.preventDefault();close(false);};$('dialog').showModal();
 });
}
$('info').onclick=()=>action('info');
$('backup').onclick=async()=>{if(!state?.hasBackup||await confirmDialog('替换板上备份？','当前备份的原卡号为 '+state.sourceUid+'。\n新的原卡全部读取成功后，会替换这份备份。','开始备份'))action('backup');};
$('restore').onclick=()=>action('restore');$('verify').onclick=()=>action('verify');
$('prepareBlock0').onclick=()=>{if(!state?.block0Eligible||state?.armed)return;const method=$('block0Method').value||'auto';const name=method==='auto'?'prepare-auto':method==='gen1a'?'prepare-block0':method==='cuid'?'prepare-cuid':'';if(name)return action(name);showError('请选择有效的写入方式。');};
$('cancelWrite').onclick=()=>action('cancel');
$('confirmWrite').onclick=()=>{if(!state?.armed||remaining()<=0)return;const name=state.confirmationKind==='block0'?'write-block0':state.confirmationKind==='cuid'?'write-cuid':state.confirmationKind==='data'?'write':'';if(name)return action(name,{uid:state.targetUid,confirmation:state.confirmation});};
$('erase').onclick=async()=>{if(await confirmDialog('删除板上备份？','这会删除设备上保存的数据。已下载的文件不受影响；本版本不支持从文件重新导入。','确认删除',true))action('erase-backup',{confirmation:'ERASE'});};
$('download').onclick=async()=>{
 if(!online||sending||!state?.hasBackup||state.busy)return;
 sending=true;render();
 try{
  const response=await request('/api/backup',{headers:{'X-ICCard-Token':state.token}});
  if(!response.ok){const result=await response.json();throw new Error(result.error);}
  const blob=await response.blob(),url=URL.createObjectURL(blob),link=document.createElement('a');
  link.href=url;link.download='iccard-'+state.sourceUid+'.json';document.body.appendChild(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);
 }catch(error){showError(error.message||'下载失败，请检查连接。');}finally{sending=false;render();}
};
setInterval(()=>{if(state?.armed)render();},250);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)poll();});
poll();
</script>
</body>
</html>
)ICWEB";
