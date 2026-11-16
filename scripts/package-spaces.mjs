// Bundle the compiled C/WASM demo as a self-contained Page embed.
import {readFile,writeFile,mkdir} from 'node:fs/promises';
const read=path=>readFile(new URL('../'+path,import.meta.url),'utf8');
const [html,css,runtime,demo,state,license,notices]=await Promise.all([
  read('web/index.html'),read('web/demo.css'),read('web/fish.js'),read('web/demo.js'),
  read('web/spaces-state.js'),read('LICENSE'),read('THIRD_PARTY_NOTICES.md')
]);
const wasm=await readFile(new URL('../web/fish.wasm',import.meta.url));
const body=html.match(/<body>([\s\S]*?)<\/body>/)?.[1];
if(!body)throw new Error('Cannot locate the demo body');
const script=source=>'<script>\n'+source.replace(/<\/script/gi,'<\\/script')+'\n</script>';
const fragment=`<div id="wasm-fish-spaces">
<style>
${css}
#wasm-fish-spaces { position:relative; display:flow-root; min-height:760px; overflow:hidden; background:var(--paper); color:var(--ink); }
#wasm-fish-spaces canvas { position:absolute; }
#wasm-fish-spaces .controls { min-width:0; }
</style>
${body}
</div>
${script(runtime)}
${script(`window.fishDemoOptions={rootId:'wasm-fish-spaces',wasmBinary:Uint8Array.from(atob('${wasm.toString('base64')}'),c=>c.charCodeAt(0))};`)}
${script(demo)}
${script(state)}
<!--
${license}
${notices}
-->
`;
const bytes=Buffer.byteLength(fragment);
if(bytes>256*1024)throw new Error(`Page embed is too large: ${bytes} bytes`);
await mkdir(new URL('../build/',import.meta.url),{recursive:true});
await writeFile(new URL('../build/wasm-fish-spaces.html',import.meta.url),fragment);
console.log(JSON.stringify({path:'build/wasm-fish-spaces.html',bytes,wasmBytes:wasm.length}));
