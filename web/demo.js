'use strict';
(async () => {
  const canvas=document.getElementById('canvas');
  const panel=document.getElementById('controls');
  const status=document.getElementById('status');
  const settings=document.getElementById('settings');
  const options=window.fishDemoOptions||{};
  const root=options.rootId?document.getElementById(options.rootId):null;
  const size=()=>({width:root?root.clientWidth:innerWidth,height:root?root.clientHeight:innerHeight});
  if(size().width<=600) document.getElementById('settings-panel').open=false;
  const reducedMotion=matchMedia('(prefers-reduced-motion: reduce)');
  let paused=reducedMotion.matches;
  let module;
  try {
    if(typeof createFishSchool!=='function') throw new Error('Build the WASM files with make wasm.');
    module=await createFishSchool({canvas,...(options.wasmBinary?{wasmBinary:options.wasmBinary}:{}),printErr:message=>console.error(message)});
    if(!module._get_fish_count()) throw new Error('WebGL 2 is required for this demo.');
  } catch(error) {
    status.textContent='Cannot start the demo. '+error.message;
    return;
  }
  const count=document.getElementById('count');
  if(size().width<=720) count.value='16';
  function obstacles() {
    const r=panel.getBoundingClientRect(), c=canvas.getBoundingClientRect();
    module._clear_obstacles();
    module._set_obstacle(0,r.x-c.x,r.y-c.y,r.width,r.height,12);
  }
  function resize() {
    const {width,height}=size();
    module._resize_canvas(width,height,Math.min(devicePixelRatio||1,2));
    obstacles();
  }
  function pause() {
    module._set_paused(paused||document.hidden?1:0);
    document.getElementById('pause').textContent=paused?'Play':'Pause';
    document.getElementById('pause').setAttribute('aria-pressed',String(paused));
  }
  for(const [id,method] of [['count','_set_count'],['speed','_set_speed'],['scale','_set_scale']]) {
    const input=document.getElementById(id),output=document.getElementById(id+'-value');
    const update=()=>{const value=Number(input.value);module[method](value);output.value=id==='count'?String(value):value.toFixed(1);};
    input.addEventListener('input',update);update();
  }
  for(const [id,method] of [['debug','_set_debug'],['depth','_set_depth'],['follow','_set_follow']]) {
    const input=document.getElementById(id);
    input.addEventListener('change',()=>module[method](input.checked?1:0));
  }
  document.getElementById('pause').addEventListener('click',()=>{paused=!paused;pause();});
  document.getElementById('reset').addEventListener('click',()=>module._reset_school(crypto.getRandomValues(new Uint32Array(1))[0]));
  addEventListener('pointermove',event=>{if(!panel.contains(event.target)){const r=canvas.getBoundingClientRect();module._set_pointer(event.clientX-r.x,event.clientY-r.y);}},{passive:true});
  addEventListener('resize',resize);
  document.addEventListener('visibilitychange',pause);
  reducedMotion.addEventListener('change',event=>{if(event.matches){paused=true;pause();}});
  new ResizeObserver(obstacles).observe(panel);
  if(root) new ResizeObserver(resize).observe(root);
  settings.disabled=false;resize();pause();
  setInterval(()=>{status.textContent=`${module._get_fish_count()} fish / 12 spine points / ${module._get_frame_ms().toFixed(1)} ms CPU per frame${paused?' / paused':''}`;},500);
  // Supported access for embedding experiments and browser verification.
  window.fishDemo=Object.freeze({module,canvas});
})();
