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
