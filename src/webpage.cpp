// Upload-Seite (wird vom Mini-Webserver ausgeliefert). Umwandlung nach WAV passiert im Browser.
extern const char WEB_PAGE[];
const char WEB_PAGE[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SYNTH CITY</title>
<style>
:root{--bg:#080612;--panel:#141026;--panel2:#20183a;--text:#d6eeff;--dim:#8a80b0;--acc:#ff2e88;--cy:#00e0ff;--ok:#00ffaa;--bad:#ff4060;--lock:#ffc040}
*{box-sizing:border-box}body{margin:0;color:var(--text);font:15px/1.45 ui-monospace,Menlo,Consolas,monospace;background:linear-gradient(180deg,#080612 0%,#140a2a 60%,#2a0a3a 100%) fixed}
main{max-width:920px;margin:0 auto;padding:20px 16px 60px}
h1{font-size:30px;margin:0 0 4px;letter-spacing:.18em;color:#fff;text-shadow:-2px 0 var(--acc),2px 0 var(--cy),0 0 18px var(--acc)}
h2{font-size:14px;color:var(--acc);letter-spacing:.12em;margin:28px 0 10px;text-transform:uppercase}
.sub{color:var(--dim);margin:0 0 18px}a{color:var(--cy)}
.row{display:flex;flex-wrap:wrap;gap:8px 14px;align-items:center;margin:10px 0}
select,input[type=text],input[type=search]{background:var(--panel2);color:var(--text);border:1px solid #3a3a46;border-radius:8px;padding:6px 10px;font:inherit}
input[type=search]{flex:1;min-width:180px}
label{color:var(--dim)}label input{accent-color:var(--acc)}
button{background:var(--panel2);color:var(--text);border:1px solid #3a3a46;border-radius:8px;padding:5px 10px;font:inherit;cursor:pointer;white-space:nowrap}
button:hover{border-color:var(--acc)}button.del:hover{border-color:var(--bad);color:var(--bad)}button.pri{border-color:var(--acc);color:#fff}
button.ic{padding:4px 8px;min-width:34px}
.list{background:var(--panel);border-radius:12px;overflow:hidden;position:relative}
.list.over{outline:2px dashed var(--acc);outline-offset:-4px;background:#22102e}
.item{display:flex;align-items:center;gap:8px;padding:6px 10px;border-top:1px solid #26222e}
.item:first-child{border-top:0}.item .n{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.item .s{color:var(--dim);font-size:13px;white-space:nowrap}
.item.dir .n{color:var(--lock);cursor:pointer}.item.dir .n:hover{text-decoration:underline}
.item.up .n{color:var(--dim);cursor:pointer}
.item input[type=checkbox]{accent-color:var(--acc);width:16px;height:16px}
.item.sel{background:#2a1838}
.info{padding:6px 12px 10px 44px;color:var(--dim);font-size:13px;border-top:1px dashed #2e2840}
.crumb{display:flex;flex-wrap:wrap;gap:4px;align-items:center;margin:10px 0 8px}
.crumb a{cursor:pointer;color:var(--cy);text-decoration:none;padding:2px 6px;border-radius:6px;background:var(--panel2)}
.crumb a:last-child{color:#fff;background:var(--acc)}
.crumb span{color:var(--dim)}
#selbar{display:none;position:sticky;bottom:10px;margin-top:10px;background:var(--panel2);border:1px solid var(--acc);border-radius:12px;padding:8px 12px;gap:10px;align-items:center;box-shadow:0 0 20px #0008}
.job{display:flex;gap:10px;padding:5px 12px;font-size:13px}.job .n{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.ok{color:var(--ok)}.bad{color:var(--bad)}.dim{color:var(--dim)}
#bar{height:4px;background:var(--panel2);border-radius:2px;overflow:hidden;margin-top:8px}#bar i{display:block;height:100%;width:0;background:var(--acc);transition:width .2s}
.hint{font-size:13px;color:var(--dim);margin:6px 2px}
@media(max-width:600px){.item{flex-wrap:wrap}.item .n{order:-1;flex:1 1 100%;white-space:normal;word-break:break-word}.item.dir .n,.item.up .n{order:0;flex:1}}
</style></head><body><main>
<h1>SYNTH <b>CITY</b></h1>
<p class="sub" id="info">verbinde …</p>
<p><a href="/screen" style="font-weight:700">▶ Bildschirm &amp; Fernbedienung</a> <span class="dim">– Live-Bild der Groovebox, bedienbar per Maus, Touch oder Tastatur</span></p>

<h2>Samples</h2>
<div class="row">
 <input type="search" id="q" placeholder="Samples suchen (alle Ordner) …">
 <button id="mk">+ Ordner</button>
 <button id="upf" class="pri">Dateien hochladen</button>
 <button id="upd2">Ordner hochladen</button>
 <input type="file" id="pick" multiple accept="audio/*,.wav,.mp3,.flac,.aif,.aiff,.ogg,.m4a" hidden>
 <input type="file" id="pickdir" webkitdirectory multiple hidden>
</div>
<div class="row">
 <label><input type="checkbox" id="conv" checked> in Mono/44,1 kHz umwandeln</label>
 <label><input type="checkbox" id="norm"> normalisieren</label>
</div>
<div class="crumb" id="crumb"></div>
<div id="samples" class="list"></div>
<p class="hint">Dateien oder ganze Ordner auf die Liste ziehen = Hochladen in den angezeigten Ordner (Unterordner bleiben erhalten). MP3, FLAC, AIFF, OGG … werden im Browser in WAV umgewandelt. Umbenennen/Verschieben: Projekte merken sich Samples per Name und finden verschobene Dateien danach nicht mehr.</p>
<div id="selbar"><b id="seln"></b><span style="flex:1"></span><button id="selmv">verschieben …</button><button id="seldel" class="del">löschen</button><button id="selnone">Auswahl aufheben</button></div>
<div id="bar"><i></i></div>
<div id="jobs" class="list" style="margin-top:8px"></div>

<h2>Projekte</h2>
<div class="row"><button id="pup" class="pri">Projekt (.gbx) hochladen</button><input type="file" id="ppick" accept=".gbx" multiple hidden>
<span class="dim">Name = Dateiname. Laden am Gerät unter PRJ → LADEN. Herunterladen = Sicherung.</span></div>
<div id="projects" class="list"></div>

<div id="upd" style="display:none">
<h2>Groovebox aktualisieren</h2>
<div class="row"><span>Installiert: <b id="ver">?</b></span><button id="uup">Update-Datei (.tgz) wählen</button><input type="file" id="upick" accept=".tgz" hidden>
<span id="ust" class="dim">Datei groovebox-os-update.tgz aus dem Release „nightly“</span></div>
</div>

<h2>Logs</h2>
<div class="row"><select id="logsel"><option>groovebox.log</option><option>groovebox.old.log</option><option>boot.txt</option><option>update.log</option><option>bench.txt</option><option>pc-modus.log</option><option>usb-diagnose</option></select>
<button id="logload">anzeigen</button><label><input type="checkbox" id="logauto"> automatisch aktualisieren</label><a id="logdl" download><button>herunterladen</button></a></div>
<pre id="logview" style="background:var(--panel);border-radius:12px;padding:12px;max-height:420px;overflow:auto;font-size:12px;white-space:pre-wrap;display:none"></pre>
</main>
<script>
const $=s=>document.querySelector(s);
const MAXDEPTH=8;
let cwd=localStorage.getItem('gbdir')||'',listing={dirs:[],files:[]},projects=[],sel=new Set(),query='',hits=null,infoOpen='';
const fmt=b=>b>1e9?(b/1e9).toFixed(1)+' GB':b>1e6?(b/1e6).toFixed(1)+' MB':Math.ceil(b/1e3)+' kB';
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const enc=encodeURIComponent;
// Namensteil bereinigen: Zeichen, die FAT nicht kennt, und Steuerzeichen raus; Umlaute bleiben
const cleanSeg=s=>s.normalize('NFC').replace(/[\\:*?"<>|\x00-\x1f\x7f\/]/g,'_').replace(/^[.\s]+/,'').replace(/\s+$/,'').slice(0,60)||'sample';
const join=(a,b)=>a?(b?a+'/'+b:a):b;
const depth=p=>p?p.split('/').length:0;
async function api(url,opt){const r=await fetch(url,opt);if(!r.ok)throw new Error(await r.text()||r.status);return r;}
async function refreshInfo(){
 try{const st=await (await fetch('/api/list')).json();projects=st.projects||[];
  $('#info').textContent=`${st.count} Samples · ${fmt(st.free)} frei auf der Karte`;
  if(st.os){$('#upd').style.display='';$('#ver').textContent=st.version||'?';}
  drawProjects();
 }catch(e){$('#info').textContent='keine Verbindung zum R36S';}
}
async function loadDir(d){
 try{listing=await (await api('/api/dir?root=samples&path='+enc(d))).json();cwd=d;}
 catch(e){if(d){return loadDir(d.includes('/')?d.slice(0,d.lastIndexOf('/')):'');}listing={dirs:[],files:[]};}
 try{localStorage.setItem('gbdir',cwd)}catch(e){}
 sel.clear();infoOpen='';draw();
}
function drawCrumb(){
 const parts=cwd?cwd.split('/'):[];let h=`<a data-cd="">samples</a>`,acc='';
 parts.forEach(p=>{acc=join(acc,p);h+=`<span>/</span><a data-cd="${esc(acc)}">${esc(p)}</a>`});
 $('#crumb').innerHTML=query?`<span>Suche „${esc(query)}“ – ${hits?hits.hits.length:0}${hits&&hits.more?'+':''} Treffer</span>`:h;
}
function fileRow(path,name,size,showDir){
 const k=esc(path),checked=sel.has(path);
 let h=`<div class="item${checked?' sel':''}"><input type="checkbox" data-sel="${k}"${checked?' checked':''}><button class="ic" data-play="${k}" title="anhören">▶</button>
  <span class="n" title="${k}">${esc(name)}${showDir?` <span class="dim">· ${esc(showDir)}</span>`:''}</span>${size!=null?`<span class="s">${fmt(size)}</span>`:''}
  <button class="ic" data-info="${k}" title="Infos">i</button><button class="ic" data-ren="${k}" title="umbenennen">✎</button>
  <a href="/api/file?root=samples&path=${enc(path)}" download="${esc(name)}"><button class="ic" title="herunterladen">⤓</button></a>
  <button class="ic del" data-del="${k}" title="löschen">✕</button></div>`;
 if(infoOpen===path)h+=`<div class="info" id="infobox">lese …</div>`;
 return h;
}
function draw(){
 drawCrumb();
 let h='';
 if(query){
  if(!hits)h='<div class="item dim">suche …</div>';
  else{hits.hits.forEach(p=>{const i=p.lastIndexOf('/');h+=fileRow(p,i<0?p:p.slice(i+1),null,i<0?'':p.slice(0,i))});if(!hits.hits.length)h='<div class="item dim">nichts gefunden</div>';}
 }else{
  if(cwd)h+=`<div class="item up"><span class="ic"></span><span class="n" data-cd="${esc(cwd.includes('/')?cwd.slice(0,cwd.lastIndexOf('/')):'')}">↰ ..</span></div>`;
  listing.dirs.sort((a,b)=>a.name.localeCompare(b.name,'de',{sensitivity:'base'})).forEach(d=>{const p=join(cwd,d.name),k=esc(p);
   h+=`<div class="item dir"><span style="width:16px">📁</span><span class="n" data-cd="${k}">${esc(d.name)}</span><span class="s">${d.n} ${d.n==1?'Eintrag':'Einträge'}</span>
   <button class="ic" data-ren="${k}" data-isdir="1" title="umbenennen/verschieben">✎</button><button class="ic del" data-rmdir="${k}" data-n="${d.n}" title="Ordner löschen">✕</button></div>`});
  listing.files.sort((a,b)=>a.name.localeCompare(b.name,'de',{numeric:true,sensitivity:'base'})).forEach(f=>{h+=fileRow(join(cwd,f.name),f.name,f.size)});
  if(!listing.dirs.length&&!listing.files.length)h+='<div class="item dim">Ordner ist leer – Dateien hierher ziehen</div>';
 }
 $('#samples').innerHTML=h;
 const n=sel.size;$('#selbar').style.display=n?'flex':'none';$('#seln').textContent=n+' ausgewählt';
 if(infoOpen)showInfo(infoOpen);
}
async function showInfo(p){
 try{const j=await (await api('/api/info?root=samples&path='+enc(p))).json();const b=$('#infobox');if(!b)return;
  b.innerHTML=j.ok?`${j.format} · ${j.ch==1?'Mono':j.ch==2?'Stereo':j.ch+' Kanäle'} · ${j.rate} Hz · ${j.bits} Bit · <b style="color:var(--text)">${j.sec.toFixed(2)} s</b>${j.sec>60?' <span class="bad">(länger als 60 s – wird gekürzt)</span>':''}`:'<span class="bad">kein lesbares WAV</span>';
 }catch(e){const b=$('#infobox');if(b)b.textContent='Fehler: '+e.message;}
}
function drawProjects(){
 $('#projects').innerHTML=projects.sort((a,b)=>a.path.localeCompare(b.path,'de',{sensitivity:'base'})).map(p=>{const k=esc(p.path),n=p.path.replace(/\.gbx$/i,'');
  return `<div class="item"><span class="n">${esc(n)}</span><span class="s">${fmt(p.size)}</span><a href="/api/file?root=projects&path=${enc(p.path)}" download="${k}"><button>herunterladen</button></a>
  <button class="ic" data-pren="${k}" title="umbenennen">✎</button><button class="ic del" data-pdel="${k}" title="löschen">✕</button></div>`}).join('')||'<div class="item dim">keine Projekte</div>';
}
let audio=null;
document.addEventListener('click',async e=>{
 const t=e.target.closest('[data-cd],[data-play],[data-del],[data-rmdir],[data-ren],[data-info],[data-pren],[data-pdel]');if(!t)return;const d=t.dataset;
 try{
  if(d.cd!==undefined){query='';$('#q').value='';hits=null;loadDir(d.cd);}
  else if(d.play){if(audio)audio.pause();audio=new Audio('/api/file?root=samples&path='+enc(d.play));audio.play();}
  else if(d.info){infoOpen=infoOpen===d.info?'':d.info;draw();}
  else if(d.del){if(confirm('Sample „'+d.del+'“ löschen?')){await api('/api/delete?root=samples&path='+enc(d.del),{method:'POST'});changed();}}
  else if(d.rmdir){if(confirm(`Ordner „${d.rmdir}“${+d.n?' mit allem Inhalt ('+d.n+' Einträge)':''} löschen?`)){await api('/api/rmdir?root=samples&path='+enc(d.rmdir),{method:'POST'});changed();}}
  else if(d.ren){const isDir=!!d.isdir,i=d.ren.lastIndexOf('/'),dir=i<0?'':d.ren.slice(0,i),name=i<0?d.ren:d.ren.slice(i+1);
   let to=prompt(`Neuer Pfad für ${isDir?'den Ordner':'das Sample'} (Ordner mit / trennen):`,d.ren);if(!to||to===d.ren)return;
   to=to.split('/').filter(x=>x).map(cleanSeg).join('/');if(!isDir&&!/\.wav$/i.test(to))to+='.wav';
   await api('/api/move?root=samples&path='+enc(d.ren)+'&to='+enc(to),{method:'POST'});changed();}
  else if(d.pdel){if(confirm('Projekt „'+d.pdel+'“ löschen?')){await api('/api/delete?root=projects&path='+enc(d.pdel),{method:'POST'});refreshInfo();}}
  else if(d.pren){let to=prompt('Neuer Projektname:',d.pren.replace(/\.gbx$/i,''));if(!to)return;to=cleanSeg(to.replace(/\//g,'_'))+'.gbx';
   await api('/api/move?root=projects&path='+enc(d.pren)+'&to='+enc(to),{method:'POST'});refreshInfo();}
 }catch(err){alert(err.message||err);}
});
document.addEventListener('change',e=>{const p=e.target.dataset&&e.target.dataset.sel;if(p===undefined)return;if(e.target.checked)sel.add(p);else sel.delete(p);
 e.target.closest('.item').classList.toggle('sel',e.target.checked);const n=sel.size;$('#selbar').style.display=n?'flex':'none';$('#seln').textContent=n+' ausgewählt';});
$('#selnone').onclick=()=>{sel.clear();draw();};
$('#seldel').onclick=async()=>{if(!confirm(sel.size+' Samples löschen?'))return;for(const p of [...sel]){try{await api('/api/delete?root=samples&path='+enc(p),{method:'POST'});}catch(e){}}changed();};
$('#selmv').onclick=async()=>{let to=prompt('Zielordner (z. B. drums/kicks, leer = Hauptordner):',cwd);if(to===null)return;
 to=to.split('/').filter(x=>x).map(cleanSeg).join('/');let bad=0;
 for(const p of [...sel]){const n=p.slice(p.lastIndexOf('/')+1),dst=join(to,n);if(dst===p)continue;try{await api('/api/move?root=samples&path='+enc(p)+'&to='+enc(dst),{method:'POST'});}catch(e){bad++;}}
 if(bad)alert(bad+' Dateien nicht verschoben (Ziel existiert schon?)');changed();};
function changed(){refreshInfo();if(query)search(query);else loadDir(cwd);}
$('#mk').onclick=async()=>{const n=prompt('Name des neuen Ordners in /samples/'+(cwd?cwd+'/':'')+':');if(!n)return;
 const p=join(cwd,n.split('/').filter(x=>x).map(cleanSeg).join('/'));if(depth(p)>MAXDEPTH){alert('Höchstens '+MAXDEPTH+' Ordnerebenen');return;}
 try{await api('/api/mkdir?root=samples&path='+enc(p),{method:'POST'});loadDir(p);}catch(e){alert(e.message);}};
let sTimer=0;
async function search(q){hits=null;draw();try{hits=await (await api('/api/search?root=samples&q='+enc(q))).json();}catch(e){hits={hits:[]};}if(query===q)draw();}
$('#q').oninput=e=>{clearTimeout(sTimer);query=e.target.value.trim();sTimer=setTimeout(()=>{if(query)search(query);else{hits=null;draw();}},250);};
// ---- Hochladen (Dateien, Ordner, Ziehen & Ablegen)
function wav(buf){
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
const AUDIO=/\.(wav|mp3|flac|aif|aiff|ogg|oga|m4a|aac|opus|webm)$/i;
// items: [{file, rel}] rel = Pfad relativ zum aktuellen Ordner (mit Unterordnern)
async function uploadItems(items){
 items=items.filter(it=>AUDIO.test(it.file.name));
 if(!items.length){alert('Keine Audiodateien gefunden');return;}
 const base=cwd;let done=0;$('#bar i').style.width='0';
 for(const it of items){
  const parts=it.rel.split('/').filter(x=>x),fname=parts.pop();
  const sub=parts.map(cleanSeg).join('/'),name=cleanSeg(fname.replace(/\.[^.]+$/,''))+'.wav',path=join(join(base,sub),name);
  const row=document.createElement('div');row.className='job';row.innerHTML=`<span class="n">${esc(join(sub,fname))}</span><span class="st dim">wandle um …</span>`;$('#jobs').prepend(row);
  const st=row.querySelector('.st');
  try{
   if(depth(path)-1>MAXDEPTH)throw new Error('zu viele Ordnerebenen (max. '+MAXDEPTH+')');
   const isWav=/\.wav$/i.test(fname);let blob=it.file;
   if(!isWav||$('#conv').checked)blob=await convert(it.file);
   st.textContent='lade hoch …';
   await api('/api/upload?root=samples&path='+enc(path),{method:'PUT',body:blob});
   st.textContent='✓ '+fmt(blob.size);st.className='st ok';
  }catch(e){st.textContent='✗ '+(e.message||e);st.className='st bad';}
  done++;$('#bar i').style.width=(done/items.length*100)+'%';
 }
 changed();
}
$('#upf').onclick=()=>$('#pick').click();$('#upd2').onclick=()=>$('#pickdir').click();
$('#pick').onchange=e=>{uploadItems([...e.target.files].map(f=>({file:f,rel:f.name})));e.target.value='';};
$('#pickdir').onchange=e=>{uploadItems([...e.target.files].map(f=>({file:f,rel:f.webkitRelativePath||f.name})));e.target.value='';};
function readEntry(en,prefix,out){return new Promise(res=>{
 if(en.isFile)en.file(f=>{out.push({file:f,rel:prefix+f.name});res();},()=>res());
 else if(en.isDirectory){const rd=en.createReader(),all=[];const next=()=>rd.readEntries(async es=>{if(es.length){all.push(...es);next();}else{for(const c of all)await readEntry(c,prefix+en.name+'/',out);res();}},()=>res());next();}
 else res();});}
const zone=$('#samples');
['dragenter','dragover'].forEach(t=>zone.addEventListener(t,e=>{e.preventDefault();zone.classList.add('over')}));
['dragleave','drop'].forEach(t=>zone.addEventListener(t,e=>{e.preventDefault();zone.classList.remove('over')}));
zone.addEventListener('drop',async e=>{const its=[...(e.dataTransfer.items||[])],out=[];
 if(its.length&&its[0].webkitGetAsEntry){const ens=its.map(i=>i.webkitGetAsEntry()).filter(x=>x);for(const en of ens)await readEntry(en,'',out);}
 else [...e.dataTransfer.files].forEach(f=>out.push({file:f,rel:f.name}));
 if(query){query='';$('#q').value='';hits=null;}
 uploadItems(out);});
// ---- Projekte
$('#pup').onclick=()=>$('#ppick').click();
$('#ppick').onchange=async e=>{for(const f of e.target.files){const n=cleanSeg(f.name.replace(/\.gbx$/i,'').replace(/\//g,'_'))+'.gbx';
 if(projects.some(p=>p.path.toLowerCase()===n.toLowerCase())&&!confirm('Projekt „'+n+'“ gibt es schon – überschreiben?'))continue;
 try{await api('/api/upload?root=projects&path='+enc(n),{method:'PUT',body:f});}catch(err){alert(n+': '+err.message);}}
 e.target.value='';refreshInfo();};
// ---- Update, Logs
$('#uup').onclick=()=>$('#upick').click();
$('#upick').onchange=async e=>{const f=e.target.files[0];if(!f)return;const st=$('#ust');st.className='';st.textContent='lade hoch …';
 try{const r=await fetch('/api/update',{method:'PUT',body:f});const t=await r.text();if(!r.ok)throw new Error(t);
  st.className='ok';st.textContent='✓ übertragen – Groovebox startet neu und installiert (ca. 15 s)';setTimeout(refreshInfo,15000);}
 catch(err){st.className='bad';st.textContent='✗ '+(err.message||err);}e.target.value='';};
async function loadLog(){const n=$('#logsel').value,v=$('#logview');v.style.display='';$('#logdl').href='/api/log?name='+n;$('#logdl').download=n;
 try{const r=await fetch('/api/log?name='+enc(n));v.textContent=await r.text();v.scrollTop=v.scrollHeight;}catch(e){v.textContent='keine Verbindung';}}
$('#logload').onclick=loadLog;$('#logsel').onchange=loadLog;setInterval(()=>{if($('#logauto').checked)loadLog()},3000);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)changed();});
refreshInfo();loadDir(cwd);
</script></body></html>)HTML";

// Bildschirm-Spiegelung + Fernbedienung (http://<ip>/screen)
extern const char WEB_SCREEN[];
const char WEB_SCREEN[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes"><meta name="mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#080612"><meta name="apple-mobile-web-app-status-bar-style" content="black-translucent"><meta name="apple-mobile-web-app-title" content="SYNTH CITY">
<link rel="manifest" href="/manifest.json"><link rel="apple-touch-icon" href="/apple-touch-icon.png"><link rel="icon" href="/icon-192.png">
<title>SYNTH CITY – Fernbedienung</title>
<style>
:root{--bg:#080612;--panel:#141026;--panel2:#20183a;--line:#3a2f60;--text:#d6eeff;--dim:#8a80b0;--acc:#ff2e88;--cy:#00e0ff;--gr:#2bff9a;--b:clamp(44px,7vmin,64px)}
*{box-sizing:border-box;-webkit-user-select:none;user-select:none;-webkit-touch-callout:none;-webkit-tap-highlight-color:transparent}
html,body{margin:0;height:100%;background:linear-gradient(180deg,#080612,#140a2a 60%,#2a0a3a) fixed;color:var(--text);font:14px ui-monospace,Menlo,Consolas,monospace;overflow:hidden}
#app{height:100%;display:flex;flex-direction:column;padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)}
header{display:flex;align-items:center;gap:10px;padding:6px 10px;flex-wrap:wrap}
h1{font-size:16px;margin:0;letter-spacing:.18em;color:#fff;text-shadow:-2px 0 var(--acc),2px 0 var(--cy)}
#stat{color:var(--dim);font-size:12px}#dot{width:9px;height:9px;border-radius:50%;background:#555;display:inline-block}
header .sp{flex:1}a{color:var(--cy)}
.hb.on{background:var(--acc);border-color:var(--acc);color:#fff}
.hb{background:var(--panel2);color:var(--text);border:1px solid var(--line);border-radius:8px;font:inherit;font-size:12px;padding:5px 10px;cursor:pointer}
#stage{flex:1;min-height:0;display:grid;grid-template-columns:1fr;grid-template-rows:minmax(0,1fr) auto;gap:8px;padding:0 10px 10px}
#scr{min-height:0;min-width:0;overflow:hidden;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px}
canvas{display:block;flex:none;width:320px;height:240px;image-rendering:pixelated;image-rendering:crisp-edges;background:#000;border-radius:6px;box-shadow:0 0 0 2px #000,0 0 26px #ff2e8840;outline:none}
#ctl{display:grid;grid-template-columns:1fr auto 1fr;align-items:center;gap:8px;touch-action:none}
.side{display:flex;flex-direction:column;align-items:center;gap:10px}
.shd{display:flex;gap:6px}
.btn{width:calc(var(--b)*1.25);height:calc(var(--b)*.72);display:flex;align-items:center;justify-content:center;background:var(--panel2);border:1px solid var(--line);border-radius:10px;font-weight:700;color:var(--text);touch-action:none;cursor:pointer;transition:background .05s}
.btn.on{background:var(--acc);border-color:var(--acc);color:#fff;box-shadow:0 0 16px var(--acc)}
.dpad{position:relative;width:calc(var(--b)*2.9);height:calc(var(--b)*2.9);touch-action:none}
.dpad i{position:absolute;width:calc(var(--b)*.98);height:calc(var(--b)*.98);background:var(--panel2);border:1px solid var(--line);border-radius:10px;display:flex;align-items:center;justify-content:center;color:var(--dim);font-style:normal;font-size:18px}
.dpad i.on{background:var(--acc);border-color:var(--acc);color:#fff;box-shadow:0 0 16px var(--acc)}
.dpad .u{left:calc(50% - var(--b)*.49);top:0}.dpad .d{left:calc(50% - var(--b)*.49);bottom:0}
.dpad .l{top:calc(50% - var(--b)*.49);left:0}.dpad .r{top:calc(50% - var(--b)*.49);right:0}
.dpad .c{left:calc(50% - var(--b)*.3);top:calc(50% - var(--b)*.3);width:calc(var(--b)*.6);height:calc(var(--b)*.6);border-radius:50%;background:var(--panel)}
.face{position:relative;width:calc(var(--b)*2.9);height:calc(var(--b)*2.9);touch-action:none}
.face .btn{position:absolute;width:var(--b);height:var(--b);border-radius:50%;font-size:18px}
.face .x{left:calc(50% - var(--b)/2);top:0}.face .b{left:calc(50% - var(--b)/2);bottom:0}
.face .y{top:calc(50% - var(--b)/2);left:0}.face .a{top:calc(50% - var(--b)/2);right:0}
.face .x,.face .y{color:var(--cy)}
.mid{display:flex;flex-direction:column;align-items:center;gap:8px}
.mid .row{display:flex;gap:8px}
.mid .btn{width:calc(var(--b)*1.5);height:calc(var(--b)*.6);font-size:12px;border-radius:20px}
.stk{display:none;grid-template-columns:repeat(3,calc(var(--b)*.6));grid-template-rows:repeat(3,calc(var(--b)*.6));gap:3px;touch-action:none}
.stk .btn{width:auto;height:auto;font-size:11px;border-radius:8px}
body.sticks .stk{display:grid}
#help{position:fixed;inset:0;background:#080612ee;display:none;align-items:center;justify-content:center;z-index:9;padding:20px}
#help.show{display:flex}#help div{max-width:560px;background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:18px 20px;line-height:1.9}
kbd{background:var(--panel2);border:1px solid var(--line);border-radius:5px;padding:1px 6px;color:var(--cy)}
/* Querformat (Handy quer, Tablet, PC-Fenster breit): Bild in der Mitte, Bedienung links und rechts */
@media (orientation:landscape) and (min-aspect-ratio:13/10){
 #stage{grid-template-columns:auto minmax(0,1fr) auto;grid-template-rows:minmax(0,1fr);align-items:center}
 #ctl{display:contents}
 .lside{grid-column:1;grid-row:1}.rside{grid-column:3;grid-row:1}
 #scr{grid-column:2;grid-row:1;height:100%;width:100%}
 .mid{flex-direction:row}
}
@media (max-height:420px){header{padding:2px 8px}h1{font-size:12px}#stat{font-size:10px}.hb{padding:2px 8px}}
#cvw{position:relative;flex:none;touch-action:none;overflow:hidden;border-radius:6px}
#cv{transform-origin:0 0}
#zl{min-width:46px;text-align:center}
#lost{position:absolute;inset:0;display:none;align-items:center;justify-content:center;text-align:center;background:rgba(8,4,18,.78);color:#fff;font-size:15px;line-height:1.6;z-index:5;backdrop-filter:grayscale(1)}#lost.on{display:flex}#lost b{color:#ff4fa3;font-size:18px}#lostT{color:var(--dim);font-size:12px}#lost button{margin-top:10px;padding:6px 16px;border-radius:6px;border:1px solid var(--cy);background:transparent;color:var(--cy);font:inherit}.spin{width:34px;height:34px;margin:0 auto 8px;border-radius:50%;border:3px solid rgba(255,255,255,.15);border-top-color:var(--cy);animation:spin 1s linear infinite}@keyframes spin{to{transform:rotate(360deg)}}.rip{position:absolute;width:44px;height:44px;margin:-22px 0 0 -22px;border-radius:50%;border:2px solid var(--cy);pointer-events:none;animation:rip .45s ease-out forwards}
.rip.hold{border-color:var(--acc);animation:none;opacity:.8}
@keyframes rip{from{transform:scale(.4);opacity:1}to{transform:scale(1.6);opacity:0}}
/* Bild-Vollbild: nur das Bild, so gross wie moeglich, schwebende Mini-Leiste */
body.full header,body.full #ctl,body.full #midL{display:none !important}
body.full #app{padding:0}body.full #stage{padding:0;gap:0;grid-template-columns:1fr !important;grid-template-rows:minmax(0,1fr) !important}
body.full #scr{grid-column:1 !important;grid-row:1 !important;width:100%;height:100%}
body.full canvas,body.full #cvw{border-radius:0}body.full canvas{box-shadow:none}
#fbar{display:none;position:fixed;right:max(6px,env(safe-area-inset-right));top:max(6px,env(safe-area-inset-top));z-index:8;gap:6px;opacity:.55;touch-action:none}
body.full #fbar{display:flex}#fbar:hover,#fbar:active{opacity:1}
#fbar .btn{width:auto;height:30px;padding:0 10px;font-size:11px;border-radius:15px;background:#20183acc}
body.full.fleft #fbar{right:auto;left:max(6px,env(safe-area-inset-left))}
body.nopad #ctl{display:none}
body.nopad #stage{grid-template-columns:1fr !important}
body.nopad #scr{grid-column:1 !important}
</style></head><body><div id="app">
<header><span id="dot"></span><h1>SYNTH CITY</h1><span id="stat">…</span><span class="sp"></span>
<button class="hb" id="bAud" title="Ton im Browser">🔈 Ton</button><input id="vol" type="range" min="0" max="100" value="80" style="width:70px;display:none"><button class="hb" id="zo" title="Verkleinern (−)">−</button><button class="hb" id="zl" title="Zoom zurücksetzen (0)">100%</button><button class="hb" id="zi" title="Vergrößern (+)">+</button><button class="hb" id="bPad">Tasten</button><button class="hb" id="bSt">Sticks</button><button class="hb" id="bHelp">?</button><button class="hb" id="bFull" title="Nur das Bild, so groß wie möglich (V)">Vollbild</button><button class="hb" id="bFs" title="Browser-Vollbild">⛶</button><a href="/" class="hb" style="text-decoration:none">Dateien</a></header>
<div id="fbar"><div class="btn" data-b="SELECT">SEL</div><div class="btn" data-b="START">START</div><div class="btn" id="fSide" title="Leiste auf die andere Seite">⇆</div><div class="btn" id="fExit" title="Vollbild beenden (V)">✕</div></div>
<div id="stage">
 <div id="scr"><div id="cvw"><canvas id="cv" width="640" height="480" tabindex="0"></canvas><div id="lost"><div><div class="spin"></div><b>Verbindung verloren</b><br><span id="lostT">verbinde neu …</span><br><button id="lostR">Neu laden</button></div></div></div>
  <div class="mid" id="midL"><div class="row"><div class="btn" data-b="SELECT">SELECT</div><div class="btn" data-b="START">START</div></div></div></div>
 <div id="ctl">
  <div class="side lside"><div class="shd"><div class="btn" data-b="L2">L2</div><div class="btn" data-b="L1">L1</div></div>
   <div class="dpad" id="dpad"><i class="u">▲</i><i class="l">◀</i><i class="c"></i><i class="r">▶</i><i class="d">▼</i></div>
   <div class="stk"><span></span><div class="btn" data-b="LY" data-v="1">↑</div><span></span><div class="btn" data-b="LX" data-v="-1">←</div><span style="align-self:center;text-align:center;color:var(--dim)">L</span><div class="btn" data-b="LX" data-v="1">→</div><span></span><div class="btn" data-b="LY" data-v="-1">↓</div><span></span></div></div>
  <div class="side rside"><div class="shd"><div class="btn" data-b="R1">R1</div><div class="btn" data-b="R2">R2</div></div>
   <div class="face"><div class="btn x" data-b="X">X</div><div class="btn y" data-b="Y">Y</div><div class="btn a" data-b="A">A</div><div class="btn b" data-b="B">B</div></div>
   <div class="stk"><span></span><div class="btn" data-b="RY" data-v="1">↑</div><span></span><div class="btn" data-b="RX" data-v="-1">←</div><span style="align-self:center;text-align:center;color:var(--dim)">R</span><div class="btn" data-b="RX" data-v="1">→</div><span></span><div class="btn" data-b="RY" data-v="-1">↓</div><span></span></div></div>
 </div>
</div></div>
<div id="help"><div><b style="color:#fff">Tastatur</b><br>
<kbd>←↑→↓</kbd> Steuerkreuz &nbsp; <kbd>Z</kbd>/<kbd>Leertaste</kbd> A &nbsp; <kbd>X</kbd>/<kbd>⌫</kbd> B &nbsp; <kbd>S</kbd> X &nbsp; <kbd>A</kbd> Y<br>
<kbd>Q</kbd> L1 &nbsp; <kbd>W</kbd> R1 &nbsp; <kbd>1</kbd> L2 &nbsp; <kbd>2</kbd> R2 &nbsp; <kbd>Shift</kbd>/<kbd>Tab</kbd> SELECT &nbsp; <kbd>Enter</kbd> START (halten = FILL)<br>
<kbd>T F G H</kbd> linker Stick &nbsp; <kbd>I J K L</kbd> rechter Stick &nbsp; <kbd>V</kbd> Bild-Vollbild (nur das Bild, Leiste oben rechts: SEL, START, ⇆ Seite, ✕ beenden) &nbsp; <kbd>F11</kbd> Browser-Vollbild<br>
<b style="color:#fff">Zoom</b>: zwei Finger gleichzeitig aufs Bild = vergrößern/verschieben, Knöpfe − 100% + oben, am PC Strg+Mausrad (Mausrad verschiebt), Tasten <kbd>+</kbd> <kbd>−</kbd> <kbd>0</kbd>.<br><b style="color:#fff">Touch aufs Bild</b>: Spur, Seite, Reiter, Kachel oder Step antippen. Auf einer Kachel ziehen (hoch/rechts = mehr) ändert den Wert. Step antippen = Trig an/aus, Step halten + mit zweitem Finger Kachel ziehen = P-Lock. Kachel nochmal antippen = Auswahlliste (Sample, LFO-Ziel, Engine). „Tasten“ blendet das Gamepad aus.<br><b style="color:#fff">Touch-Tasten</b>: mehrere Finger gleichzeitig, Wischen über das Steuerkreuz geht auch.<br>
<b style="color:#fff">Gamepad</b>: am PC/Handy angeschlossenes Gamepad wird automatisch erkannt.<br>
<span style="color:var(--dim)">Tippen zum Schließen</span></div></div>
<script>
'use strict';
const $=id=>document.getElementById(id),cv=$('cv'),cx=cv.getContext('2d',{alpha:false});
let W=640,H=480,img=cx.createImageData(W,H),px=new Uint32Array(img.data.buffer),seq=0,dirty=false,frames=0,bytes=0,lat=0,mode='',last=performance.now();
const lut=new Uint32Array(65536);
for(let v=0;v<65536;v++){const r=(v>>11)&31,g=(v>>5)&63,b=v&31;lut[v]=0xff000000|(((b<<3)|(b>>2))<<16)|(((g<<2)|(g>>4))<<8)|((r<<3)|(r>>2));}
function decode(buf){
 const dv=new DataView(buf);seq=dv.getUint32(0,true);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);
 if(w!==W||h!==H){W=w;H=h;cv.width=w;cv.height=h;img=cx.createImageData(w,h);px=new Uint32Array(img.data.buffer);}
 const t=new Uint16Array(buf,12,(buf.byteLength-12)>>1),n=w*h;let i=0,k=0;
 while(k<t.length&&i<n){const v=t[k++],c=v&0x3fff,m=v>>14;
  if(m===0)i+=c;else if(m===1){px.fill(lut[t[k++]],i,i+c);i+=c;}else for(let j=0;j<c;j++)px[i++]=lut[t[k++]];}
 dirty=true;frames++;bytes+=buf.byteLength;
}
function draw(){if(dirty){cx.putImageData(img,0,0);dirty=false;}pollPad();requestAnimationFrame(draw);}
requestAnimationFrame(draw);
// Bild immer ganz sichtbar und so gross wie moeglich (4:3), SELECT/START darunter
function fit(){const sc=$('scr'),r=sc.getBoundingClientRect(),mh=$('midL').offsetHeight+10;let w=r.width,h=r.height-mh;
 if(w<=0||h<=0)return;if(w/h>4/3)w=h*4/3;else h=w*3/4;w=Math.floor(w);h=Math.floor(h);cv.style.width=w+'px';cv.style.height=h+'px';$('cvw').style.width=w+'px';$('cvw').style.height=h+'px';fw=w;fh=h;applyZoom();}
addEventListener('resize',fit);if(window.ResizeObserver)new ResizeObserver(fit).observe($('scr'));setTimeout(fit,0);
// ---- Verbindung: WebSocket (Bildstrom + Tasten), sonst Abfrage
let ws=null,wsFails=0,everWs=false,pingT=0,lastRx=performance.now(),lostAt=0,tries=0;
// Verbindungsverlust sichtbar machen: Bild abdunkeln + Hinweis statt eingefrorenem Bild
function setLost(on){const el=$('lost');if(on){if(!lostAt){lostAt=performance.now();tries=0;}el.classList.add('on');}
 else{lostAt=0;el.classList.remove('on');}}
function lostTick(){if(!lostAt)return;const s=Math.round((performance.now()-lostAt)/1000);
 $('lostT').textContent='seit '+s+' s · Versuch '+tries+(navigator.onLine===false?' · Handy offline':' · WLAN/USB prüfen');}
function setStat(ok,txt){$('dot').style.background=ok?'var(--gr)':'#c33';if(txt)$('stat').textContent=txt;setLost(!ok);}
$('lostR').onclick=e=>{e.stopPropagation();location.reload();};
// Watchdog: der R36S beantwortet den Ping jede Sekunde - kommt 5 s nichts (grosses Bild ueber langsames WLAN braucht etwas), ist die Leitung tot
// (WLAN weg, Gerät aus, Handy aus dem Ruhezustand): Socket verwerfen und neu verbinden
function wsKill(){if(!ws)return;const w=ws;ws=null;w.onclose=w.onmessage=w.onopen=null;try{w.close();}catch(e){}
 if(mode==='ws')mode='';setStat(false,'getrennt – verbinde neu …');releaseAll();setTimeout(connect,300);
 if(aws){const a=aws;aws=null;a.onclose=a.onmessage=null;try{a.close();}catch(e){}if(aOn)setTimeout(connectAudio,800);}}   // Ton-Socket haengt meist mit
setInterval(()=>{lostTick();if(ws&&performance.now()-lastRx>5000)wsKill();},500);
function connect(){
 if(wsFails>=3&&!everWs){mode='poll';pollLoop();return;}
 tries++;
 try{ws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/ws');}catch(e){wsFails++;setTimeout(connect,500);return;}
 ws.binaryType='arraybuffer';
 const me=ws;lastRx=performance.now();
 const to=setTimeout(()=>{if(ws===me&&me.readyState!==1)wsKill();},4000);   // Aufbau haengt (kein Netz)
 ws.onopen=()=>{clearTimeout(to);mode='ws';everWs=true;wsFails=0;lastRx=performance.now();setStat(true);ws.send('f');};
 ws.onmessage=e=>{lastRx=performance.now();if(typeof e.data==='string'){if(e.data[0]==='P')lat=performance.now()-(+e.data.slice(2));}else decode(e.data);};
 ws.onclose=()=>{clearTimeout(to);if(ws!==me)return;ws=null;if(mode==='ws')mode='';wsFails++;setStat(false,'getrennt – verbinde neu …');releaseAll();setTimeout(connect,600);};
}
async function pollLoop(){
 for(;;){tries++;try{const ac=window.AbortController?new AbortController():null,tm=setTimeout(()=>{if(ac)ac.abort();},4000);
   const r=await fetch('/api/screen?since='+seq,{cache:'no-store',signal:ac?ac.signal:undefined});clearTimeout(tm);
   if(r.status===200){decode(await r.arrayBuffer());}else if(r.status===204){await new Promise(z=>setTimeout(z,20));}else throw 0;
   setStat(true);}catch(e){setStat(false,'keine Verbindung …');seq=0;await new Promise(z=>setTimeout(z,800));}}
}
// ---- Ton: eigener WebSocket /wsa (16 Bit Stereo), Pakete werden luecklos hintereinander eingeplant.
// Puffer ~120 ms; laeuft er voll (Uhren von R36S und Handy weichen ab), wird minimal schneller abgespielt, bei Luecke neu gepuffert.
let actx=null,aws=null,again=null,nextT=0,aOn=false,aBuf=0,aDrop=0;const TGT=0.12;
function audioOn(){
 if(!actx){const AC=window.AudioContext||window.webkitAudioContext;if(!AC){alert('Dieser Browser kann keinen Ton abspielen');return;}
  actx=new AC({latencyHint:'interactive'});again=actx.createGain();again.connect(actx.destination);}
 actx.resume();again.gain.value=($('vol').value/100)**2;aOn=true;$('bAud').classList.add('on');$('bAud').textContent='🔊 Ton';$('vol').style.display='';
 try{localStorage.audio=1;}catch(e){}
 connectAudio();
}
function audioOff(){aOn=false;if(aws){aws.onclose=null;aws.close();aws=null;}$('bAud').classList.remove('on');$('bAud').textContent='🔈 Ton';$('vol').style.display='none';try{localStorage.audio='';}catch(e){}}
function connectAudio(){
 if(!aOn||aws)return;
 aws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/wsa');aws.binaryType='arraybuffer';
 aws.onmessage=e=>{if(typeof e.data==='string'||!actx)return;
  const dv=new DataView(e.data),rate=dv.getUint32(0,true),n=(e.data.byteLength-8)>>2;if(n<=0)return;
  const pcm=new Int16Array(e.data,8,n*2),b=actx.createBuffer(2,n,rate),L=b.getChannelData(0),R=b.getChannelData(1);
  for(let i=0;i<n;i++){L[i]=pcm[i*2]/32768;R[i]=pcm[i*2+1]/32768;}
  const now=actx.currentTime;
  if(nextT<now+0.01){nextT=now+TGT;}
  let lead=nextT-now;if(lead>TGT*3){aDrop++;return;}
  const pr=lead>TGT*1.4?1.004:(lead<TGT*0.6?0.997:1);
  const src=actx.createBufferSource();src.buffer=b;src.playbackRate.value=pr;src.connect(again);src.start(nextT);
  nextT+=n/rate/pr;aBuf=lead;};
 aws.onclose=()=>{aws=null;if(aOn)setTimeout(connectAudio,800);};
}
$('bAud').onclick=()=>{if(aOn)audioOff();else audioOn();};
$('vol').oninput=()=>{if(again)again.gain.value=($('vol').value/100)**2;try{localStorage.vol=$('vol').value;}catch(e){}};
try{if(localStorage.vol)$('vol').value=localStorage.vol;}catch(e){}
try{if(localStorage.audio){const once=ev=>{document.removeEventListener('pointerdown',once,true);if(!aOn&&ev.target.id!=='bAud')audioOn();};document.addEventListener('pointerdown',once,true);$('bAud').textContent='🔈 Ton (antippen)';}}catch(e){}
if('serviceWorker' in navigator&&window.isSecureContext)navigator.serviceWorker.register('/sw.js').catch(()=>{});
setInterval(()=>{const now=performance.now(),s=(now-last)/1000;
 if(ws&&ws.readyState===1)ws.send('p '+now.toFixed(0));
 $('stat').textContent=(mode==='ws'?'Live':mode==='poll'?'Abfrage':'…')+' · '+Math.round(frames/s)+' fps · '+Math.round(bytes/1024/s)+' kB/s'+(mode==='ws'?' · '+Math.round(lat)+' ms':'')+(aOn&&aws?' · Ton '+Math.round(aBuf*1000)+' ms':'');
 frames=0;bytes=0;last=now;},1000);
document.addEventListener('visibilitychange',()=>{if(document.hidden){releaseAll();return;}
 lastRx=performance.now();if(ws&&ws.readyState===1){ws.send('f');ws.send('p '+lastRx.toFixed(0));}});   // zurueck aus dem Hintergrund: Watchdog prueft sofort
addEventListener('online',()=>{if(ws&&ws.readyState!==1)wsKill();});
addEventListener('offline',()=>{if(ws)wsKill();});
connect();
// ---- Tasten senden (Zaehler je Taste: Touch + Tastatur + Gamepad duerfen sich ueberlappen)
const cnt={},AX={LX:1,LY:1,RX:1,RY:1};
function vib(){if(navigator.vibrate)try{navigator.vibrate(8);}catch(e){}}
function out(msg,b,v){if(ws&&ws.readyState===1)ws.send(msg);else fetch('/api/input?b='+b+'&v='+v,{method:'POST',keepalive:true}).catch(()=>{});}
function press(b,v){const key=b+(AX[b]?':'+v:'');cnt[key]=(cnt[key]||0)+1;if(cnt[key]>1)return;
 if(AX[b])out('a '+b+' '+v,b,v);else out('d '+b,b,1);show(key,true);vib();}
function release(b,v){const key=b+(AX[b]?':'+v:'');if(!cnt[key])return;if(--cnt[key]>0)return;delete cnt[key];
 if(AX[b])out('a '+b+' 0',b,0);else out('u '+b,b,0);show(key,false);}
function releaseAll(){for(const k in cnt){const[b,v]=k.split(':');cnt[k]=1;release(b,v===undefined?1:+v);}touches.clear();}
const dEl={UP:'.u',DOWN:'.d',LEFT:'.l',RIGHT:'.r'};
function show(key,on){const[b,v]=key.split(':');
 if(dEl[b]){$('dpad').querySelector(dEl[b]).classList.toggle('on',on);return;}
 document.querySelectorAll('[data-b="'+b+'"]'+(v!==undefined?'[data-v="'+v+'"]':'')).forEach(e=>e.classList.toggle('on',on));}
// ---- Touch/Maus: jeder Finger haelt eine Taste (bzw. eine Steuerkreuz-Richtung), Wischen wechselt die Taste
const touches=new Map();   // pointerId -> Liste [b,v]
function targetsAt(x,y){
 const d=$('dpad').getBoundingClientRect();
 if(x>=d.left&&x<=d.right&&y>=d.top&&y<=d.bottom){
  const dx=x-(d.left+d.width/2),dy=y-(d.top+d.height/2),r=Math.hypot(dx,dy);if(r<d.width*0.12)return[];
  const a=Math.atan2(dy,dx)*180/Math.PI,res=[];   // 8 Richtungen: Diagonale = zwei Tasten
  if(a>-67.5&&a<67.5)res.push(['RIGHT',1]);if(a>112.5||a<-112.5)res.push(['LEFT',1]);
  if(a>22.5&&a<157.5)res.push(['DOWN',1]);if(a<-22.5&&a>-157.5)res.push(['UP',1]);return res;}
 const el=document.elementFromPoint(x,y),bt=el&&el.closest('[data-b]');
 return bt?[[bt.dataset.b,+(bt.dataset.v||1)]]:[];
}
function same(a,b){return a.length===b.length&&a.every((t,i)=>t[0]===b[i][0]&&t[1]===b[i][1]);}
function setTouch(id,list){const old=touches.get(id)||[];if(same(old,list))return;
 old.forEach(t=>{if(!list.some(n=>n[0]===t[0]&&n[1]===t[1]))release(t[0],t[1]);});
 list.forEach(t=>{if(!old.some(o=>o[0]===t[0]&&o[1]===t[1]))press(t[0],t[1]);});
 if(list.length)touches.set(id,list);else touches.delete(id);}
const ctlArea=[$('ctl'),$('midL'),$('fbar')];
ctlArea.forEach(area=>{
 area.addEventListener('pointerdown',e=>{const l=targetsAt(e.clientX,e.clientY);if(!l.length)return;e.preventDefault();try{area.setPointerCapture(e.pointerId);}catch(_){}setTouch(e.pointerId,l);});
 area.addEventListener('pointermove',e=>{if(!touches.has(e.pointerId))return;e.preventDefault();setTouch(e.pointerId,targetsAt(e.clientX,e.clientY));});
 const end=e=>{setTouch(e.pointerId,[]);};
 area.addEventListener('pointerup',end);area.addEventListener('pointercancel',end);area.addEventListener('lostpointercapture',end);
});
document.addEventListener('contextmenu',e=>e.preventDefault());
document.addEventListener('touchmove',e=>{if(e.touches.length>1||e.target.closest('#ctl,#midL,#fbar'))e.preventDefault();},{passive:false});
// ---- Zoom: Bild bis 4x vergroessern, Ausschnitt verschieben (Transform auf dem Canvas; tPos rechnet ueber getBoundingClientRect weiter richtig)
let zm=1,zx=0,zy=0,fw=640,fh=480;
function applyZoom(){zm=Math.max(1,Math.min(4,zm));zx=Math.max(0,Math.min(fw*(zm-1),zx));zy=Math.max(0,Math.min(fh*(zm-1),zy));
 cv.style.transform=zm>1?'translate('+(-zx)+'px,'+(-zy)+'px) scale('+zm+')':'';$('zl').textContent=Math.round(zm*100)+'%';$('zl').classList.toggle('on',zm>1);}
function zoomAt(nz,cx,cy){   // cx/cy: Punkt im sichtbaren Ausschnitt (CSS-Pixel), der an seiner Stelle bleibt
 nz=Math.max(1,Math.min(4,nz));const bx=(cx+zx)/zm,by=(cy+zy)/zm;zm=nz;zx=bx*zm-cx;zy=by*zm-cy;applyZoom();}
function zoomStep(f){zoomAt(zm*f,fw/2,fh/2);}
$('zi').onclick=()=>zoomStep(1.25);$('zo').onclick=()=>zoomStep(0.8);$('zl').onclick=()=>{zm=1;zx=zy=0;applyZoom();};
$('cvw').addEventListener('wheel',e=>{const r=$('cvw').getBoundingClientRect();
 if(e.ctrlKey||e.metaKey){e.preventDefault();zoomAt(zm*Math.exp(-e.deltaY*0.0025),e.clientX-r.left,e.clientY-r.top);}
 else if(zm>1){e.preventDefault();zx+=e.deltaX;zy+=e.deltaY;applyZoom();}},{passive:false});
addEventListener('keydown',e=>{if(e.ctrlKey||e.metaKey||e.altKey)return;
 if(e.key==='+'||e.code==='NumpadAdd'||e.code==='Equal'){e.preventDefault();zoomStep(1.25);}
 else if(e.key==='-'||e.code==='NumpadSubtract'||e.code==='Minus'){e.preventDefault();zoomStep(0.8);}
 else if(e.code==='Digit0'||e.code==='Numpad0'){e.preventDefault();zm=1;zx=zy=0;applyZoom();}
 else if(e.code==='KeyV'){e.preventDefault();setFull(!document.body.classList.contains('full'));}});
// Zwei Finger, die fast gleichzeitig (< 150 ms) aufs Bild kommen = Zoomen/Verschieben; spaeter dazukommender Finger = normale
// Bedienung (Step halten + Kachel ziehen). Dafuer wird der erste Touch-Finger bis zu 150 ms zurueckgehalten.
const pend=new Map();let gest=null;
function cvwXY(e){const r=$('cvw').getBoundingClientRect();return[e.clientX-r.left,e.clientY-r.top];}
function gestStart(){const p=[...pend.values()];pend.forEach(q=>clearTimeout(q.tm));pend.clear();
 const[a,b]=p;gest={ids:[a.pid,b.pid],pos:{[a.pid]:a.xy,[b.pid]:b.xy}};gest.d0=Math.hypot(a.xy[0]-b.xy[0],a.xy[1]-b.xy[1])||1;gest.z0=zm;
 gest.m0=[(a.xy[0]+b.xy[0])/2,(a.xy[1]+b.xy[1])/2];gest.zx0=zx;gest.zy0=zy;}
function gestMove(){const[a,b]=gest.ids.map(i=>gest.pos[i]);const d=Math.hypot(a[0]-b[0],a[1]-b[1]),m=[(a[0]+b[0])/2,(a[1]+b[1])/2];
 const nz=Math.max(1,Math.min(4,gest.z0*d/gest.d0));const bx=(gest.m0[0]+gest.zx0)/gest.z0,by=(gest.m0[1]+gest.zy0)/gest.z0;   // Bildpunkt unter der Anfangsmitte
 zm=nz;zx=bx*zm-m[0];zy=by*zm-m[1];applyZoom();}
// ---- Touch direkt auf dem Bild: Kacheln, Steps, Spuren, Reiter ... (die Groovebox wertet die Stelle aus)
const tIds=new Map(),tPend=new Map(),rips=new Map();
function tId(pid){if(tIds.has(pid))return tIds.get(pid);for(let i=0;i<10;i++){if(![...tIds.values()].includes(i)){tIds.set(pid,i);return i;}}return -1;}
function tPos(e){const r=cv.getBoundingClientRect();return[Math.max(0,Math.min(W-1,Math.round((e.clientX-r.left)/r.width*W))),Math.max(0,Math.min(H-1,Math.round((e.clientY-r.top)/r.height*H)))];}
function tSend(m){if(ws&&ws.readyState===1)ws.send(m);}
function ripple(e,hold){const r=$('cvw').getBoundingClientRect(),d=document.createElement('div');d.className='rip'+(hold?' hold':'');d.style.left=(e.clientX-r.left)+'px';d.style.top=(e.clientY-r.top)+'px';$('cvw').appendChild(d);if(!hold)setTimeout(()=>d.remove(),460);return d;}
function tDown(e){const id=tId(e.pointerId);if(id<0)return;
 const[x,y]=tPos(e);tSend('t '+id+' '+x+' '+y);ripple(e,false);rips.set(e.pointerId,ripple(e,true));vib();}
function commit(pid){const q=pend.get(pid);if(!q)return;clearTimeout(q.tm);pend.delete(pid);tDown(q.ev);}
cv.addEventListener('pointerdown',e=>{e.preventDefault();try{cv.setPointerCapture(e.pointerId);}catch(_){}
 if(gest)return;
 if(e.pointerType==='touch'&&!tIds.size){   // erster Finger: kurz zurueckhalten (Geste?)
  if(pend.size===1){pend.set(e.pointerId,{pid:e.pointerId,xy:cvwXY(e),ev:e});gestStart();return;}
  const q={pid:e.pointerId,xy:cvwXY(e),ev:e,x0:e.clientX,y0:e.clientY};q.tm=setTimeout(()=>commit(e.pointerId),150);pend.set(e.pointerId,q);return;}
 tDown(e);});
cv.addEventListener('pointermove',e=>{
 if(gest&&gest.pos[e.pointerId]){e.preventDefault();gest.pos[e.pointerId]=cvwXY(e);gestMove();return;}
 const q=pend.get(e.pointerId);if(q){if(Math.hypot(e.clientX-q.x0,e.clientY-q.y0)>10)commit(e.pointerId);else return;}
 if(!tIds.has(e.pointerId))return;e.preventDefault();tPend.set(e.pointerId,tPos(e));const r=rips.get(e.pointerId);
 if(r){const b=$('cvw').getBoundingClientRect();r.style.left=(e.clientX-b.left)+'px';r.style.top=(e.clientY-b.top)+'px';}});
function tEnd(e){
 if(gest&&gest.pos[e.pointerId]){delete gest.pos[e.pointerId];if(!Object.keys(gest.pos).length)gest=null;return;}
 if(pend.has(e.pointerId))commit(e.pointerId);
 if(!tIds.has(e.pointerId))return;const id=tIds.get(e.pointerId),[x,y]=tPos(e);tPend.delete(e.pointerId);tSend('e '+id+' '+x+' '+y);tIds.delete(e.pointerId);
 const r=rips.get(e.pointerId);if(r){r.remove();rips.delete(e.pointerId);}}
cv.addEventListener('pointerup',tEnd);cv.addEventListener('pointercancel',tEnd);
setInterval(()=>{for(const[pid,p]of tPend){const id=tIds.get(pid);if(id!==undefined)tSend('m '+id+' '+p[0]+' '+p[1]);}tPend.clear();},16);   // Bewegung hoechstens ~60x/s
// ---- Tastatur
const km={ArrowUp:['UP',1],ArrowDown:['DOWN',1],ArrowLeft:['LEFT',1],ArrowRight:['RIGHT',1],KeyZ:['A',1],KeyY:['A',1],Space:['A',1],KeyX:['B',1],Backspace:['B',1],Escape:['B',1],
 KeyS:['X',1],KeyA:['Y',1],KeyQ:['L1',1],KeyW:['R1',1],Digit1:['L2',1],Digit2:['R2',1],ShiftLeft:['SELECT',1],ShiftRight:['SELECT',1],Tab:['SELECT',1],
 Enter:['START',1],NumpadEnter:['START',1],KeyT:['LY',1],KeyG:['LY',-1],KeyF:['LX',-1],KeyH:['LX',1],KeyI:['RY',1],KeyK:['RY',-1],KeyJ:['RX',-1],KeyL:['RX',1]};
const kh={};
addEventListener('keydown',e=>{if(e.ctrlKey||e.metaKey||e.altKey)return;const m=km[e.code];if(!m)return;e.preventDefault();if(kh[e.code])return;kh[e.code]=1;press(m[0],m[1]);});
addEventListener('keyup',e=>{const m=km[e.code];if(!m||!kh[e.code])return;e.preventDefault();delete kh[e.code];release(m[0],m[1]);});
addEventListener('blur',()=>{for(const c in kh){release(km[c][0],km[c][1]);delete kh[c];}});
// ---- Gamepad (Standard-Belegung, Tastenlage wie am R36S: rechts A, unten B, oben X, links Y)
const gpMap={1:'A',0:'B',3:'X',2:'Y',4:'L1',5:'R1',6:'L2',7:'R2',8:'SELECT',9:'START',12:'UP',13:'DOWN',14:'LEFT',15:'RIGHT'};
const gpState={};
function pollPad(){const gps=navigator.getGamepads?navigator.getGamepads():[];
 for(const gp of gps){if(!gp)continue;
  for(const i in gpMap){const p=gp.buttons[i]&&gp.buttons[i].pressed,k='g'+gp.index+'b'+i;if(p&&!gpState[k]){gpState[k]=1;press(gpMap[i],1);}else if(!p&&gpState[k]){delete gpState[k];release(gpMap[i],1);}}
  [['LX',0,1],['LY',1,-1],['RX',2,1],['RY',3,-1]].forEach(([n,ai,sg])=>{const a=gp.axes[ai]||0,v=a>0.5?sg:(a<-0.5?-sg:0),k='g'+gp.index+n,o=gpState[k]||0;
   if(v!==o){if(o)release(n,o);if(v)press(n,v);if(v)gpState[k]=v;else delete gpState[k];}});}}
// ---- Kopfzeile
$('bPad').onclick=()=>{document.body.classList.toggle('nopad');try{localStorage.nopad=document.body.classList.contains('nopad')?1:'';}catch(e){}setTimeout(fit,30);};
try{if(localStorage.nopad)document.body.classList.add('nopad');}catch(e){}
$('bHelp').onclick=()=>$('help').classList.add('show');$('help').onclick=()=>$('help').classList.remove('show');
$('bSt').onclick=()=>{document.body.classList.toggle('sticks');try{localStorage.sticks=document.body.classList.contains('sticks')?1:'';}catch(e){}};
try{if(localStorage.sticks)document.body.classList.add('sticks');}catch(e){}
let wl=null;async function wake(){try{if(navigator.wakeLock&&!wl){wl=await navigator.wakeLock.request('screen');wl.addEventListener('release',()=>wl=null);}}catch(e){}}
document.addEventListener('pointerdown',wake);document.addEventListener('keydown',wake);
// Bild-Vollbild: Kopfzeile und Tasten weg, Bild fuellt den Bildschirm (4:3), wo moeglich auch Browser-Vollbild
function setFull(on){document.body.classList.toggle('full',on);try{localStorage.full=on?1:'';}catch(e){}
 const d=document.documentElement,fs=document.fullscreenElement||document.webkitFullscreenElement;
 try{if(on&&!fs&&(d.requestFullscreen||d.webkitRequestFullscreen)){const r=(d.requestFullscreen||d.webkitRequestFullscreen).call(d);if(r&&r.catch)r.catch(()=>{});}
  else if(!on&&fs)(document.exitFullscreen||document.webkitExitFullscreen).call(document);}catch(e){}
 setTimeout(fit,60);setTimeout(fit,400);}
$('bFull').onclick=()=>setFull(true);
$('fExit').addEventListener('pointerdown',e=>{e.stopPropagation();e.preventDefault();setFull(false);});
$('fSide').addEventListener('pointerdown',e=>{e.stopPropagation();e.preventDefault();document.body.classList.toggle('fleft');});
try{if(localStorage.full)document.body.classList.add('full');}catch(e){}
$('bFs').onclick=()=>{const d=document.documentElement;if(document.fullscreenElement||document.webkitFullscreenElement){(document.exitFullscreen||document.webkitExitFullscreen).call(document);}
 else{(d.requestFullscreen||d.webkitRequestFullscreen).call(d);try{screen.orientation.lock('landscape').catch(()=>{});}catch(e){}}};
</script></body></html>)HTML";
