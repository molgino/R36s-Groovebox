// Upload-Seite (wird vom Mini-Webserver ausgeliefert). Umwandlung nach WAV passiert im Browser.
extern const char WEB_PAGE[];
const char WEB_PAGE[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SYNTH CITY</title>
<style>
:root{--bg:#080612;--panel:#141026;--panel2:#20183a;--text:#d6eeff;--dim:#8a80b0;--acc:#ff2e88;--cy:#00e0ff;--ok:#00ffaa;--bad:#ff4060}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 ui-monospace,Menlo,Consolas,monospace}
main{max-width:880px;margin:0 auto;padding:20px 16px 60px}
h1{font-size:30px;margin:0 0 4px;letter-spacing:.18em;color:#fff;text-shadow:-2px 0 var(--acc),2px 0 var(--cy),0 0 18px var(--acc)}h1 b{color:#fff}body{background:linear-gradient(180deg,#080612 0%,#140a2a 60%,#2a0a3a 100%) fixed}
h2{font-size:14px;color:var(--acc);letter-spacing:.12em;margin:28px 0 10px;text-transform:uppercase}
.sub{color:var(--dim);margin:0 0 18px}
#drop{border:2px dashed #3a3a46;border-radius:14px;padding:34px 16px;text-align:center;background:var(--panel);cursor:pointer;transition:.15s}
#drop.over{border-color:var(--acc);background:#2a2018}
#drop strong{display:block;font-size:18px;margin-bottom:6px}
.row{display:flex;flex-wrap:wrap;gap:10px 18px;align-items:center;margin:14px 0}
select,input[type=text]{background:var(--panel2);color:var(--text);border:1px solid #3a3a46;border-radius:8px;padding:7px 10px;font:inherit}
label{color:var(--dim)}label input{accent-color:var(--acc)}
button{background:var(--panel2);color:var(--text);border:1px solid #3a3a46;border-radius:8px;padding:5px 10px;font:inherit;cursor:pointer}
button:hover{border-color:var(--acc)}button.del:hover{border-color:var(--bad);color:var(--bad)}
.list{background:var(--panel);border-radius:12px;overflow:hidden}
.item{display:flex;align-items:center;gap:10px;padding:8px 12px;border-top:1px solid #26262e}
.item:first-child{border-top:0}.item .n{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.item .s{color:var(--dim);font-size:13px;white-space:nowrap}
.folder{padding:7px 12px;background:#15151a;color:var(--acc);font-size:13px;letter-spacing:.08em}
.job{display:flex;gap:10px;padding:6px 12px;font-size:13px}.job .n{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.ok{color:var(--ok)}.bad{color:var(--bad)}.dim{color:var(--dim)}
#bar{height:4px;background:var(--panel2);border-radius:2px;overflow:hidden;margin-top:10px}#bar i{display:block;height:100%;width:0;background:var(--acc);transition:width .2s}
</style></head><body><main>
<h1>SYNTH <b>CITY</b></h1>
<p class="sub" id="info">verbinde …</p>
<p><a href="/screen" style="color:var(--cy);font-weight:700">▶ Bildschirm &amp; Fernbedienung</a> <span class="dim">– Live-Bild der Groovebox, bedienbar per Maus, Touch oder Tastatur</span></p>

<div id="drop"><strong>Samples hier hineinziehen</strong><span class="dim">oder klicken · WAV, MP3, FLAC, AIFF, OGG … werden automatisch umgewandelt</span>
<input type="file" id="pick" multiple accept="audio/*,.wav,.mp3,.flac,.aif,.aiff,.ogg,.m4a" hidden></div>
<div class="row">
 <span>Ordner <select id="folder"></select></span>
 <input type="text" id="newf" placeholder="neuer Ordner" size="14">
 <label><input type="checkbox" id="conv" checked> in Mono/44,1 kHz umwandeln</label>
 <label><input type="checkbox" id="norm"> normalisieren</label>
</div>
<div id="bar"><i></i></div>
<div id="jobs" class="list" style="margin-top:10px"></div>

<h2>Samples auf der Karte</h2>
<div id="samples" class="list"></div>

<div id="upd" style="display:none">
<h2>Groovebox aktualisieren</h2>
<div class="row"><span>Installiert: <b id="ver">?</b></span><button id="uup">Update-Datei (.tgz) wählen</button><input type="file" id="upick" accept=".tgz" hidden>
<span id="ust" class="dim">Datei groovebox-os-update.tgz aus dem Release „nightly“</span></div>
</div>

<h2>Logs</h2>
<div class="row"><select id="logsel"><option>groovebox.log</option><option>groovebox.old.log</option><option>boot.txt</option><option>update.log</option><option>pc-modus.log</option></select>
<button id="logload">anzeigen</button><label><input type="checkbox" id="logauto"> automatisch aktualisieren</label><a id="logdl" download><button>herunterladen</button></a></div>
<pre id="logview" style="background:var(--panel);border-radius:12px;padding:12px;max-height:420px;overflow:auto;font-size:12px;white-space:pre-wrap;display:none"></pre>

<h2>Projekte</h2>
<div class="row"><span>Platz <select id="pslot"></select></span><button id="pup">Projekt (.gbx) hochladen</button><input type="file" id="ppick" accept=".gbx" hidden>
<span class="dim">Laden am Gerät unter PRJ → SONG. Herunterladen = Sicherung.</span></div>
<div id="projects" class="list"></div>
</main>
<script>
const $=s=>document.querySelector(s);
let state={samples:[],folders:[],projects:[],free:0,max:128};
const fmt=b=>b>1e9?(b/1e9).toFixed(1)+' GB':b>1e6?(b/1e6).toFixed(1)+' MB':Math.ceil(b/1e3)+' kB';
const clean=s=>s.normalize('NFKD').replace(/[̀-ͯ]/g,'').replace(/[^A-Za-z0-9 _\-.()+#]/g,'_').replace(/^[.\s]+/,'').slice(0,40)||'sample';
async function refresh(){
 try{const r=await fetch('/api/list');state=await r.json();}catch(e){$('#info').textContent='keine Verbindung zum R36S';return;}
 if(state.os){$('#upd').style.display='';$('#ver').textContent=state.version||'?';}
 $('#info').textContent=`${state.samples.length} von ${state.max} Samples · ${fmt(state.free)} frei auf der Karte · max. 60 s pro Sample`;
 const sel=$('#folder'),cur=sel.value;sel.innerHTML='<option value="">(Hauptordner)</option>'+state.folders.map(f=>`<option>${f}</option>`).join('');
 if(cur&&state.folders.includes(cur))sel.value=cur;
 const by={};state.samples.forEach(s=>{const i=s.path.lastIndexOf('/');const f=i<0?'':s.path.slice(0,i);(by[f]=by[f]||[]).push(s)});
 let h='';Object.keys(by).sort().forEach(f=>{h+=`<div class="folder">${f||'HAUPTORDNER'}</div>`;
  by[f].sort((a,b)=>a.path.localeCompare(b.path)).forEach(s=>{const n=s.path.slice(f?f.length+1:0);
  h+=`<div class="item"><span class="n">${n}</span><span class="s">${fmt(s.size)}</span><button data-play="${encodeURIComponent(s.path)}">▶</button><button class="del" data-del="${encodeURIComponent(s.path)}">löschen</button></div>`})});
 $('#samples').innerHTML=h||'<div class="item dim">noch keine Samples</div>';
 const used=new Set(state.projects.map(p=>p.path)),ps=$('#pslot'),pc=ps.value;
 ps.innerHTML=Array.from({length:32},(_,i)=>{const n='song'+String(i+1).padStart(2,'0')+'.gbx';return `<option value="${n}">SONG ${String(i+1).padStart(2,'0')}${used.has(n)?' (belegt)':''}</option>`}).join('');
 if(pc)ps.value=pc;else{const f=[...Array(32).keys()].find(i=>!used.has('song'+String(i+1).padStart(2,'0')+'.gbx'));if(f!==undefined)ps.selectedIndex=f;}
 $('#projects').innerHTML=state.projects.sort((a,b)=>a.path.localeCompare(b.path)).map(p=>`<div class="item"><span class="n">${p.path.replace(/^song(\d+)\.gbx$/,'SONG $1')}</span><span class="s">${fmt(p.size)}</span><a href="/api/file?root=projects&path=${encodeURIComponent(p.path)}" download="${p.path}"><button>herunterladen</button></a></div>`).join('')||'<div class="item dim">keine Projekte</div>';
}
let audio=null;
document.addEventListener('click',async e=>{
 const p=e.target.dataset.play,d=e.target.dataset.del;
 if(p){if(audio)audio.pause();audio=new Audio('/api/file?root=samples&path='+p);audio.play();}
 if(d&&confirm('Sample '+decodeURIComponent(d)+' löschen?')){await fetch('/api/delete?root=samples&path='+d,{method:'POST'});refresh();}
});
function wav(buf){ // AudioBuffer -> 16-Bit-WAV
 const ch=buf.numberOfChannels,len=buf.length,rate=buf.sampleRate,out=new DataView(new ArrayBuffer(44+len*ch*2));
 const ws=(o,s)=>{for(let i=0;i<s.length;i++)out.setUint8(o+i,s.charCodeAt(i))};
 ws(0,'RIFF');out.setUint32(4,36+len*ch*2,true);ws(8,'WAVE');ws(12,'fmt ');out.setUint32(16,16,true);out.setUint16(20,1,true);
 out.setUint16(22,ch,true);out.setUint32(24,rate,true);out.setUint32(28,rate*ch*2,true);out.setUint16(32,ch*2,true);out.setUint16(34,16,true);
 ws(36,'data');out.setUint32(40,len*ch*2,true);
 const data=[];for(let c=0;c<ch;c++)data.push(buf.getChannelData(c));
 let peak=0;if($('#norm').checked)data.forEach(d=>{for(let i=0;i<len;i++)peak=Math.max(peak,Math.abs(d[i]))});
 const g=peak>0.0001?0.98/peak:1;let o=44;
 for(let i=0;i<len;i++)for(let c=0;c<ch;c++){let v=Math.max(-1,Math.min(1,data[c][i]*g));out.setInt16(o,v<0?v*32768:v*32767,true);o+=2}
 return new Blob([out],{type:'audio/wav'});
}
async function convert(file){
 const raw=await file.arrayBuffer();
 const ac=new (window.AudioContext||window.webkitAudioContext)();
 const dec=await ac.decodeAudioData(raw.slice(0));ac.close();
 const rate=44100,len=Math.min(Math.ceil(dec.duration*rate),rate*60);
 const off=new OfflineAudioContext(1,len,rate),src=off.createBufferSource();src.buffer=dec;src.connect(off.destination);src.start();
 return wav(await off.startRendering());
}
async function upload(files){
 const nf=clean($('#newf').value.trim()).replace(/\//g,'_'),folder=$('#newf').value.trim()?nf:$('#folder').value;
 files=[...files];let done=0;$('#bar i').style.width='0';
 for(const f of files){
  const row=document.createElement('div');row.className='job';row.innerHTML=`<span class="n">${f.name}</span><span class="st dim">wandle um …</span>`;$('#jobs').prepend(row);
  const st=row.querySelector('.st');
  try{
   const isWav=/\.wav$/i.test(f.name);let blob=f;
   if(!isWav||$('#conv').checked)blob=await convert(f);
   const name=clean(f.name.replace(/\.[^.]+$/,''))+'.wav',path=(folder?folder+'/':'')+name;
   st.textContent='lade hoch …';
   const r=await fetch('/api/upload?root=samples&path='+encodeURIComponent(path),{method:'PUT',body:blob});
   if(!r.ok)throw new Error(await r.text());
   st.textContent='✓ '+fmt(blob.size);st.className='st ok';
  }catch(e){st.textContent='✗ '+(e.message||e);st.className='st bad';}
  done++;$('#bar i').style.width=(done/files.length*100)+'%';
 }
 $('#newf').value='';refresh();
}
const drop=$('#drop');
drop.onclick=()=>$('#pick').click();$('#pick').onchange=e=>upload(e.target.files);
['dragenter','dragover'].forEach(t=>drop.addEventListener(t,e=>{e.preventDefault();drop.classList.add('over')}));
['dragleave','drop'].forEach(t=>drop.addEventListener(t,e=>{e.preventDefault();drop.classList.remove('over')}));
drop.addEventListener('drop',e=>upload(e.dataTransfer.files));
$('#pup').onclick=()=>$('#ppick').click();
$('#ppick').onchange=async e=>{const f=e.target.files[0];if(!f)return;const n=$('#pslot').value;
 if(state.projects.some(p=>p.path===n)&&!confirm(n+' ist belegt – überschreiben?'))return;
 const r=await fetch('/api/upload?root=projects&path='+encodeURIComponent(n),{method:'PUT',body:f});if(!r.ok)alert(await r.text());e.target.value='';refresh();};
$('#uup').onclick=()=>$('#upick').click();
$('#upick').onchange=async e=>{const f=e.target.files[0];if(!f)return;const st=$('#ust');st.className='';st.textContent='lade hoch …';
 try{const r=await fetch('/api/update',{method:'PUT',body:f});const t=await r.text();if(!r.ok)throw new Error(t);
  st.className='ok';st.textContent='✓ übertragen – Groovebox startet neu und installiert (ca. 15 s)';setTimeout(refresh,15000);}
 catch(err){st.className='bad';st.textContent='✗ '+(err.message||err);}e.target.value='';};
async function loadLog(){const n=$('#logsel').value,v=$('#logview');v.style.display='';$('#logdl').href='/api/log?name='+n;$('#logdl').download=n;
 try{const r=await fetch('/api/log?name='+encodeURIComponent(n));v.textContent=await r.text();v.scrollTop=v.scrollHeight;}catch(e){v.textContent='keine Verbindung';}}
$('#logload').onclick=loadLog;$('#logsel').onchange=loadLog;setInterval(()=>{if($('#logauto').checked)loadLog()},3000);
refresh();setInterval(refresh,8000);
</script></body></html>)HTML";

// Bildschirm-Spiegelung + Fernbedienung (http://<ip>/screen)
extern const char WEB_SCREEN[];
const char WEB_SCREEN[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>SYNTH CITY – Bildschirm</title>
<style>
:root{--bg:#080612;--panel:#141026;--panel2:#20183a;--text:#d6eeff;--dim:#8a80b0;--acc:#ff2e88;--cy:#00e0ff}
*{box-sizing:border-box;-webkit-user-select:none;user-select:none;touch-action:manipulation}
body{margin:0;background:linear-gradient(180deg,#080612,#140a2a 60%,#2a0a3a) fixed;color:var(--text);font:14px ui-monospace,Menlo,Consolas,monospace}
main{max-width:1100px;margin:0 auto;padding:14px 16px 40px}
header{display:flex;align-items:center;gap:14px;flex-wrap:wrap;margin-bottom:10px}
h1{font-size:22px;margin:0;letter-spacing:.18em;color:#fff;text-shadow:-2px 0 var(--acc),2px 0 var(--cy),0 0 14px var(--acc)}
.dim{color:var(--dim)}a{color:var(--cy)}
#wrap{position:relative;background:#000;border-radius:12px;padding:10px;box-shadow:0 0 30px #ff2e8833}
canvas{display:block;width:100%;max-width:1280px;margin:0 auto;image-rendering:pixelated;aspect-ratio:4/3;background:#000;border-radius:4px}
#pad{display:grid;grid-template-columns:1fr auto 1fr;gap:16px;margin-top:14px;align-items:center}
.grp{display:grid;gap:6px;justify-content:center}
.dp{grid-template-columns:repeat(3,54px);grid-template-rows:repeat(3,54px)}
.fx{grid-template-columns:repeat(3,54px);grid-template-rows:repeat(3,54px)}
.mid{grid-template-columns:repeat(2,90px)}
.sh{display:flex;justify-content:space-between;gap:8px;margin-top:12px}
button{background:var(--panel2);color:var(--text);border:1px solid #3a2f60;border-radius:12px;font:inherit;font-weight:700;cursor:pointer;min-height:44px}
button.on,button:active{background:var(--acc);color:#fff;border-color:var(--acc);box-shadow:0 0 14px var(--acc)}
.round{border-radius:50%}.cy{color:var(--cy)}.st{font-size:11px;color:var(--dim)}
.stk{grid-template-columns:repeat(3,40px);grid-template-rows:repeat(3,40px);margin-top:10px}
.stk button{min-height:0;font-size:12px}
#keys{margin-top:14px;font-size:12px;line-height:1.7}
kbd{background:var(--panel2);border:1px solid #3a2f60;border-radius:5px;padding:1px 6px;color:var(--cy)}
</style></head><body><main>
<header><h1>SYNTH CITY</h1><span id="stat" class="dim">verbinde …</span><span style="flex:1"></span>
<a href="/">Samples &amp; Logs</a><button id="fs" style="min-height:32px;padding:0 12px">Vollbild</button></header>
<div id="wrap"><canvas id="cv" width="640" height="480"></canvas></div>
<div class="sh"><span><button data-b="L1" style="width:70px">L1</button> <button data-b="L2" style="width:70px">L2</button></span>
<span><button data-b="R2" style="width:70px">R2</button> <button data-b="R1" style="width:70px">R1</button></span></div>
<div id="pad">
 <div>
  <div class="grp dp"><span></span><button data-b="UP">▲</button><span></span><button data-b="LEFT">◀</button><span></span><button data-b="RIGHT">▶</button><span></span><button data-b="DOWN">▼</button><span></span></div>
  <div class="grp stk"><span></span><button data-b="LY" data-v="1">↑</button><span></span><button data-b="LX" data-v="-1">←</button><span class="st" style="align-self:center;text-align:center">L</span><button data-b="LX" data-v="1">→</button><span></span><button data-b="LY" data-v="-1">↓</button><span></span></div>
 </div>
 <div class="grp mid"><button data-b="SELECT">SELECT</button><button data-b="START">START</button></div>
 <div>
  <div class="grp fx"><span></span><button data-b="X" class="round cy">X</button><span></span><button data-b="Y" class="round cy">Y</button><span></span><button data-b="A" class="round">A</button><span></span><button data-b="B" class="round">B</button><span></span></div>
  <div class="grp stk"><span></span><button data-b="RY" data-v="1">↑</button><span></span><button data-b="RX" data-v="-1">←</button><span class="st" style="align-self:center;text-align:center">R</span><button data-b="RX" data-v="1">→</button><span></span><button data-b="RY" data-v="-1">↓</button><span></span></div>
 </div>
</div>
<div id="keys" class="dim">Tastatur: <kbd>Pfeile</kbd> Steuerkreuz · <kbd>Z</kbd> A · <kbd>X</kbd> B · <kbd>S</kbd> X · <kbd>A</kbd> Y ·
<kbd>Q</kbd>/<kbd>W</kbd> L1/R1 · <kbd>1</kbd>/<kbd>2</kbd> L2/R2 · <kbd>Shift</kbd> SELECT · <kbd>Enter</kbd> START ·
<kbd>T F G H</kbd> linker Stick · <kbd>I J K L</kbd> rechter Stick</div>
</main>
<script>
const cv=document.getElementById('cv'),cx=cv.getContext('2d');
let W=640,H=480,img=cx.createImageData(W,H),px=new Uint32Array(img.data.buffer),seq=0,frames=0,bytes=0,last=performance.now();
const lut=new Uint32Array(65536);
for(let v=0;v<65536;v++){const r=(v>>11)&31,g=(v>>5)&63,b=v&31;lut[v]=0xff000000|(((b<<3)|(b>>2))<<16)|(((g<<2)|(g>>4))<<8)|((r<<3)|(r>>2));}
function decode(buf){
 const dv=new DataView(buf);seq=dv.getUint32(0,true);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);
 if(w!==W||h!==H){W=w;H=h;cv.width=w;cv.height=h;img=cx.createImageData(w,h);px=new Uint32Array(img.data.buffer);}
 const t=new Uint16Array(buf,12,(buf.byteLength-12)>>1);let i=0,k=0;const n=w*h;
 while(k<t.length&&i<n){const v=t[k++],c=v&0x3fff,m=v>>14;
  if(m===0)i+=c;
  else if(m===1){const col=lut[t[k++]];for(let j=0;j<c;j++)px[i++]=col;}
  else for(let j=0;j<c;j++)px[i++]=lut[t[k++]];}
 cx.putImageData(img,0,0);
}
async function loop(){
 try{const r=await fetch('/api/screen?since='+seq,{cache:'no-store'});
  if(r.status===200){const b=await r.arrayBuffer();bytes+=b.byteLength;decode(b);frames++;}
  else if(r.status===204){await new Promise(z=>setTimeout(z,25));}
  else throw new Error(r.status);
 }catch(e){document.getElementById('stat').textContent='keine Verbindung – erneuter Versuch …';seq=0;await new Promise(z=>setTimeout(z,800));}
 const now=performance.now();if(now-last>1000){document.getElementById('stat').textContent=frames+' Bilder/s · '+Math.round(bytes/1024)+' kB/s';frames=0;bytes=0;last=now;}
 loop();
}
loop();
function send(b,v){fetch('/api/input?b='+b+'&v='+v,{method:'POST'}).catch(()=>{});}
document.querySelectorAll('button[data-b]').forEach(el=>{
 const b=el.dataset.b,v=el.dataset.v||'1';
 const dn=e=>{e.preventDefault();el.setPointerCapture&&el.setPointerCapture(e.pointerId);el.classList.add('on');send(b,v);};
 const up=e=>{if(!el.classList.contains('on'))return;el.classList.remove('on');send(b,0);};
 el.addEventListener('pointerdown',dn);el.addEventListener('pointerup',up);el.addEventListener('pointercancel',up);el.addEventListener('lostpointercapture',up);
});
const km={ArrowUp:['UP',1],ArrowDown:['DOWN',1],ArrowLeft:['LEFT',1],ArrowRight:['RIGHT',1],KeyZ:['A',1],KeyX:['B',1],KeyS:['X',1],KeyA:['Y',1],
 KeyQ:['L1',1],KeyW:['R1',1],Digit1:['L2',1],Digit2:['R2',1],ShiftLeft:['SELECT',1],ShiftRight:['SELECT',1],Enter:['START',1],
 KeyT:['LY',1],KeyG:['LY',-1],KeyF:['LX',-1],KeyH:['LX',1],KeyI:['RY',1],KeyK:['RY',-1],KeyJ:['RX',-1],KeyL:['RX',1]};
const held={};
addEventListener('keydown',e=>{const m=km[e.code];if(!m)return;e.preventDefault();if(held[e.code])return;held[e.code]=1;send(m[0],m[1]);});
addEventListener('keyup',e=>{const m=km[e.code];if(!m)return;e.preventDefault();delete held[e.code];send(m[0],0);});
addEventListener('blur',()=>{for(const c in held){send(km[c][0],0);}for(const c in held)delete held[c];});
document.getElementById('fs').onclick=()=>{const w=document.getElementById('wrap');(w.requestFullscreen||w.webkitRequestFullscreen).call(w);};
</script></body></html>)HTML";
