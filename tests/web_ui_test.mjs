// Executes the embedded page script against a small fake DOM and HTTP API.
// This tests client behavior, not browser layout or Wi-Fi/radio communication.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';

const source = await readFile(new URL('../CardBackup/WebPage.h', import.meta.url), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
const nodes = new Map();
let download, now = 1000, interval;
function node(id) {
  if (!nodes.has(id)) nodes.set(id, {
    id, textContent:'', hidden:false, disabled:false, style:{}, className:'',
    set innerHTML(value){throw new Error('Page must render API values as text, not HTML: '+value);},
    classList:{toggle(){}}, lastElementChild:{textContent:''}, open:false,
    showModal(){this.open=true;},close(){this.open=false;},
    click(){download={filename:this.download,url:this.href};},remove(){}
  });
  return nodes.get(id);
}
for (const match of source.matchAll(/\bid="([^"]+)"/g)) node(match[1]);
let api = {busy:false,readerReady:true,storageReady:true,hasBackup:true,
  phase:'idle',message:'已就绪',detail:'',diagnostics:'',cardUid:'',cardType:'',sourceUid:'AB0A1EBB',
  crc:'1234ABCD',progress:0,total:0,armed:false,targetUid:'',confirmation:'',
  confirmationKind:'',block0Eligible:true,replacementUid:'AB0A1EBB',sourceBlock0:'AB0A1EBB0408040003C234747F4CA890',
  remainingMs:0,ssid:'ICCard-S3',token:'session-test',ip:'192.168.4.1'};
const posts=[];
let releasePost, holdPost=false, failState=false, targetSupportsGen1A=true;
const context=vm.createContext({
  document:{getElementById:node,createElement:()=>node('download-link'),body:{appendChild(){}},addEventListener(){}},
  Date:class extends Date{static now(){return now;}},
  setTimeout:()=>1,clearTimeout(){},setInterval:fn=>{interval=fn;},AbortController,URLSearchParams,
  URL:{createObjectURL:()=> 'blob:test',revokeObjectURL(){}},
  fetch:async(url,options={})=>{
    if(url==='/api/state'){
      if(failState)throw new Error('offline');
      return {ok:true,json:async()=>structuredClone(api)};
    }
    assert.equal(options.headers['X-ICCard-Token'],'session-test');
    if(url==='/api/backup')return {ok:true,blob:async()=>new Blob(['{}'])};
    assert.equal(url,'/api/action');assert.equal(options.method,'POST');
    const body=Object.fromEntries(new URLSearchParams(options.body));posts.push(body);
    if(holdPost)await new Promise(resolve=>{releasePost=resolve;});
    if(body.action==='restore')Object.assign(api,{armed:true,confirmationKind:'data',targetUid:'11223344',confirmation:'4',remainingMs:30000,phase:'confirm'});
    if(body.action==='prepare-block0'||body.action==='prepare-auto')Object.assign(api,targetSupportsGen1A?
      {armed:true,confirmationKind:'block0',targetUid:'9EBA0902',cardUid:'9EBA0902',confirmation:'5',remainingMs:30000,phase:'confirm'}:
      body.action==='prepare-auto'?
      {armed:true,confirmationKind:'cuid',targetUid:'9EBA0902',cardUid:'9EBA0902',confirmation:'6',remainingMs:30000,phase:'confirm',message:'普通认证与读取通过，尚未验证可写。'}:
      {armed:false,confirmationKind:'',phase:'error',message:'未检测到 Gen1A 支持，没有写入。'});
    if(body.action==='prepare-cuid')Object.assign(api,{armed:true,confirmationKind:'cuid',targetUid:'9EBA0902',cardUid:'9EBA0902',confirmation:'6',remainingMs:30000,phase:'confirm',message:'普通认证与读取通过，尚未验证可写。'});
    if(body.action==='write'){
      assert.equal(api.confirmationKind,'data','data writes require a data confirmation');
      Object.assign(api,{armed:false,confirmationKind:'',busy:true,phase:'writing'});
    }
    if(body.action==='write-block0'){
      assert.equal(api.confirmationKind,'block0','block 0 writes require a dedicated confirmation');
      assert.equal(body.uid,'9EBA0902','submit the current UID, not the replacement UID');
      assert.equal(body.confirmation,'5');
      Object.assign(api,{armed:false,confirmationKind:'',phase:'success',cardUid:api.replacementUid,
        message:'第 0 块写入成功，已重新识别并校验。'});
    }
    if(body.action==='write-cuid'){
      assert.equal(api.confirmationKind,'cuid');
      assert.equal(body.uid,'9EBA0902');
      assert.equal(body.confirmation,'6');
      Object.assign(api,{armed:false,confirmationKind:'',phase:'success',cardUid:api.replacementUid,
        message:'标准写入完成，新UID和完整第0块已验证。'});
    }
    if(body.action==='cancel')Object.assign(api,{armed:false,confirmationKind:'',phase:'idle'});
    if(body.action==='erase-backup')Object.assign(api,{hasBackup:false,block0Eligible:false});
    return {ok:true,json:async()=>({accepted:true})};
  }
});
const settle=()=>new Promise(resolve=>setImmediate(resolve));
vm.runInContext(script,context);
await settle();
assert.equal(node('connectionText').textContent,'设备已连接');
assert.equal(node('info').disabled,false);
assert.equal(node('sourceUid').textContent,'AB0A1EBB');
assert.equal(node('prepareBlock0').disabled,false);
assert.equal(node('diagnosticsPanel').hidden,true);

api.diagnostics='AUTH: OK\nREAD: failed <img src=x onerror=alert(1)> & <script>test</script>';
await vm.runInContext('poll()',context);
assert.equal(node('diagnosticsPanel').hidden,false);
assert.equal(node('diagnostics').textContent,api.diagnostics,'diagnostics remain literal selectable text');
api.diagnostics='';
await vm.runInContext('poll()',context);
assert.equal(node('diagnosticsPanel').hidden,true,'clear the previous operation diagnostics when cleared by device');

holdPost=true;
const first=node('info').onclick();await settle();
assert.equal(node('info').disabled,true);
node('info').onclick();await settle();
assert.equal(posts.length,1,'double click must not send another action');
holdPost=false;releasePost();await first;

await node('restore').onclick();
assert.equal(node('confirmPanel').hidden,false);
assert.equal(node('targetUid').textContent,'11223344');
assert.equal(node('backup').disabled,true,'other card actions disabled while confirming');
assert.equal(node('prepareBlock0').disabled,true);
assert.equal(node('confirmWrite').disabled,false);
assert.equal(node('confirmWrite').textContent,'确认写入这张卡');
assert.equal(node('replacementRow').hidden,true);
node('confirmWrite').onclick();await settle();
assert.deepEqual(posts.at(-1),{action:'write',uid:'11223344',confirmation:'4'});
assert.equal(node('confirmPanel').hidden,true);
assert.equal(node('info').disabled,true,'cannot issue card commands during writes');

Object.assign(api,{busy:false,phase:'success'});
await vm.runInContext('poll()',context);
await node('restore').onclick();
now+=31000;interval();
assert.equal(node('confirmWrite').disabled,true);
const count=posts.length;
node('confirmWrite').onclick();await settle();
assert.equal(posts.length,count,'expired button does not submit');
await node('cancelWrite').onclick();
assert.equal(posts.at(-1).action,'cancel');

// The default automatic option only prepares a confirmation, never a write.
const writesBeforeCheck=posts.filter(post=>post.action.startsWith('write')).length;
await node('prepareBlock0').onclick();
assert.deepEqual(posts.at(-1),{action:'prepare-auto'});
assert.equal(posts.filter(post=>post.action.startsWith('write')).length,writesBeforeCheck);
assert.equal(node('confirmPanel').hidden,false);
assert.equal(node('confirmTitle').textContent,'核对第 0 块写入');
assert.equal(node('confirmWrite').textContent,'确认写入第 0 块');
assert.equal(node('targetUid').textContent,'9EBA0902');
assert.equal(node('replacementRow').hidden,false);
assert.equal(node('replacementUid').textContent,'AB0A1EBB');
assert.match(node('block0Bytes').textContent,/AB0A1EBB0408040003C234747F4CA890/);
assert.match(node('confirmWarning').textContent,/16 字节/);
assert.match(node('confirmWarning').textContent,/厂家信息/);
assert.equal(node('restore').disabled,true);
await node('confirmWrite').onclick();
assert.deepEqual(posts.at(-1),{action:'write-block0',uid:'9EBA0902',confirmation:'5'});
assert.equal(node('confirmPanel').hidden,true);
assert.equal(node('cardUid').textContent,'AB0A1EBB','show the newly read UID after writing');
assert.equal(node('sourceUid').textContent,'AB0A1EBB','saved backup remains separate from live card');

await node('restore').onclick();
assert.equal(node('confirmWrite').textContent,'确认写入这张卡','do not reuse the block 0 button label');
assert.equal(node('replacementRow').hidden,true);
assert.equal(node('block0Bytes').hidden,true);
await node('cancelWrite').onclick();

targetSupportsGen1A=false;
node('block0Method').value='gen1a';
await node('prepareBlock0').onclick();
assert.deepEqual(posts.at(-1),{action:'prepare-block0'});
assert.equal(node('confirmPanel').hidden,true,'unsupported cards never show a write confirmation');
assert.equal(node('confirmWrite').disabled,true);
assert.match(node('message').textContent,/未检测到 Gen1A/);
const postsAfterFailure=posts.length;
await node('confirmWrite').onclick();
assert.equal(posts.length,postsAfterFailure,'failed checks cannot be confirmed');

// Fallback to standard preparation must make writability uncertainty explicit.
node('block0Method').value='auto';
const writesBeforeStandard=posts.filter(post=>post.action.startsWith('write')).length;
await node('prepareBlock0').onclick();
assert.deepEqual(posts.at(-1),{action:'prepare-auto'});
assert.equal(posts.filter(post=>post.action.startsWith('write')).length,writesBeforeStandard);
assert.equal(node('confirmTitle').textContent,'核对 CUID 标准写入');
assert.equal(node('confirmWrite').textContent,'确认标准写入第 0 块');
assert.match(node('confirmWarning').textContent,/尚未验证/);
assert.match(node('confirmWarning').textContent,/OTP/);
assert.equal(node('block0Method').disabled,true);
// Changing the local selector cannot change the mode bound by the device.
node('block0Method').value='gen1a';
await node('confirmWrite').onclick();
assert.deepEqual(posts.at(-1),{action:'write-cuid',uid:'9EBA0902',confirmation:'6'});
assert.equal(node('cardUid').textContent,'AB0A1EBB');

node('block0Method').value='cuid';
await node('prepareBlock0').onclick();
assert.deepEqual(posts.at(-1),{action:'prepare-cuid'});
now+=31000;interval();
const postsAfterCuidExpiry=posts.length;
await node('confirmWrite').onclick();
assert.equal(posts.length,postsAfterCuidExpiry);
await node('cancelWrite').onclick();

node('block0Method').value='unknown';
const postsBeforeBadMethod=posts.length;
await node('prepareBlock0').onclick();
assert.equal(posts.length,postsBeforeBadMethod);
node('block0Method').value='gen1a';

targetSupportsGen1A=true;
await node('prepareBlock0').onclick();
now+=31000;interval();
assert.equal(node('confirmWrite').disabled,true);
const postsAfterExpiry=posts.length;
await node('confirmWrite').onclick();
assert.equal(posts.length,postsAfterExpiry,'block 0 confirmations also expire');
await node('cancelWrite').onclick();

Object.assign(api,{armed:true,confirmationKind:'unknown',remainingMs:30000});
await vm.runInContext('poll()',context);
assert.equal(node('confirmWrite').disabled,true);
const postsBeforeUnknown=posts.length;
await node('confirmWrite').onclick();
assert.equal(posts.length,postsBeforeUnknown,'unknown confirmation kinds must never become data writes');
await node('cancelWrite').onclick();

api.block0Eligible=false;
await vm.runInContext('poll()',context);
assert.equal(node('prepareBlock0').disabled,true);
assert.equal(node('block0Hint').hidden,false);
const postsBeforeIneligible=posts.length;
await node('prepareBlock0').onclick();
assert.equal(posts.length,postsBeforeIneligible);
api.block0Eligible=true;
await vm.runInContext('poll()',context);

let pending=node('backup').onclick();await settle();
assert.equal(node('dialog').open,true);
node('dialogCancel').onclick();await pending;
assert.equal(posts.at(-1).action,'cancel','rejecting overwrite leaves backup alone');
pending=node('backup').onclick();await settle();
node('dialogOk').onclick();await pending;await settle();
assert.equal(posts.at(-1).action,'backup');

await node('download').onclick();
assert.equal(download.filename,'iccard-AB0A1EBB.json');
failState=true;await vm.runInContext('poll()',context);
assert.equal(node('offlineTip').hidden,false);
assert.equal(node('info').disabled,true);
assert.equal(node('download').disabled,true);
assert.equal(node('prepareBlock0').disabled,true);
failState=false;await vm.runInContext('poll()',context);
assert.equal(node('info').disabled,false);
pending=node('erase').onclick();await settle();node('dialogOk').onclick();await pending;await settle();
assert.deepEqual(posts.at(-1),{action:'erase-backup',confirmation:'ERASE'});
assert.equal(node('restore').disabled,true);
assert.equal(node('download').disabled,true);
assert.equal(node('prepareBlock0').disabled,true);
console.log('PASS: page script, diagnostics, automatic/Gen1A/CUID preparation, explicit unverified-write warning, three confirmation modes, UID read-back, expiry, duplicate exclusion, download and reconnect');
