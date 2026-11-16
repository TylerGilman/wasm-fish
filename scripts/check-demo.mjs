// Observable browser checks through Chrome CDP. Node.js 22+; no dependencies.
import { spawn } from 'node:child_process';
import { mkdtemp, mkdir, writeFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import assert from 'node:assert/strict';

const [url='http://127.0.0.1:8090/',widthArg='1440',heightArg='900',motion='no-preference',frameMode='page']=process.argv.slice(2);
assert(['localhost','127.0.0.1','[::1]'].includes(new URL(url).hostname),'Use a local preview URL');
const width=Number(widthArg),height=Number(heightArg);
const profile=await mkdtemp(join(tmpdir(),'wasm-fish-check-'));
const chrome=spawn(process.env.CHROME||'google-chrome',[
  '--headless=new','--remote-debugging-port=0',`--user-data-dir=${profile}`,
  '--no-first-run','--no-default-browser-check','about:blank'
],{stdio:['ignore','ignore','pipe']});
process.on('exit',()=>chrome.kill());
let socket,endpoint,target;
try {
  endpoint=await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(new Error('Chrome startup timeout')),15000);
    chrome.stderr.on('data',data=>{
      const match=String(data).match(/DevTools listening on ws:\/\/(127\.0\.0\.1:\d+)/);
      if(match){clearTimeout(timer);resolve('http://'+match[1]);}
    });
    chrome.on('error',reject);
  });
  target=await(await fetch(`${endpoint}/json/new?about:blank`,{method:'PUT'})).json();
  socket=new WebSocket(target.webSocketDebuggerUrl);
  await new Promise(resolve=>socket.addEventListener('open',resolve,{once:true}));
  let id=0,evaluationContext,frameSession;const pending=new Map(),errors=[],contexts=new Map();
  socket.addEventListener('message',event=>{
    const message=JSON.parse(event.data);
    if(message.method==='Runtime.executionContextCreated')contexts.set((message.sessionId||'')+':'+message.params.context.id,{...message.params.context,sessionId:message.sessionId});
    if(message.method==='Runtime.executionContextDestroyed')contexts.delete((message.sessionId||'')+':'+message.params.executionContextId);
    if(message.method==='Runtime.exceptionThrown') errors.push(message.params.exceptionDetails);
    if(message.method==='Log.entryAdded'&&message.params.entry.level==='error')errors.push(message.params.entry);
    const request=pending.get(message.id);if(!request)return;
    pending.delete(message.id);clearTimeout(request.timer);
    message.error?request.reject(new Error(JSON.stringify(message.error))):request.resolve(message.result);
  });
  function send(method,params={},sessionId){
    return new Promise((resolve,reject)=>{
      const requestId=++id;
      const timer=setTimeout(()=>{pending.delete(requestId);reject(new Error('CDP timeout: '+method));},30000);
      pending.set(requestId,{resolve,reject,timer});socket.send(JSON.stringify({id:requestId,method,params,...(sessionId?{sessionId}:{})}));
    });
  }
  async function evaluate(expression){
    const r=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true,...(evaluationContext?{contextId:evaluationContext}:{})},frameSession);
    if(r.exceptionDetails)throw new Error(JSON.stringify(r.exceptionDetails));return r.result.value;
  }
  const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
  async function capture(suffix){
    await mkdir('build',{recursive:true});
    const r=await send('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});
    await writeFile(`build/demo-${width}-${suffix}.png`,Buffer.from(r.data,'base64'));
  }
  await send('Page.enable');await send('Runtime.enable');await send('Log.enable');
  await send('Emulation.setDeviceMetricsOverride',{width,height,deviceScaleFactor:1,mobile:width<=720});
  await send('Emulation.setEmulatedMedia',{features:[{name:'prefers-reduced-motion',value:motion}]});
  const navigation=await send('Page.navigate',{url});
  if(navigation.errorText)throw new Error('Cannot open preview: '+navigation.errorText);
  if(frameMode==='iframe'){
    await sleep(1000);
    for(let n=0;n<150&&!evaluationContext;n++){
      const tree=(await send('Page.getFrameTree')).frameTree;
      const childFrame=tree.childFrames?.[0]?.frame;
      const child=childFrame&&childFrame.url!=='about:blank'?childFrame.id:null;
      if(!frameSession){
        const targets=(await send('Target.getTargets')).targetInfos;
        const iframe=targets.find(t=>t.type==='iframe');
        if(iframe){
          frameSession=(await send('Target.attachToTarget',{targetId:iframe.targetId,flatten:true})).sessionId;
          await send('Runtime.enable',{},frameSession);await send('Log.enable',{},frameSession);
        }
      }
      const context=[...contexts.values()].find(c=>c.auxData?.isDefault&&(frameSession?c.sessionId===frameSession:c.auxData.frameId===child));
      if(context)evaluationContext=context.id;else await sleep(100);
    }
    if(!evaluationContext){console.log(JSON.stringify({frameTree:await send('Page.getFrameTree'),contexts:[...contexts.values()]}));throw new Error('Cannot find the sandbox frame context');}
  }
  await evaluate(`new Promise((resolve,reject)=>{let n=0;const t=setInterval(()=>{if(window.fishDemo){clearInterval(t);resolve();}else if(n++>200){clearInterval(t);reject(new Error(document.getElementById('status').textContent));}},50);})`);
  await sleep(2500);
  const initial=await evaluate(`({width:document.getElementById('canvas').getBoundingClientRect().width,count:fishDemo.module._get_fish_count(),time:fishDemo.module._get_simulation_time(),cpuMs:fishDemo.module._get_frame_ms(),overflow:document.documentElement.scrollWidth>innerWidth,status:document.getElementById('status').textContent})`);
  assert.equal(initial.count,initial.width<=720?16:40);assert.equal(initial.overflow,false);
  if(motion==='reduce')assert.equal(initial.time,0);else assert(initial.time>1);
  await capture(motion==='reduce'?'reduced':'normal');
  await evaluate(`document.getElementById('settings-panel').open=true`);
  await evaluate(`(()=>{const el=document.getElementById('count');el.value='20';el.dispatchEvent(new Event('input',{bubbles:true}));})()`);
  assert.equal(await evaluate('fishDemo.module._get_fish_count()'),20);
  // Begin this part running, regardless of the motion preference.
  if(motion==='reduce')await evaluate(`document.getElementById('pause').click()`);
  await evaluate(`document.getElementById('pause').click()`);
  await sleep(50);const stopped=await evaluate('fishDemo.module._get_simulation_time()');
  await sleep(300);assert.equal(await evaluate('fishDemo.module._get_simulation_time()'),stopped);
  await evaluate(`document.getElementById('pause').click()`);await sleep(400);
  assert(await evaluate('fishDemo.module._get_simulation_time()')>stopped);
  await evaluate(`document.getElementById('debug').click();document.getElementById('follow').click();document.getElementById('depth').click();`);
  await send('Input.dispatchMouseEvent',{type:'mouseMoved',x:width-80,y:height-80});
  await sleep(500);await capture('spines');
  await evaluate(`(()=>{const el=document.getElementById('count');el.value='128';el.dispatchEvent(new Event('input',{bubbles:true}));})()`);
  assert.equal(await evaluate('fishDemo.module._get_fish_count()'),128);
  await sleep(1000);const stressCpuMs=await evaluate('fishDemo.module._get_frame_ms()');
  assert.equal(await evaluate('fishDemo.module._get_dropped_triangles()'),0);
  await evaluate(`document.getElementById('reset').click()`);
  assert(await evaluate('fishDemo.module._get_simulation_time()')<.2);
  await send('Emulation.setDeviceMetricsOverride',{width:width<=720?844:900,height:width<=720?390:700,deviceScaleFactor:1,mobile:width<=720});
  await sleep(200);
  assert.equal(await evaluate('document.documentElement.scrollWidth>innerWidth'),false);
  assert.deepEqual(errors,[]);
  console.log(JSON.stringify({passed:true,viewport:{width,height},motion,initial,stressCpuMs,checks:['WASM initialization','count control','pause and resume','spine view','depth toggle','pointer input','128 fish','reset','resize','console']},null,2));
} finally {
  socket?.close();
  if(endpoint&&target)await fetch(`${endpoint}/json/close/${target.id}`).catch(()=>{});
  const exited=new Promise(resolve=>chrome.once('exit',resolve));chrome.kill();
  await exited;await rm(profile,{recursive:true,force:true,maxRetries:10,retryDelay:200});
}
