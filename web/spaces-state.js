// Keep the Page's controls available for later review. The WASM engine is unchanged.
(() => {
  let applying=false,timer,attempts=0;
  const ready=setInterval(()=>{
    if(!window.fishDemo){if(++attempts>400)clearInterval(ready);return;}
    clearInterval(ready);
    function apply(state){
      clearTimeout(timer);
      const config=state?.modelContent;
      if(!config||typeof config!=='object')return;
      applying=true;
      try {
        for(const id of ['count','speed','scale']){
          const el=document.getElementById(id),value=config[id];
          if(typeof value==='number'&&Number.isFinite(value)&&value>=Number(el.min)&&value<=Number(el.max)){
            el.value=String(value);el.dispatchEvent(new Event('input',{bubbles:true}));
          }
        }
        for(const id of ['debug','depth','follow']){
          const el=document.getElementById(id);
          if(typeof config[id]==='boolean'&&el.checked!==config[id])el.click();
        }
        const pause=document.getElementById('pause');
        if(typeof config.paused==='boolean'&&(pause.getAttribute('aria-pressed')==='true')!==config.paused)pause.click();
      } finally {applying=false;}
    }
    function save(){
      if(applying||!window.openai?.setWidgetState)return;
      clearTimeout(timer);
      timer=setTimeout(()=>{
        const config={};
        for(const id of ['count','speed','scale'])config[id]=Number(document.getElementById(id).value);
        for(const id of ['debug','depth','follow'])config[id]=document.getElementById(id).checked;
        config.paused=document.getElementById('pause').getAttribute('aria-pressed')==='true';
        Promise.resolve(window.openai.setWidgetState({modelContent:config,privateContent:null})).catch(()=>{});
      },180);
    }
    apply(window.openai?.widgetState);
    for(const id of ['count','speed','scale'])document.getElementById(id).addEventListener('input',save);
    for(const id of ['debug','depth','follow'])document.getElementById(id).addEventListener('change',save);
    document.getElementById('pause').addEventListener('click',save);
    window.addEventListener('openai:set_globals',event=>{
      const globals=event.detail?.globals;
      if(globals&&Object.hasOwn(globals,'widgetState'))apply(globals.widgetState);
    });
  },50);
})();
