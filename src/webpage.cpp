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
