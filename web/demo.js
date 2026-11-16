'use strict';
(async () => {
  const canvas=document.getElementById('canvas');
  const panel=document.getElementById('controls');
  const status=document.getElementById('status');
  const settings=document.getElementById('settings');
  if(innerWidth<=600) document.getElementById('settings-panel').open=false;
  const reducedMotion=matchMedia('(prefers-reduced-motion: reduce)');
  let paused=reducedMotion.matches;
  let module;
  try {
    if(typeof createFishSchool!=='function') throw new Error('Build the WASM files with make wasm.');
    module=await createFishSchool({canvas,printErr:message=>console.error(message)});
    if(!module._get_fish_count()) throw new Error('WebGL 2 is required for this demo.');
  } catch(error) {
    status.textContent='Cannot start the demo. '+error.message;
    return;
  }
  const count=document.getElementById('count');
  if(innerWidth<=720) count.value='16';
  function obstacles() {
    const r=panel.getBoundingClientRect();
    module._clear_obstacles();
    module._set_obstacle(0,r.x,r.y,r.width,r.height,12);
  }
  function resize() {
    module._resize_canvas(innerWidth,innerHeight,Math.min(devicePixelRatio||1,2));
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
  addEventListener('pointermove',event=>{if(!panel.contains(event.target))module._set_pointer(event.clientX,event.clientY);},{passive:true});
  addEventListener('resize',resize);
  document.addEventListener('visibilitychange',pause);
  reducedMotion.addEventListener('change',event=>{if(event.matches){paused=true;pause();}});
  new ResizeObserver(obstacles).observe(panel);
  settings.disabled=false;resize();pause();
  setInterval(()=>{status.textContent=`${module._get_fish_count()} fish / 12 spine points / ${module._get_frame_ms().toFixed(1)} ms CPU per frame${paused?' / paused':''}`;},500);
  // Supported access for embedding experiments and browser verification.
  window.fishDemo=Object.freeze({module,canvas});
})();
