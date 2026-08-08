let streamOpen=false,streamTimer=null;
async function toggleStreamPanel(){if(isBusy||termOpen||canvasOpen)return;if(activeView===19)await leaveMediaForView();streamOpen=!streamOpen;document.getElementById('streamWrap').classList.toggle('open',streamOpen);document.querySelectorAll('.vbtn').forEach(b=>b.classList.toggle('active',streamOpen&&parseInt(b.dataset.v)===21));if(streamOpen){await pollStreamStatus();streamTimer=setInterval(pollStreamStatus,2000);}else if(streamTimer){clearInterval(streamTimer);streamTimer=null;}}
async function closeStreamPanel(){streamOpen=false;if(streamTimer){clearInterval(streamTimer);streamTimer=null;}document.getElementById('streamWrap').classList.remove('open');document.querySelectorAll('.vbtn').forEach(b=>b.classList.remove('active'));}
async function enterStream(){try{const r=await fetch('/stream/enter',{method:'POST'});const j=await r.json();if(!r.ok){toast('stream enter failed: low memory',false);}else{activeView=21;toast('stream mode on');}renderStreamState(j);}catch(e){toast('stream enter failed',false);}}
async function exitStream(){try{const r=await fetch('/stream/exit',{method:'POST'});const j=await r.json();activeView=null;renderStreamState(j);toast('stream off');}catch(e){toast('stream exit failed',false);}}
async function pollStreamStatus(){try{const r=await fetch('/stream/status',{cache:'no-store'});renderStreamState(await r.json());}catch(e){}}
function renderStreamState(j){const el=document.getElementById('streamState');if(!el)return;if(!j.active){el.textContent='OFF';}else if(j.connected){el.textContent='STREAMING '+j.fps.toFixed(1)+' FPS';}else{el.textContent='WAITING PC';}const enterBtn=document.getElementById('streamEnter'),exitBtn=document.getElementById('streamExit');if(enterBtn)enterBtn.disabled=!!j.active;if(exitBtn)exitBtn.disabled=!j.active;}
let activeView=0,termOpen=false,canvasOpen=false,arcadeOpen=false,dinoOpen=false,dinoPollTimer=null,dinoJumpPending=false,sokobanOpen=false,sokobanMovePending=false,sokobanStateData=null,arcadeGameOpen='',arcadeGameState=null,arcadeGamePending=false,arcadeGamePollTimer=null,breakoutPositionTimer=null,wifiSetupOpen=false,wifiConnecting=false,lastWifiStatus=null,blOn=true,claudeStatusOn=true,displayTheme=1,fontStyle='pixel',isBusy=false,drawing=false,initialLoadComplete=false,weatherSearchTimer=null,weatherSearchSeq=0;
let timetable={schemaVersion:1,school:'GDUFS',termStart:'',courses:[]},editingCourse=-1,timetableStatus=null;
let importSource='',importDraft=null;
let weatherLocation={label:'',source:'ip',override:false};
const graduateCourseNames={
'数据结构':['DATA STRUCTURES','DATA STRUCT.'],'高级数据结构':['DATA STRUCTURES','DATA STRUCT.'],
'算法设计与分析':['ALGORITHM DESIGN','ALGORITHMS'],'高级算法设计与分析':['ADV. ALGORITHMS','ALGORITHMS'],
'机器学习':['MACHINE LEARNING','ML'],'深度学习':['DEEP LEARNING','DL'],
'强化学习':['REINFORCEMENT LEARN.','RL'],'自然语言处理':['NATURAL LANGUAGE PROC.','NLP'],
'统计自然语言处理':['STATISTICAL NLP','STAT. NLP'],'计算机视觉':['COMPUTER VISION','CV'],
'高级人工智能技术':['ADVANCED AI','ADV. AI'],'知识图谱及应用':['KNOWLEDGE GRAPHS','KG'],
'信息检索与抽取':['INFO RETRIEVAL','IR'],'数据挖掘进阶':['ADV. DATA MINING','DATA MINING'],
'大数据原理与实践':['BIG DATA','BIG DATA'],'社会计算与社交网络分析':['SOCIAL COMPUTING','SOCIAL COMP.'],
'高级操作系统':['ADV. OPERATING SYS.','ADV. OS'],'分布式系统':['DISTRIBUTED SYSTEMS','DIST. SYSTEMS'],
'高级计算机网络':['ADV. NETWORKS','NETWORKS'],'高级数据库系统':['ADV. DATABASES','DATABASES'],
'软件服务工程与实践':['SERVICE ENGINEERING','SERVICE ENG.'],'高级软件工程':['ADV. SOFTWARE ENG.','ADV. SE'],
'网络安全技术':['NETWORK SECURITY','NET SECURITY'],'网络攻击与防御':['NETWORK DEFENSE','NET DEFENSE'],
'现代密码学':['MODERN CRYPTOGRAPHY','CRYPTOGRAPHY'],'网络取证与实践':['NETWORK FORENSICS','NET FORENSICS'],
'漏洞挖掘与实践':['VULNERABILITY MINING','VULN. MINING'],'区块链技术与实践':['BLOCKCHAIN','BLOCKCHAIN'],
'论文写作指导':['THESIS WRITING','THESIS'],'工程伦理':['ENGINEERING ETHICS','ENG. ETHICS'],
'工程实践与科研训练':['RESEARCH PRACTICE','RESEARCH'],'前沿讲座':['FRONTIER LECTURES','FRONTIERS']
};
Object.assign(graduateCourseNames,{
'药用植物资源综合利用':['COMPREHENSIVE UTILIZATION OF MEDICINAL PLANT RESOURCES','MED PLANT RESOURCES'],
'中学生物学课程标准与教材分析':['BIOLOGY CURRICULUM STANDARDS AND TEXTBOOK ANALYSIS','BIO CURRICULUM'],
'德育与班级管理':['MORAL EDUCATION AND CLASS MANAGEMENT','CLASS MANAGEMENT'],
'教育心理学':['EDUCATIONAL PSYCHOLOGY','EDU PSYCHOLOGY'],
'现代生物技术概论':['INTRODUCTION TO MODERN BIOTECHNOLOGY','MODERN BIOTECH'],
'生物信息学与基因组学':['BIOINFORMATICS AND GENOMICS','BIOINFO & GENOMICS'],
'园艺植物栽培学':['HORTICULTURAL PLANT CULTIVATION','HORTICULTURE'],
'就业指导':['CAREER GUIDANCE','CAREER GUIDANCE'],
'形势与政策':['CURRENT AFFAIRS AND POLICY','AFFAIRS & POLICY'],
'食品营养与卫生':['FOOD NUTRITION AND HYGIENE','FOOD NUTRITION']
});
const coursePhraseNames={
'现代':'MODERN','高级':'ADVANCED','基础':'FUNDAMENTALS','概论':'INTRODUCTION','导论':'INTRODUCTION',
'计算机':'COMPUTER','软件':'SOFTWARE','工程':'ENGINEERING','技术':'TECHNOLOGY','系统':'SYSTEMS',
'数据':'DATA','结构':'STRUCTURES','算法':'ALGORITHMS','设计':'DESIGN','分析':'ANALYSIS',
'人工智能':'ARTIFICIAL INTELLIGENCE','机器学习':'MACHINE LEARNING','深度学习':'DEEP LEARNING',
'网络':'NETWORKS','安全':'SECURITY','数据库':'DATABASES','程序设计':'PROGRAMMING',
'生物技术':'BIOTECHNOLOGY','生物信息学':'BIOINFORMATICS','基因组学':'GENOMICS',
'植物':'PLANTS','资源':'RESOURCES','综合利用':'UTILIZATION','课程标准':'CURRICULUM STANDARDS',
'教材分析':'TEXTBOOK ANALYSIS','教育心理学':'EDUCATIONAL PSYCHOLOGY','班级管理':'CLASS MANAGEMENT',
'食品':'FOOD','营养':'NUTRITION','卫生':'HYGIENE','实践':'PRACTICE','专题':'TOPICS','研究':'RESEARCH'
};
const courseMappingStorageKey='mochi.courseMappings.v1';
const testMode=new URLSearchParams(location.search).has('test');
const consoleViewMeta={
  control:['DEVICE CONTROL.','Set the face, brightness and display language.'],
  modules:['DISPLAY MODULES.','Choose what Mochi shows next.'],
  workspace:['ACTIVITY.','Open live panels, timers and device previews.'],
  setup:['SETUP.','Personalize Mochi and connect it to your network.'],
  system:['SYSTEM.','Drawing, terminal and firmware tools.']
};
function setConsoleView(view){
  if(!consoleViewMeta[view])return;
  document.querySelectorAll('[data-console-section]').forEach(section=>section.classList.toggle('console-view-active',section.dataset.consoleSection===view));
  document.querySelectorAll('.console-nav button').forEach(button=>{const active=button.dataset.consoleView===view;button.classList.toggle('active',active);if(active)button.setAttribute('aria-current','page');else button.removeAttribute('aria-current');});
  const meta=consoleViewMeta[view],title=document.getElementById('consoleViewTitle'),subtitle=document.getElementById('consoleViewSubtitle');
  if(title)title.textContent=meta[0];
  if(subtitle)subtitle.textContent=meta[1];
  document.querySelector('.console-content')?.scrollTo({top:0,behavior:'smooth'});
}
document.querySelectorAll('[data-console-view]').forEach(button=>button.addEventListener('click',()=>setConsoleView(button.dataset.consoleView)));
setConsoleView('control');
let expressionState={mode:'manual',selected:'normal',rendered:'normal'};
let bgPreviewHex='#aa4818',bgPreviewRevision=0,bgPreviewTimer=0,bgPreviewInFlight=false;
let lastX=0,lastY=0,tt;
let marketSelected=[],marketDirectory=[],marketLoaded=false,marketSaving=false,marketSaveQueued=false,marketUpdatedAt=null,marketDrag=null;
let stockSelected=[],stockSaving=false,stockSaveQueued=false,stockUpdatedAt=null,stockDrag=null,stockSearchTimer=null,stockSearchSeq=0;
let carouselConfig={enabled:false,speed:12,order:[8,9,10,6,17,7,18],fixed:8},carouselSpeedTimer=null,carouselPanelOpen=false,carouselDrag=null;
let salaryState=null,salaryConfigured=false,salarySettingsOpen=false,salaryRequestPending=false,lastSalaryStatus=null,salaryMotionBase=0,salaryMotionAt=0;
function toast(msg,ok=true){const el=document.getElementById('toast');el.textContent=msg;el.style.borderColor=ok?'#28b878':'#c96a3e';el.classList.add('show');clearTimeout(tt);tt=setTimeout(()=>el.classList.remove('show'),1300);}
function setBusy(b){isBusy=b;document.getElementById('busy').classList.toggle('show',b);const locked=b||termOpen;document.querySelectorAll('.vbtn').forEach(el=>{el.disabled=canvasOpen?parseInt(el.dataset.v)!==3:locked;});document.querySelectorAll('.ebtn').forEach(el=>{el.disabled=locked||canvasOpen;});document.querySelectorAll('.cbtn').forEach(el=>{if(el.id!=='blBtn'&&el.id!=='ccStatusBtn')el.disabled=locked;});}
async function req(path){try{const r=await fetch(path);return r.ok;}catch(e){toast('no connection',false);return false;}}
function renderWeatherLocation(j){
  weatherLocation={label:j.label||j.city||'',source:j.source||'ip',override:j.override===true};
  const source=document.getElementById('weatherLocationSource'),current=document.getElementById('weatherLocationCurrent');
  if(source)source.textContent=weatherLocation.source==='gps'?'GPS LOCATION':(weatherLocation.source==='manual'?'CUSTOM LOCATION':'IP AUTO');
  if(current)current.textContent=weatherLocation.label?(weatherLocation.label+' · '+(weatherLocation.source==='ip'?'detected automatically':'saved on device')):'IP location will be used after WiFi connects.';
}
async function loadWeatherLocation(){try{const r=await fetch('/weather/location',{cache:'no-store'});if(r.ok)renderWeatherLocation(await r.json());}catch(e){}}
async function saveWeatherLocation(lat,lon,city,source){
  const params=new URLSearchParams({lat:String(lat),lon:String(lon),city:city||'LOCATION',source});
  try{const r=await fetch('/weather/location?'+params.toString(),{method:'POST',cache:'no-store'});const j=await r.json();if(!r.ok)throw new Error(j.error||'location save failed');renderWeatherLocation(j);document.getElementById('weatherSuggestions').innerHTML='';document.getElementById('weatherLocationSearch').value='';toast('weather location saved');}
  catch(e){toast(e.message||'weather location save failed',false);}
}
function renderWeatherSuggestions(results){const out=document.getElementById('weatherSuggestions');out.innerHTML='';results.forEach(item=>{const button=document.createElement('button');button.type='button';button.className='weather-suggestion';const name=document.createElement('span');name.textContent=item.name||'LOCATION';const detail=document.createElement('small');detail.textContent=[item.admin1,item.country].filter(Boolean).join(' · ');button.append(name,detail);button.onclick=()=>saveWeatherLocation(item.latitude,item.longitude,item.name||'LOCATION','manual');out.appendChild(button);});}
async function searchWeatherLocations(){const input=document.getElementById('weatherLocationSearch'),query=input.value.trim(),out=document.getElementById('weatherSuggestions');if(query.length<1){out.innerHTML='';return;}const seq=++weatherSearchSeq;out.textContent='SEARCHING...';try{const r=await fetch('https://geocoding-api.open-meteo.com/v1/search?name='+encodeURIComponent(query)+'&count=8&language=zh&format=json',{cache:'no-store'});const j=await r.json();if(seq!==weatherSearchSeq)return;renderWeatherSuggestions(Array.isArray(j.results)?j.results:[]);}catch(e){if(seq===weatherSearchSeq)out.textContent='SEARCH UNAVAILABLE';}}
async function locateWeatherByGps(){const button=document.getElementById('weatherGpsBtn');if(!window.isSecureContext&&!['localhost','127.0.0.1'].includes(location.hostname)){toast('GPS requires a secure browser page',false);return;}if(!navigator.geolocation){toast('browser GPS unavailable',false);return;}button.disabled=true;button.textContent='LOCATING...';navigator.geolocation.getCurrentPosition(async position=>{const lat=position.coords.latitude,lon=position.coords.longitude;let city='GPS LOCATION';try{const r=await fetch('https://nominatim.openstreetmap.org/reverse?format=jsonv2&lat='+encodeURIComponent(lat)+'&lon='+encodeURIComponent(lon)+'&zoom=10&accept-language=zh-CN',{headers:{Accept:'application/json'}});const j=await r.json();const a=j.address||{};city=a.city||a.town||a.municipality||a.county||city;}catch(e){}await saveWeatherLocation(lat,lon,city,'gps');button.disabled=false;button.textContent='⌖ AUTO LOCATE';},error=>{button.disabled=false;button.textContent='⌖ AUTO LOCATE';toast(error.code===1?'GPS permission denied':'GPS location failed',false);},{enableHighAccuracy:true,timeout:12000,maximumAge:300000});}
async function resetWeatherToIp(){try{const r=await fetch('/weather/location/reset',{method:'POST',cache:'no-store'});const j=await r.json();if(!r.ok)throw new Error();renderWeatherLocation(j);toast('IP auto location enabled');}catch(e){toast('IP location reset failed',false);}}
document.getElementById('weatherLocationSearch').addEventListener('input',()=>{clearTimeout(weatherSearchTimer);weatherSearchTimer=setTimeout(searchWeatherLocations,280);});
async function waitNotBusy(){for(let i=0;i<100;i++){try{const r=await fetch('/state');const j=await r.json();if(!j.busy)return;}catch(e){}await new Promise(r=>setTimeout(r,150));}}
function scheduleBgPreview(){if(bgPreviewTimer)return;bgPreviewTimer=setTimeout(()=>{bgPreviewTimer=0;flushBgPreview();},24);}
async function flushBgPreview(){
  if(bgPreviewInFlight)return;
  bgPreviewInFlight=true;
  const revision=bgPreviewRevision,hex=bgPreviewHex;
  if(canvasOpen){await req('/draw/clear?bg='+encodeURIComponent(hex));await req('/prefs?bg='+encodeURIComponent(hex));}
  else await req('/redraw?bg='+encodeURIComponent(hex));
  bgPreviewInFlight=false;
  if(revision!==bgPreviewRevision)scheduleBgPreview();
}
function onBgChange(hex){
  if(!/^#[0-9a-f]{6}$/i.test(hex))return;
  bgPreviewHex=hex;bgPreviewRevision++;
  const mediaBg=document.getElementById('mediaBg');
  if(mediaBg)mediaBg.value=hex;
  redrawCanvas(hex);
  if(typeof redrawSelectedMedia==='function')redrawSelectedMedia();
  scheduleBgPreview();
}
function applyExpressionState(state){
  expressionState={mode:state.mode==='auto'?'auto':'manual',selected:state.selected||'normal',rendered:state.rendered||state.selected||'normal'};
  const shown=expressionState.mode==='auto'?expressionState.rendered:expressionState.selected;
  document.getElementById('exprCurrent').textContent=shown.charAt(0).toUpperCase()+shown.slice(1);
  document.getElementById('exprMode').textContent=expressionState.mode;
  const activityCurrent=document.getElementById('activityCurrent'),activityDisplay=document.getElementById('activityDisplay');
  if(activityCurrent)activityCurrent.textContent=shown.charAt(0).toUpperCase()+shown.slice(1)+' expression';
  if(activityDisplay)activityDisplay.textContent=shown.charAt(0).toUpperCase()+shown.slice(1);
  const auto=document.getElementById('exprAuto'),enabled=expressionState.mode==='auto';
  auto.textContent=enabled?'● auto on':'○ auto off';auto.classList.toggle('on',enabled);auto.classList.toggle('dim',!enabled);
  document.querySelectorAll('.ebtn').forEach(button=>button.classList.toggle('active',!enabled&&button.dataset.expression===expressionState.selected));
}
async function loadExpressions(){try{const r=await fetch('/expressions',{cache:'no-store'});if(!r.ok)throw new Error();applyExpressionState(await r.json());}catch(e){applyExpressionState(expressionState);}}
async function updateExpression(payload){try{const r=await fetch('/expression',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});if(!r.ok)throw new Error();applyExpressionState(await r.json());return true;}catch(e){toast('expression update failed',false);return false;}}
async function selectExpression(id){if(isBusy||termOpen||canvasOpen)return;if(activeView===19)await leaveMediaForView();if(await updateExpression({id})){activeView=0;document.querySelectorAll('.vbtn').forEach(button=>button.classList.remove('active'));toast(id+' expression');}}
async function toggleExpressionAuto(){if(isBusy||termOpen||canvasOpen)return;const mode=expressionState.mode==='auto'?'manual':'auto';if(await updateExpression({mode}))toast(mode==='auto'?'auto on':'manual expression');}
function applyProfile(p){const name=p.deviceName||'MOCHI';document.getElementById('profileName').value=name;document.getElementById('profileBoot1').value=p.bootLine1??'HELLO';document.getElementById('profileBoot2').value=p.bootLine2??'MOCHI';document.getElementById('profileExpression').value=p.defaultExpression||'normal';document.getElementById('profileMode').value=p.expressionMode==='auto'?'auto':'manual';document.querySelector('.sitename').textContent=name+' · CONTROLLER';const consoleName=document.getElementById('consoleDeviceName');if(consoleName)consoleName.textContent=name;}
function toggleProfilePanel(){const panel=document.getElementById('profileWrap'),open=!panel.classList.contains('open');panel.classList.toggle('open',open);document.getElementById('profilePanelState').textContent=open?'close':'open';document.getElementById('profilePanelBtn').setAttribute('aria-expanded',String(open));}
async function loadProfile(){try{const r=await fetch('/profile',{cache:'no-store'});if(!r.ok)throw new Error();applyProfile(await r.json());}catch(e){toast('profile load failed',false);}}
async function saveProfile(){const payload={deviceName:document.getElementById('profileName').value,bootLine1:document.getElementById('profileBoot1').value,bootLine2:document.getElementById('profileBoot2').value,defaultExpression:document.getElementById('profileExpression').value,expressionMode:document.getElementById('profileMode').value};try{const r=await fetch('/profile',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});const p=await r.json();if(!r.ok)throw new Error(p.error||'save failed');applyProfile(p);await loadExpressions();toast('profile saved');}catch(e){toast(e.message||'profile save failed',false);}}
async function resetProfile(){try{const r=await fetch('/profile/reset',{method:'POST'});if(!r.ok)throw new Error();applyProfile(await r.json());await loadExpressions();toast('profile defaults restored');}catch(e){toast('profile reset failed',false);}}
async function exportConfig(){try{const r=await fetch('/config/export',{cache:'no-store'});if(!r.ok)throw new Error();const blob=await r.blob(),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='clawd-mochi-config.json';a.click();URL.revokeObjectURL(url);toast('config exported');}catch(e){toast('config export failed',false);}}
async function importConfig(file){if(!file)return;try{const text=await file.text();JSON.parse(text);const r=await fetch('/config/import',{method:'POST',headers:{'Content-Type':'application/json'},body:text});const result=await r.json();if(!r.ok)throw new Error(result.error||'invalid config');await loadProfile();await loadPrefs();await loadExpressions();toast('config imported');}catch(e){toast(e.message||'config import failed',false);}finally{document.getElementById('configFile').value='';}}
function renderOta(j){if(!j)return;const state=String(j.state||'idle').replace('_',' ').toUpperCase();document.getElementById('otaState').textContent=state;document.getElementById('otaVersion').textContent=j.version||'--';document.getElementById('otaLatest').textContent=j.latestVersion||'--';document.getElementById('otaNotes').textContent=j.releaseNotes||'No release notes';document.getElementById('otaInstallBtn').disabled=!j.available;}
async function loadOta(){try{const r=await fetch('/ota/status',{cache:'no-store'});if(r.ok)renderOta(await r.json());}catch(e){}}
async function checkOta(){const b=document.getElementById('otaCheckBtn');b.disabled=true;b.textContent='CHECKING...';try{const r=await fetch('/ota/check',{method:'POST'});renderOta(await r.json());}catch(e){toast('OTA check failed',false);}finally{b.disabled=false;b.textContent='CHECK NOW';}}
async function installOta(){if(!confirm('Install the available update and restart the device?'))return;const b=document.getElementById('otaInstallBtn');b.disabled=true;b.textContent='INSTALLING...';try{const r=await fetch('/ota/install',{method:'POST'});if(!r.ok)throw new Error();toast('device is restarting');}catch(e){toast('OTA install failed',false);loadOta();}}
async function uploadOta(file){if(!file)return;if(!confirm('Upload '+file.name+' and restart the device?'))return;try{const form=new FormData();form.append('file',file,file.name);const r=await fetch('/ota/upload',{method:'POST',body:form});const j=await r.json();if(!r.ok)throw new Error(j.error||'upload failed');toast('device is restarting');}catch(e){toast(e.message||'OTA upload failed',false);}finally{document.getElementById('otaFirmwareFile').value='';document.getElementById('otaFsFile').value='';}}
async function setView(v){if(isBusy||termOpen||canvasOpen)return;if(activeView===19)await leaveMediaForView();if(v===3){toggleCanvas();return;}const keys={0:'w',1:'s',2:'d',6:'c',7:'p',8:'e',9:'m',10:'k',17:'y',18:'u'};if(!await req('/cmd?k='+keys[v]))return;activeView=v;document.querySelectorAll('.vbtn').forEach(b=>b.classList.toggle('active',parseInt(b.dataset.v)===v));document.getElementById('pwrap').classList.toggle('open',v===7);document.getElementById('ywrap').classList.toggle('open',v===17);document.getElementById('mwrap').classList.toggle('open',v===9);document.getElementById('swrap').classList.toggle('open',v===10);document.getElementById('uwrap').classList.toggle('open',v===18);document.getElementById('termPanel').classList.toggle('open',v===2);if(v===18){await Promise.all([loadTimetable(),pollTimetableStatus()]);toast('timetable open');return;}if(v===17){await loadSalaryConfig();await pollSalary();toast('live ledger open');return;}if(v===9){await loadMarketConfig();loadMarketDirectory();toast('crypto open');return;}if(v===10){await loadStockConfig();toast('market open');return;}if(v===2){termOpen=true;document.getElementById('twrap').classList.add('open');setBusy(false);setBusy(false);document.querySelectorAll('.vbtn,.lbtn').forEach(b=>b.disabled=true);document.getElementById('tin').focus();toast('terminal open');return;}if(v===6||v===7||v===8){toast(v===6?'clock open':(v===7?'pomodoro open':'locating weather'));return;}setBusy(true);await waitNotBusy();setBusy(false);}
function timetableDay(day){return['','MON','TUE','WED','THU','FRI','SAT','SUN'][Number(day)]||'---';}
function timetablePreviewNode(className,text){const node=document.createElement('span');node.className=className;if(text!==undefined)node.textContent=text;return node;}
function splitTimetableCourse(course){const name=String(course.name||'UNTITLED CLASS'),fits=(value,size)=>value.length*size*.6<=212;if(fits(name,27))return{name,size:4};for(let i=name.length-1;i>0;i--){if(name[i]===' '&&fits(name.slice(0,i),27)&&fits(name.slice(i+1),27))return{name:name.slice(0,i)+'\n'+name.slice(i+1),size:4};}const shortName=String(course.shortName||name);return{name:shortName,size:fits(shortName,21)?3:2};}
function timetableDuration(course){const minutes=value=>{const parts=String(value||'').split(':');return Number(parts[0]||0)*60+Number(parts[1]||0);};return Math.max(1,minutes(course.end)-minutes(course.start));}
function renderTimetablePreview(status){
  status=status||{state:'not_configured'};timetableStatus=status;const preview=document.getElementById('utPreview');preview.innerHTML='';
  const weekday=timetableDay(status.weekday||(status.course&&status.course.day)),week=String(Number(status.week)||0).padStart(2,'0');
  const header=status.state==='in_class'?'IN CLASS':(status.state==='next_class'?'NEXT CLASS':(status.state==='not_configured'?'TIMETABLE':'TODAY'));
  preview.append(timetablePreviewNode('utp-head',header),timetablePreviewNode('utp-week',weekday+' · W'+week),timetablePreviewNode('utp-rule'));
  if(status.state==='not_configured'){
    preview.append(timetablePreviewNode('utp-setup1','NO SCHEDULE'),timetablePreviewNode('utp-setup2','OPEN CONTROLLER'),timetablePreviewNode('utp-setup3','TO IMPORT CLASSES'));return;
  }
  if(status.state==='next_class'||status.state==='in_class'){
    const course=status.course||{},remaining=Math.max(0,Number(status.minutesRemaining)||0),inClass=status.state==='in_class',split=splitTimetableCourse(course);
    const courseName=timetablePreviewNode('utp-course size'+split.size,split.name);courseName.style.whiteSpace='pre-line';
    preview.append(timetablePreviewNode('utp-time',(course.start||'00:00')+' - '+(course.end||'00:00')),courseName,
      timetablePreviewNode('utp-detail',(course.room||'TBA')+(course.teacher?' / '+course.teacher:'')),timetablePreviewNode('utp-midrule'),
      timetablePreviewNode('utp-count-label',inClass?'ENDS IN':'STARTS IN'),timetablePreviewNode('utp-count',remaining+' MIN'));
    const progress=document.createElement('div');progress.className='utp-progress';const fill=document.createElement('i');fill.style.width=(inClass?Math.max(0,Math.min(100,(timetableDuration(course)-remaining)/timetableDuration(course)*100)):0)+'%';progress.appendChild(fill);preview.appendChild(progress);
    preview.appendChild(timetablePreviewNode('utp-footer',(inClass?Math.max(0,(Number(status.remainingToday)||0)-1):Number(status.remainingToday)||0)+' MORE TODAY'));return;
  }
  if(status.state==='all_done'){
    const stamp=document.createElement('div');stamp.className='utp-stamp';preview.append(stamp,timetablePreviewNode('utp-empty-line1','ALL CLASSES'),timetablePreviewNode('utp-empty-line2','COMPLETE'),timetablePreviewNode('utp-lower-rule'),timetablePreviewNode('utp-today','TODAY'),timetablePreviewNode('utp-total',(Number(status.todayCompleted)||0)+' / '+(Number(status.todayTotal)||0)));
  }else{
    const calendar=document.createElement('div');calendar.className='utp-calendar';preview.append(calendar,timetablePreviewNode('utp-ring one'),timetablePreviewNode('utp-ring two'),timetablePreviewNode('utp-empty-line1','NO CLASSES'),timetablePreviewNode('utp-empty-line2','TODAY'),timetablePreviewNode('utp-next-rule'));
  }
  if(status.hasNextCourse&&status.nextCourse){const next=status.nextCourse;preview.append(timetablePreviewNode('utp-next','NEXT · '+timetableDay(next.day)+' '+(next.start||'--:--')),timetablePreviewNode('utp-next-name',next.shortName||next.name||'UNTITLED CLASS'),timetablePreviewNode('utp-next-room',next.room||'TBA'));}
}
function renderTimetable(){
  const list=document.getElementById('uList');list.innerHTML='';document.getElementById('uTermStart').value=timetable.termStart||'';
  const courses=[...(timetable.courses||[])].sort((a,b)=>Number(a.day)-Number(b.day)||String(a.start).localeCompare(String(b.start)));
  document.getElementById('uState').textContent=courses.length?courses.length+' RULES':'NOT SET';
  if(!courses.length){list.innerHTML='<div class="uempty">NO CLASSES YET<br>IMPORT FROM GDUFS OR ADD ONE MANUALLY</div>';return;}
  courses.forEach(course=>{const realIndex=timetable.courses.indexOf(course),row=document.createElement('div');row.className='ucourse';
    const name=document.createElement('strong');name.textContent=course.displayName||course.englishName||'UNTITLED CLASS';
    const meta=document.createElement('small');meta.textContent=timetableDay(course.day)+' · '+course.start+'-'+course.end+' · W'+(course.weeks||'ALL')+' · '+(course.room||'TBA');
    const edit=document.createElement('button');edit.type='button';edit.textContent='EDIT';edit.onclick=()=>openCourseForm(realIndex);row.append(name,meta,edit);list.appendChild(row);});
}
async function loadTimetable(){try{const r=await fetch('/timetable',{cache:'no-store'});if(!r.ok)throw new Error();timetable=await r.json();if(!Array.isArray(timetable.courses))timetable.courses=[];renderTimetable();}catch(e){toast('timetable unavailable',false);}}
async function pollTimetableStatus(){if(activeView!==18)return;try{const r=await fetch('/timetable/status',{cache:'no-store'}),j=await r.json();if(r.ok)renderTimetablePreview(j);}catch(e){}}
async function saveTimetable(){timetable.termStart=document.getElementById('uTermStart').value;if(!timetable.termStart){toast('set the first Monday of term',false);return false;}try{const r=await fetch('/timetable',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(timetable)}),j=await r.json();if(!r.ok)throw new Error(j.error||'save failed');renderTimetable();await pollTimetableStatus();toast('timetable saved');return true;}catch(e){toast(e.message||'timetable save failed',false);return false;}}
function cleanCourseName(name){return String(name||'').trim().replace(/[（(]\s*(?:分组|实验|实践)?\s*0*\d+\s*[）)]/g,'').trim();}
function loadCourseMappings(){try{const value=JSON.parse(localStorage.getItem(courseMappingStorageKey)||'{}');return value&&typeof value==='object'?value:{}}catch(e){return{}}}
function rememberCourseMapping(source,english){const key=cleanCourseName(source),value=String(english||'').trim();if(!key||!value)return;const saved=loadCourseMappings();saved[key]=value;try{localStorage.setItem(courseMappingStorageKey,JSON.stringify(saved))}catch(e){}}
function makeCourseShortName(name){const upper=String(name||'').trim().toUpperCase().replace(/[^A-Z0-9&+ ./-]/g,'').replace(/\s+/g,' ');if(upper.length<=22)return upper;const ignored=new Set(['TO','OF','THE','AND','FOR','IN','WITH','INTRODUCTION']);let compact=upper.split(' ').filter(Boolean).map(word=>ignored.has(word)?(word==='AND'?'&':''):word.length>8?word.slice(0,6)+'.':word).filter(Boolean).join(' ');if(compact.length<=22)return compact;const initials=upper.split(' ').filter(word=>word&&!ignored.has(word)).map(word=>word[0]).join('');return(initials.length>=2&&initials.length<=10?initials:compact.slice(0,22)).trim();}
function composeCourseName(source){let rest=cleanCourseName(source),result=[];const phrases=Object.entries(coursePhraseNames).sort((a,b)=>b[0].length-a[0].length);while(rest){let found=false;for(const [cn,en] of phrases){if(rest.startsWith(cn)){result.push(en);rest=rest.slice(cn.length);found=true;break;}}if(!found){if(/^[与和及、]/.test(rest)){result.push('AND');rest=rest.slice(1);continue;}return'';}}return result.join(' ').replace(/\bAND AND\b/g,'AND');}
function lookupCourseName(source){const clean=cleanCourseName(source),saved=loadCourseMappings()[clean],mapped=graduateCourseNames[clean]||graduateCourseNames[String(source||'').trim()];if(saved)return{englishName:saved,displayName:saved.toUpperCase(),shortName:makeCourseShortName(saved),mappingSource:'user'};if(mapped)return{englishName:mapped[0],displayName:mapped[0],shortName:mapped[1],mappingSource:'dictionary'};const composed=composeCourseName(clean);if(composed)return{englishName:composed,displayName:composed,shortName:makeCourseShortName(composed),mappingSource:'phrases'};return null;}
function mapCourseName(){const cn=document.getElementById('uChinese').value.trim(),mapped=lookupCourseName(cn);if(mapped&&!document.getElementById('uEnglish').value.trim())document.getElementById('uEnglish').value=mapped.englishName;}
function openCourseForm(index=-1){editingCourse=index;const c=index>=0?timetable.courses[index]:{};document.getElementById('uChinese').value=c.sourceName||'';document.getElementById('uEnglish').value=c.displayName||'';document.getElementById('uDay').value=String(c.day||1);document.getElementById('uWeeks').value=c.weeks||'1-18';document.getElementById('uStart').value=c.start||'08:00';document.getElementById('uEnd').value=c.end||'09:40';document.getElementById('uRoom').value=c.room||'';document.getElementById('uTeacher').value=c.teacher||'';document.getElementById('uDelete').style.display=index>=0?'block':'none';document.getElementById('uForm').classList.add('open');}
function closeCourseForm(){editingCourse=-1;document.getElementById('uForm').classList.remove('open');}
async function deleteCourse(){if(editingCourse<0)return;const name=timetable.courses[editingCourse]?.displayName||'this class';if(!confirm('Delete '+name+'?'))return;timetable.courses.splice(editingCourse,1);if(await saveTimetable())closeCourseForm();}
async function commitCourse(){mapCourseName();const cn=document.getElementById('uChinese').value.trim(),rawEnglish=document.getElementById('uEnglish').value.trim(),en=rawEnglish.toUpperCase();if(!en){toast('enter or map an English name',false);return;}const course={sourceName:cn,englishName:rawEnglish,displayName:en,shortName:makeCourseShortName(en),mappingSource:'user',userConfirmed:true,day:Number(document.getElementById('uDay').value),weeks:document.getElementById('uWeeks').value.trim(),start:document.getElementById('uStart').value,end:document.getElementById('uEnd').value,room:document.getElementById('uRoom').value.trim().toUpperCase(),teacher:document.getElementById('uTeacher').value.trim().toUpperCase()};if(!course.start||!course.end||course.end<=course.start){toast('check class start and end time',false);return;}rememberCourseMapping(cn,rawEnglish);if(editingCourse>=0)timetable.courses[editingCourse]=course;else timetable.courses.push(course);if(await saveTimetable())closeCourseForm();}
function openTimetableImporter(){importSource='';importDraft=null;document.getElementById('uImporter').classList.add('open');document.getElementById('uPreview').classList.remove('open');document.getElementById('uImportStatus').classList.remove('show');document.body.style.overflow='hidden';}
function closeTimetableImporter(){document.getElementById('uImporter').classList.remove('open');document.body.style.overflow='';}
function selectImportSource(source){importSource=source;document.querySelectorAll('.usource').forEach(b=>b.classList.toggle('active',b.dataset.source===source));const guides={wakeup:'In WakeUp, share the timetable and paste the complete message or its share code here. The temporary code is used once and is never saved.',xiaoai:'In XiaoAi, choose Share timetable and paste the official i.ai.mi.com link.',ics:'Choose an .ics file exported by your calendar or paste an HTTPS calendar URL.',file:'Choose an exported JSON, CSV, HTML, or ICS timetable file.'};document.getElementById('uImportGuide').textContent=guides[source];if(source==='file'||source==='ics')document.getElementById('uWizardFile').click();}
function setImportStatus(message,error=false){const el=document.getElementById('uImportStatus');el.textContent=message;el.style.borderColor=error?'#c96a3e':'#28b878';el.classList.add('show');}
function unfoldIcs(text){return text.replace(/\r?\n[ \t]/g,'');}
function icsValue(block,key){const line=block.split(/\r?\n/).find(v=>v.startsWith(key+':')||v.startsWith(key+';'));return line?line.slice(line.indexOf(':')+1).replace(/\\n/g,' ').replace(/\\,/g,',').trim():'';}
function parseIcsDate(value){const clean=value.replace(/[^0-9TZ]/g,'');if(clean.length<8)return null;const y=+clean.slice(0,4),m=+clean.slice(4,6)-1,d=+clean.slice(6,8),h=+(clean.slice(9,11)||0),min=+(clean.slice(11,13)||0);return new Date(y,m,d,h,min);}
function mondayOf(date){const d=new Date(date.getFullYear(),date.getMonth(),date.getDate()),day=d.getDay()||7;d.setDate(d.getDate()-day+1);return d;}
function dateIso(date){return date.getFullYear()+'-'+String(date.getMonth()+1).padStart(2,'0')+'-'+String(date.getDate()).padStart(2,'0');}
function normalizeTermStart(value){if(value===null||value===undefined||value==='')return'';let date=null;if(typeof value==='number'||/^\d{10,13}$/.test(String(value).trim())){let stamp=Number(value);if(stamp<1e12)stamp*=1000;date=new Date(stamp);}else{const match=String(value).match(/(20\d{2})\D+(\d{1,2})\D+(\d{1,2})/);if(match)date=new Date(Number(match[1]),Number(match[2])-1,Number(match[3]),12);}return date&&!Number.isNaN(date.getTime())?dateIso(mondayOf(date)):'';}
function findTermStart(table){const preferred=['startDate','start_date','termStart','term_start','semesterStart','semester_start','startDay','startTime'];for(const key of preferred){const value=table&&typeof table==='object'?table[key]:undefined,normalized=normalizeTermStart(value);if(normalized)return normalized;}if(table&&typeof table==='object'){for(const value of Object.values(table)){if(value&&typeof value==='object'){const nested=findTermStart(value);if(nested)return nested;}}}return'';}
function timeHm(date){return String(date.getHours()).padStart(2,'0')+':'+String(date.getMinutes()).padStart(2,'0');}
function mapImportedName(source){const clean=String(source||'').trim(),mapped=lookupCourseName(clean);if(mapped)return mapped;if(/^[\x20-\x7E]+$/.test(clean)){const upper=clean.toUpperCase();return{englishName:clean,displayName:upper,shortName:makeCourseShortName(upper),mappingSource:'original'};}return{englishName:'',displayName:'',shortName:'',mappingSource:'unmapped'};}
function parseIcs(text){const events=unfoldIcs(text).split('BEGIN:VEVENT').slice(1).map(v=>v.split('END:VEVENT')[0]),raw=[];for(const event of events){const start=parseIcsDate(icsValue(event,'DTSTART')),end=parseIcsDate(icsValue(event,'DTEND')),name=icsValue(event,'SUMMARY');if(!start||!end||!name)continue;raw.push({start,end,name,room:icsValue(event,'LOCATION'),teacher:(icsValue(event,'DESCRIPTION').match(/(?:Teacher|教师|老师)[:：]\s*([^;，,]+)/i)||[])[1]||'',rrule:icsValue(event,'RRULE')});}if(!raw.length)throw new Error('no calendar events found');const term=mondayOf(new Date(Math.min(...raw.map(x=>x.start.getTime()))));const courses=raw.map(item=>{let weeks='1-18';const until=(item.rrule.match(/UNTIL=([^;]+)/)||[])[1];if(until){const last=parseIcsDate(until);if(last)weeks='1-'+Math.max(1,Math.floor((last-term)/604800000)+1);}return{sourceName:item.name,...mapImportedName(item.name),day:item.start.getDay()||7,weeks,start:timeHm(item.start),end:timeHm(item.end),room:item.room,teacher:item.teacher};});return{schemaVersion:1,school:'',source:'ics',termStart:dateIso(term),courses};}
function normalizeJsonTimetable(data){if(data.data?.courseInfos)data={source:'xiaoai',courses:data.data.courseInfos};if(data.courseInfos)data={source:'xiaoai',courses:data.courseInfos};if(!Array.isArray(data.courses))throw new Error('courses array missing');const courses=data.courses.map(c=>{const source=c.sourceName||c.name||c.courseName||c.title||'',sections=c.sections||[],first=sections[0]?.section||c.startSection,last=sections[sections.length-1]?.section||c.endSection,periodTimes={1:['08:30','09:15'],2:['09:20','10:05'],3:['10:25','11:10'],4:['11:15','12:00'],5:['14:00','14:45'],6:['14:50','15:35'],7:['15:55','16:40'],8:['16:45','17:30'],9:['19:00','19:45'],10:['19:50','20:35'],11:['20:40','21:25'],12:['21:30','22:15']},mapped=mapImportedName(source),weeks=Array.isArray(c.weeks)?compressWeeks(c.weeks):String(c.weeks||'1-18');return{sourceName:source,englishName:c.englishName||mapped.englishName,displayName:(c.displayName||mapped.displayName||'').toUpperCase(),shortName:(c.shortName||mapped.shortName||'').toUpperCase(),day:Number(c.day||c.weekday||1),weeks,start:c.start||c.startTime||periodTimes[first]?.[0]||'',end:c.end||c.endTime||periodTimes[last]?.[1]||'',room:c.room||c.position||c.location||'',teacher:c.teacher||''};});return{schemaVersion:1,school:data.school||data.schoolId||'',source:data.source||'json',termStart:data.termStart||data.term?.startDate||'',courses};}
function compressWeeks(weeks){const nums=[...new Set(weeks.map(Number).filter(n=>n>0))].sort((a,b)=>a-b);if(!nums.length)return'';const out=[];let start=nums[0],last=nums[0];for(const n of nums.slice(1)){if(n===last+1){last=n;continue;}out.push(start===last?String(start):start+'-'+last);start=last=n;}out.push(start===last?String(start):start+'-'+last);return out.join(',');}
function parseCsv(text){const rows=text.trim().split(/\r?\n/).map(line=>line.split(',').map(v=>v.trim().replace(/^"|"$/g,''))),head=rows.shift().map(v=>v.toLowerCase()),pick=(row,names)=>{const i=head.findIndex(h=>names.includes(h));return i>=0?row[i]:'';},courses=rows.filter(r=>r.some(Boolean)).map(r=>{const source=pick(r,['课程名称','课程','name','course']),mapped=mapImportedName(source);return{sourceName:source,...mapped,day:Number(pick(r,['星期','day','weekday']))||1,weeks:pick(r,['周数','周次','weeks'])||'1-18',start:pick(r,['开始时间','start','starttime']),end:pick(r,['结束时间','end','endtime']),room:pick(r,['地点','教室','room','location']),teacher:pick(r,['老师','教师','teacher'])};});if(!courses.length)throw new Error('no CSV courses found');return{schemaVersion:1,school:'',source:'csv',termStart:'',courses};}
function detectAndParseText(text,name=''){const trimmed=text.trim(),lower=name.toLowerCase();if(trimmed.includes('BEGIN:VCALENDAR')||lower.endsWith('.ics'))return parseIcs(trimmed);if(trimmed.startsWith('{')||trimmed.startsWith('[')||lower.endsWith('.json'))return normalizeJsonTimetable(JSON.parse(trimmed));if(lower.endsWith('.csv')||trimmed.split(/\r?\n/)[0].includes(','))return parseCsv(trimmed);throw new Error('unsupported file format');}
async function translateCourseName(source){const url=new URL('https://api.mymemory.translated.net/get');url.searchParams.set('q',cleanCourseName(source));url.searchParams.set('langpair','zh-CN|en');url.searchParams.set('mt','1');const response=await fetch(url.toString(),{cache:'no-store'});if(!response.ok)throw new Error('translation service unavailable');const data=await response.json(),translated=String(data?.responseData?.translatedText||'').trim();if(!translated||/[\u3400-\u9fff]/.test(translated)||Number(data?.responseStatus||200)>=400)throw new Error(data?.responseDetails||'course could not be translated');return translated.replace(/\s*\((?:group|experiment|practice)\s*0*\d+\)\s*/ig,' ').trim();}
async function prepareCourseTranslations(draft){if(!document.getElementById('uAutoTranslate').checked)return;const missing=[...new Set(draft.courses.filter(c=>!c.displayName&&/[\u3400-\u9fff]/.test(c.sourceName||'')).map(c=>cleanCourseName(c.sourceName)))];if(!missing.length)return;setImportStatus('TRANSLATING · 0 / '+missing.length);let completed=0;const results=new Map();for(let offset=0;offset<missing.length;offset+=3){await Promise.all(missing.slice(offset,offset+3).map(async source=>{try{results.set(source,await translateCourseName(source))}catch(e){}finally{completed++;setImportStatus('TRANSLATING · '+completed+' / '+missing.length);}}));}draft.courses.forEach(c=>{const translated=results.get(cleanCourseName(c.sourceName));if(!c.displayName&&translated){c.englishName=translated;c.displayName=translated.toUpperCase();c.shortName=makeCourseShortName(translated);c.mappingSource='translation';}});}
function updateImportReviewCount(draft){document.getElementById('uPreviewReview').textContent=new Set(draft.courses.filter(c=>!c.displayName).map(c=>cleanCourseName(c.sourceName))).size;}
function showImportPreview(draft){if(!draft.courses.length)throw new Error('no courses found');draft.termStart=normalizeTermStart(draft.termStart);importDraft=draft;document.getElementById('uPreviewSource').textContent=String(draft.source||importSource||'FILE').toUpperCase();document.getElementById('uPreviewCount').textContent=draft.courses.length;document.getElementById('uPreviewTerm').value=draft.termStart;updateImportReviewCount(draft);const list=document.getElementById('uReview');list.innerHTML='';const groups=new Map();draft.courses.forEach(c=>{const key=cleanCourseName(c.sourceName)||c.sourceName;if(!groups.has(key))groups.set(key,[]);groups.get(key).push(c);});groups.forEach((rules,key)=>{const c=rules[0],row=document.createElement('div');row.className='ureview-row'+(!c.displayName?' warn':'');const source=document.createElement('small');source.className='ureview-source';source.textContent=c.sourceName||'UNNAMED COURSE';const input=document.createElement('input');input.className='uinput';input.value=c.englishName||c.displayName||'';input.placeholder='ENTER ENGLISH COURSE NAME';const meta=document.createElement('small');meta.textContent=rules.length+' RULE'+(rules.length===1?'':'S')+' · '+String(c.mappingSource||'unmapped').toUpperCase()+' · SCREEN: '+(c.shortName||'NOT SET');input.addEventListener('input',()=>{const english=input.value.trim(),display=english.toUpperCase(),shortName=makeCourseShortName(display);rules.forEach(rule=>{rule.englishName=english;rule.displayName=display;rule.shortName=shortName;rule.mappingSource='user';rule.userConfirmed=true;});row.classList.toggle('warn',!display);meta.textContent=rules.length+' RULE'+(rules.length===1?'':'S')+' · USER · SCREEN: '+(shortName||'NOT SET');updateImportReviewCount(draft);});row.append(source,input,meta);list.append(row);});document.getElementById('uPreview').classList.add('open');setImportStatus((draft.termStart?'READY TO REVIEW':'TERM DATE NEEDS REVIEW')+' · '+groups.size+' COURSES / '+draft.courses.length+' RULES',!draft.termStart);}
async function readTimetableFile(file){if(!file)return;try{setImportStatus('READING FILE · '+file.name);const draft=detectAndParseText(await file.text(),file.name);await prepareCourseTranslations(draft);showImportPreview(draft);}catch(e){setImportStatus(e.message||'file import failed',true);}finally{document.getElementById('uWizardFile').value='';}}
async function importTimetableFile(file){if(!file)return;openTimetableImporter();await readTimetableFile(file);document.getElementById('uImport').value='';}
async function readXiaoAiLink(value){const url=new URL(value);if(!/(^|\.)i\.ai\.mi\.com$/.test(url.hostname))throw new Error('not an official XiaoAi share link');const token=url.searchParams.get('linkToken');if(!token)throw new Error('XiaoAi linkToken missing');const parts=decodeURIComponent(atob(token)).split('&');if(parts.length<5)throw new Error('invalid XiaoAi share token');const endpoint=new URL('https://i.ai.mi.com/course-multi/table');endpoint.searchParams.set('userId',parts[0]);endpoint.searchParams.set('deviceId',parts[1]);endpoint.searchParams.set('ctId',parts[4]);const response=await fetch(endpoint.toString(),{cache:'no-store'});if(!response.ok)throw new Error('XiaoAi share request failed');return normalizeJsonTimetable(await response.json());}
function extractWakeUpCode(value){const marked=value.match(/分享口令为[「\"']?([A-Za-z0-9_-]{16,64})/),raw=value.match(/^([A-Za-z0-9_-]{16,64})$/);return(marked||raw||[])[1]||'';}
function parseWakeUpPayload(payload){if(Number(payload?.status)!==1||typeof payload.data!=='string')throw new Error(payload?.message||'WakeUp share code is invalid or expired');const parts=payload.data.replace(/\\\"/g,'\"').replace(/\\\\/g,'\\').trim().split(/\r?\n/);if(parts.length<5)throw new Error('WakeUp returned an unsupported timetable format');let slots,table,infos,details;try{slots=JSON.parse(parts[1]);table=JSON.parse(parts[2]);infos=JSON.parse(parts[3]);details=JSON.parse(parts[4]);}catch(e){throw new Error('WakeUp timetable data could not be decoded');}const byId=new Map(infos.map(c=>[Number(c.id),c])),byNode=new Map(slots.map(s=>[Number(s.node),s])),courses=[];for(const d of details){if(d.ownTime)continue;const info=byId.get(Number(d.id)),first=byNode.get(Number(d.startNode)),last=byNode.get(Number(d.startNode)+Number(d.step)-1);if(!info||!first||!last)continue;const mapped=mapImportedName(info.courseName||''),weeks=[];for(let w=Number(d.startWeek)||1;w<=(Number(d.endWeek)||1);w++){if(Array.isArray(d.weekList)&&d.weekList.length&&!d.weekList.includes(w))continue;if(Number(d.type)===1&&w%2===0)continue;if(Number(d.type)===2&&w%2===1)continue;weeks.push(w);}courses.push({sourceName:info.courseName||'',...mapped,day:Number(d.day)||1,weeks:compressWeeks(weeks),start:String(first.startTime||'').slice(0,5),end:String(last.endTime||'').slice(0,5),room:d.room||d.position||'',teacher:d.teacher||''});}if(!courses.length)throw new Error('WakeUp timetable contains no importable courses');return{schemaVersion:1,school:'',source:'wakeup',termStart:findTermStart(table),courses};}
async function readWakeUpCode(code){if(!window.WakeUpImport)throw new Error('WakeUp importer did not load');const data=await WakeUpImport.importCode(code);return parseWakeUpPayload({status:1,data});}
async function readPastedTimetable(){const value=document.getElementById('uPaste').value.trim();if(!value){setImportStatus('paste a share link, code, or calendar URL',true);return;}try{setImportStatus('DETECTING SOURCE');let draft,wakeUpCode=extractWakeUpCode(value);if(wakeUpCode){selectImportSource('wakeup');setImportStatus('SOURCE DETECTED · WAKEUP');draft=await readWakeUpCode(wakeUpCode);}else if(/^https?:\/\//i.test(value)){const url=new URL(value);if(/(^|\.)i\.ai\.mi\.com$/.test(url.hostname)){selectImportSource('xiaoai');setImportStatus('SOURCE DETECTED · XIAOAI');draft=await readXiaoAiLink(value);}else if(/\.ics($|\?)/i.test(url.pathname)){selectImportSource('ics');const r=await fetch(value);if(!r.ok)throw new Error('calendar download failed');draft=parseIcs(await r.text());}else throw new Error('unsupported or non-official share URL');}else if(value.startsWith('{')||value.includes('BEGIN:VCALENDAR'))draft=detectAndParseText(value);else throw new Error('could not detect a supported timetable');await prepareCourseTranslations(draft);showImportPreview(draft);}catch(e){setImportStatus(e.message||'could not read timetable',true);}}
async function confirmTimetableImport(){if(!importDraft)return;if(importDraft.courses.some(c=>!c.displayName)){setImportStatus('review every unmapped course before importing',true);return;}const term=normalizeTermStart(document.getElementById('uPreviewTerm').value||importDraft.termStart);if(!term){setImportStatus('WakeUp did not include a term date · choose any date in the first teaching week',true);document.getElementById('uPreviewTerm').focus();return;}document.getElementById('uPreviewTerm').value=term;importDraft.termStart=term;const remembered=new Set();importDraft.courses.forEach(c=>{const key=cleanCourseName(c.sourceName);if(key&&!remembered.has(key)){rememberCourseMapping(key,c.englishName||c.displayName);remembered.add(key);}c.userConfirmed=true;});timetable=importDraft;if(await saveTimetable()){closeTimetableImporter();toast(remembered.size+' courses imported');}}
function exportTimetable(){const blob=new Blob([JSON.stringify(timetable,null,2)],{type:'application/json'}),a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='gdufs-timetable.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);}
function applyDinoState(state){document.getElementById('dinoState').textContent=String(state.state||'ready').replace('_',' ').toUpperCase();document.getElementById('dinoScore').textContent=String(state.score||0).padStart(4,'0').slice(-4);document.getElementById('dinoHighScore').textContent=String(state.highScore||0).padStart(4,'0').slice(-4);}
async function dinoPost(path){try{const r=await fetch(path,{method:'POST',cache:'no-store'});const state=await r.json();if(!r.ok)throw new Error(state.error||'game request failed');applyDinoState(state);return true;}catch(e){toast(e.message||'game unavailable',false);return false;}}
function lockViewsForDino(locked){document.querySelectorAll('.vbtn').forEach(button=>{button.disabled=locked&&Number(button.dataset.v)!==11;});}
async function openArcade(){if(isBusy||termOpen||canvasOpen)return;if(activeView===19)await leaveMediaForView();arcadeOpen=true;document.getElementById('arcadeWrap').classList.add('open');document.getElementById('arcadeHome').style.display='grid';document.body.style.overflow='hidden';lockViewsForDino(true);}
async function closeArcade(){if(dinoOpen)await exitDinoGame();if(sokobanOpen)await exitSokobanGame();if(arcadeGameOpen)await exitArcadeGame();arcadeOpen=false;document.getElementById('arcadeWrap').classList.remove('open');document.body.style.overflow='';lockViewsForDino(false);}
function showArcadeHome(){document.getElementById('arcadeHome').style.display='grid';document.getElementById('dinoWrap').classList.remove('open');document.getElementById('sokobanWrap').classList.remove('open');document.querySelectorAll('[data-game-panel]').forEach(panel=>panel.classList.remove('open'));activeView=0;}
async function openDinoGame(){if(isBusy||termOpen||canvasOpen||dinoOpen)return;if(!arcadeOpen)openArcade();if(!await dinoPost('/game/dino/start'))return;dinoOpen=true;activeView=11;document.getElementById('arcadeHome').style.display='none';document.getElementById('dinoWrap').classList.add('open');document.querySelectorAll('.vbtn').forEach(button=>button.classList.toggle('active',Number(button.dataset.v)===11));clearInterval(dinoPollTimer);dinoPollTimer=setInterval(pollDinoState,500);toast('dino game ready');}
async function jumpDino(){if(!dinoOpen||dinoJumpPending)return;dinoJumpPending=true;await dinoPost('/game/dino/action?action=jump');setTimeout(()=>{dinoJumpPending=false;},55);}
async function restartDinoGame(){if(await dinoPost('/game/dino/restart'))toast('game reset');}
async function pollDinoState(){if(!dinoOpen)return;try{const r=await fetch('/game/dino/state',{cache:'no-store'});if(r.ok)applyDinoState(await r.json());}catch(e){}}
async function exitDinoGame(){if(!dinoOpen)return;await dinoPost('/game/dino/exit');dinoOpen=false;clearInterval(dinoPollTimer);dinoPollTimer=null;document.querySelectorAll('.vbtn').forEach(button=>button.classList.remove('active'));showArcadeHome();toast('back to arcade');}
document.getElementById('dinoJump').addEventListener('pointerdown',e=>{e.preventDefault();jumpDino();});
function applySokobanState(state){sokobanStateData=state;document.getElementById('sokobanState').textContent=String(state.state||'playing').toUpperCase();document.getElementById('sokobanLevel').textContent=String(state.level||1).padStart(2,'0')+'/'+String(state.levelCount||8).padStart(2,'0');document.getElementById('sokobanMoves').textContent=String(state.moves||0).padStart(3,'0').slice(-3);document.getElementById('sokobanBoxes').textContent=String(state.boxesOnGoals||0)+'/'+String(state.boxes||0);document.getElementById('sokobanLevelName').textContent='CLASSIC '+String(state.level||1).padStart(2,'0');document.getElementById('sokobanUndo').disabled=!state.canUndo;document.getElementById('sokobanPrev').disabled=Number(state.level)<=1;document.getElementById('sokobanNext').disabled=Number(state.level)>=Number(state.levelCount);}
async function sokobanPost(path){try{const r=await fetch(path,{method:'POST',cache:'no-store'});const state=await r.json();if(!r.ok)throw new Error(state.error||'game request failed');applySokobanState(state);return true;}catch(e){toast(e.message||'game unavailable',false);return false;}}
async function openSokobanGame(){if(isBusy||termOpen||canvasOpen||sokobanOpen)return;if(!arcadeOpen)openArcade();if(!await sokobanPost('/game/sokoban/start'))return;sokobanOpen=true;activeView=12;document.getElementById('arcadeHome').style.display='none';document.getElementById('sokobanWrap').classList.add('open');document.querySelectorAll('.vbtn').forEach(button=>button.classList.toggle('active',Number(button.dataset.v)===11));toast('box.push ready');}
async function moveSokoban(direction){if(!sokobanOpen||sokobanMovePending)return;sokobanMovePending=true;await sokobanPost('/game/sokoban/move?direction='+encodeURIComponent(direction));setTimeout(()=>{sokobanMovePending=false;},55);}
async function undoSokoban(){if(await sokobanPost('/game/sokoban/undo'))toast('move undone');}
async function restartSokoban(){if(await sokobanPost('/game/sokoban/restart'))toast('level reset');}
async function changeSokobanLevel(delta){if(!sokobanStateData)return;const level=Number(sokobanStateData.level)+delta;if(level<1||level>Number(sokobanStateData.levelCount))return;await sokobanPost('/game/sokoban/level?index='+level);}
async function exitSokobanGame(){if(!sokobanOpen)return;await sokobanPost('/game/sokoban/exit');sokobanOpen=false;document.querySelectorAll('.vbtn').forEach(button=>button.classList.remove('active'));showArcadeHome();toast('back to arcade');}
document.querySelectorAll('#sokobanPad .dbtn').forEach(button=>button.addEventListener('pointerdown',e=>{e.preventDefault();moveSokoban(button.dataset.direction);}));
function applyArcadeGameState(state){arcadeGameState=state;const id=state.id||arcadeGameOpen;if(id==='tetris'){document.getElementById('tetrisState').textContent=String(state.state||'playing').replace('_',' ').toUpperCase();document.getElementById('tetrisScore').textContent=String(state.score||0).padStart(6,'0');document.getElementById('tetrisLines').textContent=String(state.lines||0).padStart(3,'0');document.getElementById('tetrisLevel').textContent=String(state.level||1).padStart(2,'0');}else if(id==='snake'){document.getElementById('snakeState').textContent=String(state.state||'ready').replace('_',' ').toUpperCase();document.getElementById('snakeScore').textContent=String(state.score||0).padStart(3,'0');document.getElementById('snakeLength').textContent=String(state.length||5).padStart(3,'0');document.getElementById('snakeSpeed').textContent=String(state.speedMs||175);}else if(id==='2048'){document.getElementById('game2048State').textContent=String(state.state||'playing').replace('_',' ').toUpperCase();document.getElementById('game2048Score').textContent=String(state.score||0).padStart(5,'0');document.getElementById('game2048Best').textContent=String(state.bestScore||0).padStart(5,'0');document.getElementById('game2048Max').textContent=String(state.maxTile||2).padStart(4,'0');document.getElementById('game2048Undo').disabled=!state.canUndo;}else if(id==='breakout'){document.getElementById('breakoutState').textContent=String(state.state||'ready').replace('_',' ').toUpperCase();document.getElementById('breakoutScore').textContent=String(state.score||0).padStart(5,'0');document.getElementById('breakoutLevel').textContent=String(state.level||1).padStart(2,'0');document.getElementById('breakoutLives').textContent=String(state.lives??3);}}
async function arcadeRequest(path){try{const r=await fetch(path,{method:'POST',cache:'no-store'}),state=await r.json();if(!r.ok)throw new Error(state.error||'game request failed');applyArcadeGameState(state);return true;}catch(e){toast(e.message||'game unavailable',false);return false;}}
async function openArcadeGame(id){if(isBusy||termOpen||canvasOpen||arcadeGameOpen)return;if(!arcadeOpen)openArcade();if(!await arcadeRequest('/game/start?id='+encodeURIComponent(id)))return;arcadeGameOpen=id;activeView={tetris:13,snake:14,'2048':15,breakout:16}[id];document.getElementById('arcadeHome').style.display='none';document.getElementById(id==='2048'?'game2048Wrap':id+'Wrap').classList.add('open');document.querySelectorAll('.vbtn').forEach(button=>button.classList.toggle('active',Number(button.dataset.v)===11));clearInterval(arcadeGamePollTimer);arcadeGamePollTimer=setInterval(pollArcadeGameState,400);toast(id+' ready');}
async function arcadeAction(action,value=0){if(!arcadeGameOpen||arcadeGamePending)return false;arcadeGamePending=true;const ok=await arcadeRequest('/game/action?action='+encodeURIComponent(action)+'&value='+encodeURIComponent(value));setTimeout(()=>{arcadeGamePending=false;},45);return ok;}
async function pollArcadeGameState(){if(!arcadeGameOpen)return;try{const r=await fetch('/game/state?id='+encodeURIComponent(arcadeGameOpen),{cache:'no-store'});if(r.ok)applyArcadeGameState(await r.json());}catch(e){}}
async function exitArcadeGame(){if(!arcadeGameOpen)return;await fetch('/game/exit',{method:'POST'}).catch(()=>{});arcadeGameOpen='';arcadeGameState=null;clearInterval(arcadeGamePollTimer);arcadeGamePollTimer=null;document.querySelectorAll('.vbtn').forEach(button=>button.classList.remove('active'));showArcadeHome();toast('back to arcade');}
document.querySelectorAll('[data-arcade-action]').forEach(button=>button.addEventListener('pointerdown',e=>{e.preventDefault();arcadeAction(button.dataset.arcadeAction);}));
document.getElementById('breakoutPaddle').addEventListener('input',e=>{clearTimeout(breakoutPositionTimer);const value=e.target.value;breakoutPositionTimer=setTimeout(()=>arcadeAction('position',value),35);});
window.addEventListener('keydown',e=>{if(dinoOpen&&(e.code==='Space'||e.key===' ')){e.preventDefault();jumpDino();return;}if(sokobanOpen){const direction={ArrowUp:'up',ArrowDown:'down',ArrowLeft:'left',ArrowRight:'right'}[e.key];if(direction){e.preventDefault();moveSokoban(direction);}else if(e.key.toLowerCase()==='u'){e.preventDefault();undoSokoban();}else if(e.key.toLowerCase()==='r'){e.preventDefault();restartSokoban();}return;}if(!arcadeGameOpen)return;let action='';if(arcadeGameOpen==='tetris'){action={ArrowLeft:'left',ArrowRight:'right',ArrowDown:'down',ArrowUp:'rotate',Space:'drop'}[e.code]||'';}else if(arcadeGameOpen==='snake'||arcadeGameOpen==='2048'){action={ArrowUp:'up',ArrowDown:'down',ArrowLeft:'left',ArrowRight:'right'}[e.key]||'';}else if(arcadeGameOpen==='breakout'){action={ArrowLeft:'left',ArrowRight:'right',Space:'launch'}[e.code]||'';}if(action){e.preventDefault();arcadeAction(action);}else if(e.key.toLowerCase()==='r'){e.preventDefault();arcadeAction('restart');}});
function updateBlButton(){const b=document.getElementById('blBtn');b.textContent=blOn?'☀ display on':'○ display off';b.classList.toggle('on',blOn);b.classList.toggle('dim',!blOn);}
function updateClaudeStatusButton(){const b=document.getElementById('ccStatusBtn');b.textContent=claudeStatusOn?'◆ claude status on':'◇ claude status off';b.classList.toggle('on',claudeStatusOn);b.classList.toggle('dim',!claudeStatusOn);const activityClaude=document.getElementById('activityClaude');if(activityClaude)activityClaude.textContent=claudeStatusOn?'ON':'OFF';}
let carouselPages=[{id:6,name:'Clock'},{id:7,name:'Pomodoro'},{id:8,name:'Weather'},{id:9,name:'Crypto'},{id:10,name:'Market'},{id:17,name:'Live Ledger'},{id:18,name:'Timetable'}];
function carouselName(v){return carouselPages.find(p=>p.id===Number(v))?.name||'Unknown page';}
function applyCarouselPrefs(p){
  carouselConfig.enabled=p.carousel===true;carouselConfig.speed=Math.max(5,Math.min(60,Number(p.carouselSpeed)||12));
  if(Array.isArray(p.carouselPages)&&p.carouselPages.length){const oldNames=new Map(carouselPages.map(x=>[x.id,x.name]));carouselPages=p.carouselPages.map(Number).filter((v,i,a)=>a.indexOf(v)===i).map(id=>({id,name:oldNames.get(id)||('Page '+id)}));}
  const order=Array.isArray(p.carouselOrder)?p.carouselOrder.map(Number).filter((v,i,a)=>carouselPages.some(x=>x.id===v)&&a.indexOf(v)===i):[];
  carouselConfig.order=order.length?order:carouselPages.map(x=>x.id);
  carouselConfig.fixed=carouselPages.some(x=>x.id===Number(p.carouselFixed))?Number(p.carouselFixed):8;renderCarousel();
}
function renderCarousel(){
  const enabled=carouselConfig.enabled,toggle=document.getElementById('carouselToggle'),fixed=document.getElementById('carouselFixed'),speed=document.getElementById('carouselSpeed');
  toggle.textContent=enabled?'● carousel on':'○ carousel off';toggle.classList.toggle('on',enabled);toggle.classList.toggle('dim',!enabled);
  fixed.innerHTML='';carouselPages.forEach(p=>{const o=document.createElement('option');o.value=String(p.id);o.textContent=p.name;fixed.appendChild(o);});fixed.value=String(carouselConfig.fixed);fixed.disabled=enabled;speed.value=String(carouselConfig.speed);speed.disabled=!enabled;document.getElementById('carouselSpeedV').textContent=carouselConfig.speed+'s';
  document.getElementById('carouselHint').textContent=enabled?'Claude Code pauses the carousel, then it resumes from the interrupted page.':'Select the single info page shown while the carousel is off.';
  const out=document.getElementById('carouselOrder');out.innerHTML='';carouselConfig.order.forEach((view,i)=>{
    const row=document.createElement('div');row.className='ritem';
    const index=document.createElement('span');index.className='rindex';index.textContent=String(i+1).padStart(2,'0');
    const name=document.createElement('span');name.className='rname';name.textContent=carouselName(view);
    const remove=document.createElement('button');remove.className='mdrag rremove';remove.type='button';remove.textContent='×';remove.disabled=carouselConfig.order.length<=1;remove.setAttribute('aria-label','Remove '+carouselName(view));remove.addEventListener('click',()=>removeCarouselPage(view));
    const handle=document.createElement('button');handle.className='mdrag rdrag';handle.type='button';handle.textContent='⠿';handle.disabled=!enabled;handle.setAttribute('aria-label','Drag '+carouselName(view));
    handle.addEventListener('pointerdown',e=>startCarouselDrag(e,i,row));
    row.append(index,name,remove,handle);out.appendChild(row);
  });
  const add=document.getElementById('carouselAdd');if(add){add.innerHTML='';const empty=document.createElement('option');empty.value='';empty.textContent='ADD PAGE...';add.appendChild(empty);carouselPages.forEach(p=>{if(!carouselConfig.order.includes(p.id)){const o=document.createElement('option');o.value=String(p.id);o.textContent=p.name;add.appendChild(o);}});add.value='';}
}
function toggleCarouselPanel(){carouselPanelOpen=!carouselPanelOpen;const panel=document.getElementById('carouselWrap'),state=document.getElementById('carouselPanelState');panel.classList.toggle('open',carouselPanelOpen);state.textContent=carouselPanelOpen?'close':'open';}
async function saveCarousel(){
  const q=new URLSearchParams({carousel:carouselConfig.enabled?'1':'0',carouselSpeed:String(carouselConfig.speed),carouselFixed:String(carouselConfig.fixed),carouselOrder:carouselConfig.order.join(',')});
  try{const r=await fetch('/prefs?'+q.toString(),{cache:'no-store'});if(!r.ok)throw new Error();const p=await r.json();if(typeof p.carousel==='boolean')applyCarouselPrefs(p);toast('idle display saved');}
  catch(e){toast('idle display save failed',false);}
}
function toggleCarousel(){carouselConfig.enabled=!carouselConfig.enabled;renderCarousel();saveCarousel();}
function setCarouselFixed(value){carouselConfig.fixed=Number(value);saveCarousel();}
function setCarouselSpeed(value){carouselConfig.speed=Number(value);document.getElementById('carouselSpeedV').textContent=carouselConfig.speed+'s';clearTimeout(carouselSpeedTimer);carouselSpeedTimer=setTimeout(saveCarousel,260);}
function addCarouselPage(){const select=document.getElementById('carouselAdd'),view=Number(select.value);if(!view||carouselConfig.order.includes(view))return;carouselConfig.order.push(view);renderCarousel();saveCarousel();}
function removeCarouselPage(view){if(carouselConfig.order.length<=1)return;carouselConfig.order=carouselConfig.order.filter(v=>v!==view);renderCarousel();saveCarousel();}
function startCarouselDrag(e,index,row){
  if(e.button!==undefined&&e.button!==0||!carouselConfig.enabled)return;e.preventDefault();
  const rows=[...document.querySelectorAll('#carouselOrder .ritem')],box=document.getElementById('carouselOrder').getBoundingClientRect();
  carouselDrag={index,target:index,row,startY:e.clientY,box,rowH:box.height/rows.length};row.classList.add('dragging');e.currentTarget.setPointerCapture(e.pointerId);
  document.addEventListener('pointermove',moveCarouselDrag,{passive:false});document.addEventListener('pointerup',endCarouselDrag,{once:true});
}
function moveCarouselDrag(e){if(!carouselDrag)return;e.preventDefault();const delta=e.clientY-carouselDrag.startY;carouselDrag.row.style.transform='translateY('+delta+'px)';carouselDrag.target=Math.max(0,Math.min(carouselConfig.order.length-1,Math.floor((e.clientY-carouselDrag.box.top)/carouselDrag.rowH)));}
function endCarouselDrag(){document.removeEventListener('pointermove',moveCarouselDrag);if(!carouselDrag)return;const from=carouselDrag.index,to=carouselDrag.target;carouselDrag.row.classList.remove('dragging');carouselDrag.row.style.transform='';carouselDrag=null;if(from===to)return;const view=carouselConfig.order.splice(from,1)[0];carouselConfig.order.splice(to,0,view);renderCarousel();saveCarousel();}
async function toggleBL(){blOn=!blOn;const v=blOn?100:0;document.getElementById('bright').value=v;document.getElementById('brightV').textContent=v+'%';await req('/backlight?on='+(blOn?1:0));updateBlButton();}
async function toggleClaudeStatus(){const next=!claudeStatusOn;if(!await req('/prefs?claudeStatus='+(next?'1':'0')))return;claudeStatusOn=next;updateClaudeStatusButton();toast(claudeStatusOn?'Claude status on':'Claude status off');}
function applyDisplayTheme(theme){displayTheme=[1,2,3,4].includes(Number(theme))?Number(theme):1;const fg={1:'#fff',2:'#f64b00',3:'#5fe0ad',4:'#ff7eb3'}[displayTheme];document.documentElement.style.setProperty('--device-fg',fg);document.querySelectorAll('.theme-btn').forEach(btn=>btn.setAttribute('aria-pressed',String(Number(btn.dataset.theme)===displayTheme)));}
async function setDisplayTheme(theme){try{const r=await fetch('/prefs?theme='+encodeURIComponent(theme),{cache:'no-store'});if(!r.ok)throw new Error();const p=await r.json();applyDisplayTheme(p.theme);document.getElementById('bgCol').value=p.bg;document.getElementById('mediaBg').value=p.bg;document.getElementById('bright').value=p.brightness;document.getElementById('brightV').textContent=p.brightness+'%';if(typeof redrawSelectedMedia==='function')redrawSelectedMedia();await loadProfile();await loadExpressions();toast(['','classic','dark','mint','pink'][displayTheme]+' theme');}catch(e){toast('theme save failed',false);}}
function applyFontStyle(style){const allowed=['pixel','courier','terminal','dashboard'];fontStyle=allowed.includes(style)?style:'pixel';document.documentElement.dataset.fontStyle=fontStyle;const select=document.getElementById('fontSelect');if(select)select.value=fontStyle;}
async function setFontStyle(style){try{const r=await fetch('/prefs?fontStyle='+encodeURIComponent(style),{cache:'no-store'});if(!r.ok)throw new Error();const p=await r.json();applyFontStyle(p.fontStyle);toast(fontStyle+' font selected');}catch(e){toast('font save failed',false);}}
async function setBrightness(v){v=parseInt(v||0);document.getElementById('brightV').textContent=v+'%';const activityBrightness=document.getElementById('activityBrightness');if(activityBrightness)activityBrightness.textContent=v+'%';blOn=v>0;updateBlButton();await req('/brightness?v='+v);}
async function loadPrefs(){try{const r=await fetch('/prefs');const p=await r.json();const bg=p.bg||'#aa4818';bgPreviewHex=bg;document.getElementById('bgCol').value=bg;document.getElementById('mediaBg').value=bg;claudeStatusOn=p.claudeStatus!==false;updateClaudeStatusButton();applyDisplayTheme(p.theme);applyFontStyle(p.fontStyle);applyCarouselPrefs(p);redrawCanvas(bg);if(typeof redrawSelectedMedia==='function')redrawSelectedMedia();}catch(e){applyDisplayTheme(1);applyFontStyle('pixel');renderCarousel();}}
function fmtSec(s){s=Math.max(0,parseInt(s||0));return String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0');}
async function pollTimer(){try{const r=await fetch('/timer/status');const j=await r.json();document.getElementById('pPhase').textContent=(j.phase==='break'?'BREAK':'FOCUS')+(j.paused?' / PAUSED':'');document.getElementById('pTime').textContent=fmtSec(j.remaining);document.getElementById('focusMin').value=j.focus;document.getElementById('breakMin').value=j.break;}catch(e){}}
async function startTimer(phase){await fetch('/timer/start?phase='+phase);document.getElementById('pwrap').classList.add('open');await pollTimer();toast(phase==='break'?'break started':'focus started');}
async function pauseTimer(){await fetch('/timer/pause');await pollTimer();toast('timer toggled');}
async function resetTimer(){await fetch('/timer/reset');await pollTimer();toast('timer reset');}
async function configTimer(){const f=document.getElementById('focusMin').value||25,b=document.getElementById('breakMin').value||5;await fetch('/timer/config?focus='+encodeURIComponent(f)+'&break='+encodeURIComponent(b));await pollTimer();}
function salaryClock(seconds){seconds=Math.max(0,Number(seconds)||0);const h=Math.floor(seconds/3600),m=Math.floor(seconds/60)%60,s=Math.floor(seconds)%60;return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');}
function salaryMoney(units){
  const value=Math.max(0,Number(units)||0)/10000;
  return (Math.floor(value*1000+1e-9)/1000).toFixed(3);
}
function renderSalaryAmount(units){
  const text=salaryMoney(units),length=text.length,size=length<=6?5:(length<=8?4:(length<=10?3:(length<=16?2:1))),amount=document.getElementById('ypAmount');
  const amountX=32+(196-length*6*size)/2;
  amount.textContent=text;amount.style.setProperty('--amount-scale',size);document.querySelector('.ypcurrency').style.left=(amountX-19)+'px';
}
function salaryTime(minutes){minutes=Math.max(0,Math.min(1440,Number(minutes)||0));const h=Math.floor(minutes/60)%24,m=minutes%60;return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0');}
function salaryMinutes(value){const parts=String(value||'').split(':');return parts.length===2?Number(parts[0])*60+Number(parts[1]):NaN;}
function animateSalaryAmount(now){
  if(activeView===17&&salaryState==='running'&&lastSalaryStatus){
    const elapsed=Math.max(0,now-salaryMotionAt)/1000;
    const units=salaryMotionBase+(Number(lastSalaryStatus.rateTenThousandths)||0)*elapsed;
    renderSalaryAmount(units);
  }
  requestAnimationFrame(animateSalaryAmount);
}
function applySalaryStatus(j){
  lastSalaryStatus=j;
  salaryState=j.state||'unconfigured';salaryConfigured=j.configured===true;
  const state=salaryState.toUpperCase(),running=salaryState==='running',paused=salaryState==='paused',active=running||paused;
  const worked=salaryClock(j.activeSeconds);
  salaryMotionBase=Number(j.earnedTenThousandths)||0;salaryMotionAt=performance.now();
  const previewState=running?'RUNNING':(paused?'PAUSED':(salaryState==='finished'?'DONE':(!salaryConfigured?'SET PAY':'READY'))),progress=Math.max(0,Math.min(1000,Number(j.progressPermille)||0));
  document.getElementById('yState').textContent=state;document.getElementById('ypLive').textContent=previewState;
  renderSalaryAmount(j.earnedTenThousandths);document.getElementById('ypWorked').textContent=worked;
  document.getElementById('ypProgressText').textContent=Math.round(progress/10)+'%';document.getElementById('ypProgress').style.width=Math.floor(214*progress/1000)+'px';
  document.getElementById('yTarget').textContent=((Number(j.dailyTargetTenThousandths)||0)/10000).toFixed(2);
  const primary=document.getElementById('yPrimary');primary.textContent=running?'PAUSE':(paused?'RESUME':'START WORK');
  primary.disabled=salaryRequestPending||(!salaryConfigured&&active);document.getElementById('yFinish').disabled=!active||salaryRequestPending;
  document.getElementById('yReset').disabled=active||salaryRequestPending;
  ['yMonthlyInput','yDaysInput','yHoursInput','yStartInput','yEndInput','yAutoInput','ySave'].forEach(id=>document.getElementById(id).disabled=active||salaryRequestPending);
  if(!salaryConfigured)toggleSalarySettings(true);
}
async function loadSalaryConfig(){
  try{const r=await fetch('/salary/config',{cache:'no-store'}),j=await r.json();if(!r.ok)throw new Error(j.error||'config unavailable');
    document.getElementById('yMonthlyInput').value=j.monthlyCents?Number(j.monthlyCents)/100:15000;
    document.getElementById('yDaysInput').value=(Number(j.workDaysX100)||2175)/100;document.getElementById('yHoursInput').value=(Number(j.workMinutesPerDay)||480)/60;
    document.getElementById('yStartInput').value=salaryTime(j.startMinutes??570);
    document.getElementById('yEndInput').value=salaryTime(j.endMinutes??1140);
    document.getElementById('ypStart').textContent=salaryTime(j.startMinutes??570);document.getElementById('ypEnd').textContent=salaryTime(j.endMinutes??1140);
    document.getElementById('yAutoInput').value=j.autoEnabled===false?'0':'1';
    document.getElementById('yMonthly').textContent=j.monthlyCents?(Number(j.monthlyCents)/100).toFixed(0):'--';
  }catch(e){toast(e.message||'salary config unavailable',false);}
}
async function pollSalary(){if(activeView!==17)return;try{const r=await fetch('/salary/status',{cache:'no-store'}),j=await r.json();if(r.ok)applySalaryStatus(j);}catch(e){}}
async function salaryPost(path,payload={}){
  if(salaryRequestPending)return null;salaryRequestPending=true;
  try{const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)}),j=await r.json();if(!r.ok)throw new Error(j.error||'request failed');applySalaryStatus(j);return j;}
  catch(e){toast(e.message||'salary request failed',false);return null;}
  finally{salaryRequestPending=false;if(lastSalaryStatus)applySalaryStatus(lastSalaryStatus);}
}
async function saveSalaryConfig(){
  const monthly=Number(document.getElementById('yMonthlyInput').value),days=Number(document.getElementById('yDaysInput').value),hours=Number(document.getElementById('yHoursInput').value);
  const startMinutes=salaryMinutes(document.getElementById('yStartInput').value),endMinutes=salaryMinutes(document.getElementById('yEndInput').value),autoEnabled=document.getElementById('yAutoInput').value==='1';
  if(!Number.isFinite(monthly)||monthly<=0||!Number.isFinite(days)||days<1||days>31||!Number.isFinite(hours)||hours<1||hours>24||!Number.isFinite(startMinutes)||!Number.isFinite(endMinutes)||startMinutes>=endMinutes){toast('check salary settings and work time',false);return false;}
  const result=await salaryPost('/salary/config',{monthlyCents:Math.round(monthly*100),workDaysX100:Math.round(days*100),workMinutesPerDay:Math.round(hours*60),autoEnabled,startMinutes,endMinutes});
  if(!result)return false;document.getElementById('yMonthly').textContent=monthly.toFixed(0);toast('salary settings saved');return true;
}
async function salaryPrimary(){
  if(salaryState==='running'){if(await salaryPost('/salary/pause'))toast('earnings paused');return;}
  if(salaryState==='paused'){if(await salaryPost('/salary/resume',{epoch:Math.floor(Date.now()/1000)}))toast('back to work');return;}
  if(!salaryConfigured&&!(await saveSalaryConfig()))return;
  if(await salaryPost('/salary/start',{epoch:Math.floor(Date.now()/1000)})){toggleSalarySettings(false);toast('work started');}
}
async function salaryFinish(){if(!confirm('Clock out and finish today?'))return;if(await salaryPost('/salary/finish'))toast('day finished');}
async function salaryReset(){if(!confirm("Reset today's earnings?"))return;if(await salaryPost('/salary/reset'))toast('today reset');}

function toggleSalarySettings(force){salarySettingsOpen=typeof force==='boolean'?force:!salarySettingsOpen;document.getElementById('ySettings').classList.toggle('open',salarySettingsOpen);document.getElementById('ySettingsState').textContent=salarySettingsOpen?'CLOSE':'OPEN';}
function marketPrice(v){if(v==null)return'--';if(v>=1000000)return'$'+(v/1000000).toFixed(2)+'M';if(v>=100000)return'$'+Math.round(v/1000)+'K';if(v>=10000)return'$'+(v/1000).toFixed(1)+'K';if(v>=1000)return'$'+Math.round(v);if(v>=100)return'$'+v.toFixed(1);if(v>=1)return'$'+v.toFixed(2);if(v>=.01)return'$'+v.toFixed(4);return'$'+v.toFixed(6);}
function marketChange(v){return v==null?'--':(v>=0?'+':'')+Number(v).toFixed(1)+'%';}
function marketTime(){if(!marketUpdatedAt)return'UPDATED --:--';return'UPDATED '+marketUpdatedAt.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit',hour12:false});}
function renderMarket(){
  document.getElementById('mCount').textContent=marketSelected.length+' / 5';
  document.getElementById('mTime').textContent=marketTime();
  const preview=document.getElementById('mPreview');preview.innerHTML='';
  const selected=document.getElementById('mSelected');selected.innerHTML='';
  marketSelected.forEach((a,i)=>{
    const row=document.createElement('div');row.className='mrow';
    ['msym','mprice','mchg'].forEach((cls,n)=>{const s=document.createElement('span');s.className=cls;s.textContent=n===0?a.symbol:(n===1?marketPrice(a.price):marketChange(a.change));row.appendChild(s);});
    preview.appendChild(row);
    const item=document.createElement('div');item.className='mselrow';item.dataset.index=i;
    const handle=document.createElement('button');handle.className='mdrag';handle.type='button';handle.textContent='⠿';handle.setAttribute('aria-label','Drag '+a.symbol);
    handle.addEventListener('pointerdown',e=>startMarketDrag(e,i,item));item.appendChild(handle);
    const name=document.createElement('div');name.className='mselname';
    const strong=document.createElement('strong');strong.textContent=a.symbol;
    const label=document.createElement('span');label.textContent=a.name+(a.gold?' (troy oz)':'');
    name.append(strong,label);item.appendChild(name);
    const remove=document.createElement('button');remove.className='mmini remove';remove.textContent='×';remove.setAttribute('aria-label','Remove '+a.symbol);remove.onclick=()=>removeMarket(i);item.appendChild(remove);
    selected.appendChild(item);
  });
  renderMarketResults();
}
async function loadMarketConfig(){
  try{const r=await fetch('/crypto/config',{cache:'no-store'});const j=await r.json();marketSelected=Array.isArray(j.assets)?j.assets:[];marketUpdatedAt=typeof j.updatedAgeSec==='number'?new Date(Date.now()-j.updatedAgeSec*1000):null;renderMarket();}
  catch(e){toast('market config unavailable',false);}
}
async function loadMarketDirectory(){
  if(marketLoaded)return;
  const out=document.getElementById('mResults');out.textContent='loading public asset directory...';
  try{
    const r=await fetch('https://api.coinlore.net/api/assets/');
    const j=await r.json();const list=Array.isArray(j)?j:(j.data||j.assets||[]);
    marketDirectory=list.map(a=>({id:String(a.id),symbol:String(a.symbol||'').toUpperCase(),name:String(a.name||a.nameid||''),rank:Number(a.rank)||999999,gold:false}));
    marketLoaded=true;renderMarketResults();
  }catch(e){out.textContent='asset search needs internet access';}
}
function renderMarketResults(){
  const out=document.getElementById('mResults');if(!marketLoaded){return;}
  const count=document.getElementById('mResultCount');const q=document.getElementById('mSearch').value.trim().toLowerCase();out.innerHTML='';
  if(!q){count.textContent='';out.textContent='type a symbol or asset name';return;}
  const terms=q.split(/[\s,;|/]+/).filter(Boolean),compact=q.replace(/[^a-z0-9]/g,'');
  const selectedIds=new Set(marketSelected.map(a=>(a.gold?'gold:':'coin:')+a.id));
  function matchScore(a){
    const symbol=a.symbol.toLowerCase(),name=a.name.toLowerCase();
    let best=999;
    terms.forEach(term=>{
      if(symbol===term)best=Math.min(best,0);
      else if(symbol.startsWith(term))best=Math.min(best,10);
      else if(name.split(/[\s-]+/).some(word=>word.startsWith(term)))best=Math.min(best,20);
      else if(name.startsWith(term))best=Math.min(best,25);
      else if(symbol.includes(term))best=Math.min(best,30);
      else if(name.includes(term))best=Math.min(best,40);
    });
    if(symbol.length>=2&&compact.includes(symbol))best=Math.min(best,50);
    return best;
  }
  const matches=marketDirectory.map(a=>({a,score:matchScore(a)})).filter(x=>x.score<999)
    .sort((x,y)=>x.score-y.score||x.a.rank-y.a.rank||x.a.symbol.localeCompare(y.a.symbol))
    .slice(0,10).map(x=>x.a);
  count.textContent=matches.length+' result'+(matches.length===1?'':'s');
  if(!matches.length){out.textContent='no matching assets';return;}
  matches.forEach(a=>{
    const row=document.createElement('div');row.className='mresult';
    const name=document.createElement('div');name.className='mresname';
    const strong=document.createElement('strong');strong.textContent=a.symbol;
    const small=document.createElement('small');small.textContent=a.name+(a.gold?' · spot gold':' · CoinLore #'+a.id);
    name.append(strong,small);row.appendChild(name);
    const add=document.createElement('button');add.className='madd';add.textContent=selectedIds.has((a.gold?'gold:':'coin:')+a.id)?'ADDED':'ADD';
    add.disabled=marketSelected.length>=5||selectedIds.has((a.gold?'gold:':'coin:')+a.id);add.onclick=()=>addMarket(a);row.appendChild(add);out.appendChild(row);
  });
}
async function saveMarket(){
  if(marketSaving){marketSaveQueued=true;return;}marketSaving=true;document.getElementById('mAuto').textContent='● SAVING...';
  try{const r=await fetch('/crypto/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({assets:marketSelected.map(a=>({id:a.id,symbol:a.symbol,name:a.name,gold:!!a.gold}))})});if(!r.ok)throw new Error();const j=await r.json();marketSelected=j.assets||marketSelected;document.getElementById('mAuto').textContent='● SAVED TO DEVICE';renderMarket();}
  catch(e){document.getElementById('mAuto').textContent='● SAVE FAILED';toast('save failed',false);}
  marketSaving=false;if(marketSaveQueued){marketSaveQueued=false;saveMarket();}
}
function addMarket(a){if(marketSelected.length>=5)return;marketSelected.push({...a,price:null,change:null});renderMarket();saveMarket();}
function removeMarket(i){if(marketSelected.length<=1){toast('keep at least one asset',false);return;}marketSelected.splice(i,1);renderMarket();saveMarket();}
function startMarketDrag(e,index,row){
  if(e.button!==undefined&&e.button!==0)return;e.preventDefault();
  const rows=[...document.querySelectorAll('#mSelected .mselrow')],box=document.getElementById('mSelected').getBoundingClientRect();
  marketDrag={index,target:index,row,startY:e.clientY,box,rowH:box.height/rows.length};
  row.classList.add('dragging');e.currentTarget.setPointerCapture(e.pointerId);
  document.addEventListener('pointermove',moveMarketDrag,{passive:false});document.addEventListener('pointerup',endMarketDrag,{once:true});
}
function moveMarketDrag(e){
  if(!marketDrag)return;e.preventDefault();const delta=e.clientY-marketDrag.startY;
  marketDrag.row.style.transform='translateY('+delta+'px)';
  marketDrag.target=Math.max(0,Math.min(marketSelected.length-1,Math.floor((e.clientY-marketDrag.box.top)/marketDrag.rowH)));
}
function endMarketDrag(){
  document.removeEventListener('pointermove',moveMarketDrag);
  if(!marketDrag)return;const from=marketDrag.index,to=marketDrag.target;marketDrag.row.classList.remove('dragging');marketDrag.row.style.transform='';
  marketDrag=null;if(from===to)return;const asset=marketSelected.splice(from,1)[0];marketSelected.splice(to,0,asset);renderMarket();saveMarket();
}
document.getElementById('mSearch').addEventListener('input',renderMarketResults);
function stockPrice(v){if(v==null)return'--';if(v>=10000)return Math.round(v).toString();if(v>=1000)return Number(v).toFixed(1);return Number(v).toFixed(2);}
function stockTime(){if(!stockUpdatedAt)return'UPDATED --:--';return'UPDATED '+stockUpdatedAt.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit',hour12:false});}
function renderStocks(){
  document.getElementById('sCount').textContent=stockSelected.length+' / 5';
  document.getElementById('sTime').textContent=stockTime();
  const preview=document.getElementById('sPreview');preview.innerHTML='';
  const selected=document.getElementById('sSelected');selected.innerHTML='';
  stockSelected.forEach((a,i)=>{
    const row=document.createElement('div');row.className='mrow';
    const sym=document.createElement('span');sym.className='msym';sym.textContent=a.label||a.code;if(sym.textContent.length>4)sym.style.fontSize='11px';row.appendChild(sym);
    const price=document.createElement('span');price.className='mprice';price.textContent=stockPrice(a.price);row.appendChild(price);
    const change=document.createElement('span');change.className='mchg';change.textContent=marketChange(a.change);row.appendChild(change);preview.appendChild(row);
    const item=document.createElement('div');item.className='mselrow';item.dataset.index=i;
    const handle=document.createElement('button');handle.className='mdrag';handle.type='button';handle.textContent='⠿';handle.setAttribute('aria-label','Drag '+a.code);
    handle.addEventListener('pointerdown',e=>startStockDrag(e,i,item));item.appendChild(handle);
    const name=document.createElement('div');name.className='mselname';
    const strong=document.createElement('strong');strong.textContent=a.code;
    const label=document.createElement('span');label.textContent=a.name;
    name.append(strong,label);item.appendChild(name);
    const remove=document.createElement('button');remove.className='mmini remove';remove.textContent='×';remove.setAttribute('aria-label','Remove '+a.code);remove.onclick=()=>removeStock(i);item.appendChild(remove);
    selected.appendChild(item);
  });
}
async function loadStockConfig(){
  try{const r=await fetch('/market/config',{cache:'no-store'});const j=await r.json();stockSelected=Array.isArray(j.assets)?j.assets:[];stockUpdatedAt=typeof j.updatedAgeSec==='number'?new Date(Date.now()-j.updatedAgeSec*1000):null;renderStocks();}
  catch(e){toast('market config unavailable',false);}
}
async function searchStocksNow(){
  const q=document.getElementById('sSearch').value.trim(),out=document.getElementById('sResults'),count=document.getElementById('sResultCount'),seq=++stockSearchSeq;
  out.innerHTML='';count.textContent='';
  if(!q){out.textContent='type a stock code or Chinese name';return;}
  out.textContent='searching...';
  try{
    const r=await fetch('/market/search?q='+encodeURIComponent(q),{cache:'no-store'});if(!r.ok)throw new Error();const j=await r.json();if(seq!==stockSearchSeq)return;
    const results=Array.isArray(j.results)?j.results:[],selectedIds=new Set(stockSelected.map(a=>a.secid));
    out.innerHTML='';count.textContent=results.length+' result'+(results.length===1?'':'s');
    if(!results.length){out.textContent='no matching A-share stocks';return;}
    results.forEach(a=>{
      const row=document.createElement('div');row.className='mresult';
      const name=document.createElement('div');name.className='mresname';
      const strong=document.createElement('strong');strong.textContent=a.code;
      const small=document.createElement('small');small.textContent=a.name;
      name.append(strong,small);row.appendChild(name);
      const add=document.createElement('button');add.className='madd';add.textContent=selectedIds.has(a.secid)?'ADDED':'ADD';
      add.disabled=stockSelected.length>=5||selectedIds.has(a.secid);add.onclick=()=>addStock(a);row.appendChild(add);out.appendChild(row);
    });
  }catch(e){if(seq===stockSearchSeq){count.textContent='';out.textContent='stock search unavailable';}}
}
document.getElementById('sSearch').addEventListener('input',()=>{clearTimeout(stockSearchTimer);stockSearchTimer=setTimeout(searchStocksNow,280);});
async function saveStocks(){
  if(stockSaving){stockSaveQueued=true;return;}stockSaving=true;document.getElementById('sAuto').textContent='● SAVING...';
  try{const r=await fetch('/market/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({assets:stockSelected.map(a=>({secid:a.secid,code:a.code,label:a.label||a.code,name:a.name}))})});if(!r.ok)throw new Error();const j=await r.json();stockSelected=j.assets||stockSelected;document.getElementById('sAuto').textContent='● SAVED TO DEVICE';renderStocks();}
  catch(e){document.getElementById('sAuto').textContent='● SAVE FAILED';toast('save failed',false);}
  stockSaving=false;if(stockSaveQueued){stockSaveQueued=false;saveStocks();}
}
function addStock(a){if(stockSelected.length>=5)return;stockSelected.push({...a,price:null,change:null});renderStocks();searchStocksNow();saveStocks();}
function removeStock(i){if(stockSelected.length<=1){toast('keep at least one stock',false);return;}stockSelected.splice(i,1);renderStocks();searchStocksNow();saveStocks();}
function startStockDrag(e,index,row){
  if(e.button!==undefined&&e.button!==0)return;e.preventDefault();
  const rows=[...document.querySelectorAll('#sSelected .mselrow')],box=document.getElementById('sSelected').getBoundingClientRect();
  stockDrag={index,target:index,row,startY:e.clientY,box,rowH:box.height/rows.length};
  row.classList.add('dragging');e.currentTarget.setPointerCapture(e.pointerId);
  document.addEventListener('pointermove',moveStockDrag,{passive:false});document.addEventListener('pointerup',endStockDrag,{once:true});
}
function moveStockDrag(e){
  if(!stockDrag)return;e.preventDefault();const delta=e.clientY-stockDrag.startY;
  stockDrag.row.style.transform='translateY('+delta+'px)';
  stockDrag.target=Math.max(0,Math.min(stockSelected.length-1,Math.floor((e.clientY-stockDrag.box.top)/stockDrag.rowH)));
}
function endStockDrag(){
  document.removeEventListener('pointermove',moveStockDrag);
  if(!stockDrag)return;const from=stockDrag.index,to=stockDrag.target;stockDrag.row.classList.remove('dragging');stockDrag.row.style.transform='';
  stockDrag=null;if(from===to)return;const asset=stockSelected.splice(from,1)[0];stockSelected.splice(to,0,asset);renderStocks();saveStocks();
}
async function useSerialMode(){if(!confirm('Switch Claude status input to USB serial? Local Web control stays available.'))return;await req('/serial_mode');toast('serial mode active');}
function escapeWifiText(value){const span=document.createElement('span');span.textContent=String(value??'');return span.innerHTML;}
function wifiStatusHtml(j){
  const ap='http://'+(j.apIp||'192.168.4.1'),mdns=j.mdns||'http://clawd-mochi.local';
  const line='font-size:9px;color:#8a8278;margin-top:3px';
  if(j.connected){const lan='http://'+(j.lanIp||j.ip);return '<strong style="color:#28b878">CONNECTED · '+escapeWifiText(j.ssid)+'</strong><small>LAN: '+escapeWifiText(lan)+'<br>Name: '+escapeWifiText(mdns)+'<br>Credentials: NVS (safe from static uploads)</small>';}
  if(j.changingNetwork)return '<strong style="color:#e0b341">VERIFYING NEW NETWORK</strong><small>Keep this page open. If it disconnects, join ClaWD-Mochi and open '+ap+'.</small>';
  if(j.configured&&j.savedSsid){const failed=Boolean(j.lastError)||j.retryCount>0||j.retryExhausted;const state=failed?(j.lastError||'Connection failed'):(j.phase||'Connecting'),retry=j.retryCount?' · retry '+j.retryCount:'';return '<strong style="color:#c96a3e">'+escapeWifiText(state+retry)+'</strong><small>Saved network kept: '+escapeWifiText(j.savedSsid)+'<br>Recovery AP: '+ap+'</small>';}
  return '<strong style="color:#c96a3e">WIFI SETUP NEEDED</strong><small>Join '+escapeWifiText(j.apSsid||'ClaWD-Mochi')+' and open '+ap+', then choose a network below.</small>';
}
function renderWifiStatus(j){
  lastWifiStatus=j;
  const dot=document.getElementById('wdot'),button=document.getElementById('wscanBtn');
  document.getElementById('wstatus').innerHTML=wifiStatusHtml(j);
  dot.className='wdot '+(j.connected?'connected':(j.changingNetwork?'connecting':''));
  button.style.display='block';
  button.textContent=j.connected?'CHANGE NETWORK':'CONNECT WIFI';
  if(!j.connected&&!j.configured){wifiSetupOpen=true;document.getElementById('wsetup').classList.add('open');}
}
function toggleWifiSetup(){wifiSetupOpen=!wifiSetupOpen;document.getElementById('wsetup').classList.toggle('open',wifiSetupOpen);if(wifiSetupOpen&&!document.getElementById('wlist').children.length)loadWifiList();}
function closeWifiSetup(){if(wifiConnecting)return;if(lastWifiStatus&&!lastWifiStatus.connected&&!lastWifiStatus.configured){toast('connect WiFi or use the recovery AP',false);return;}wifiSetupOpen=false;document.getElementById('wsetup').classList.remove('open');}
async function loadWifiList(){
  wifiSetupOpen=true;document.getElementById('wsetup').classList.add('open');
  const button=document.getElementById('wrescanBtn'),list=document.getElementById('wlist');
  button.disabled=true;button.textContent='SCANNING...';list.style.display='flex';list.textContent='Scanning nearby networks...';
  try{
    const r=await fetch('/wifi/scan',{cache:'no-store'}),nets=await r.json();list.innerHTML='';
    if(!Array.isArray(nets)||!nets.length){list.textContent='No networks found. You can type an SSID manually.';}
    else nets.sort((a,b)=>b.rssi-a.rssi).forEach(network=>{
      const row=document.createElement('button');row.type='button';row.className='wnet';
      const name=document.createElement('span');name.textContent=network.ssid||'(hidden network)';
      const meta=document.createElement('small');meta.textContent=(network.encrypted?'LOCK · ':'OPEN · ')+network.rssi+' dBm';
      row.append(name,meta);row.onclick=()=>{document.getElementById('wssid').value=network.ssid||'';document.getElementById('wpass').focus();};list.appendChild(row);
    });
  }catch(e){list.textContent='Scan failed. Type the network name manually.';}
  button.disabled=false;button.textContent='RESCAN';
}
async function connectWifi(){
  const ssid=document.getElementById('wssid').value.trim(),password=document.getElementById('wpass').value,button=document.getElementById('wconnectBtn');
  if(!ssid){toast('enter a network name',false);return;}
  if(password.length>0&&password.length<8){toast('password must be at least 8 characters',false);return;}
  wifiConnecting=true;button.disabled=true;button.textContent='VERIFYING...';
  document.getElementById('wstatus').innerHTML='<strong style="color:#e0b341">SWITCHING NETWORK</strong><small>If this page disconnects, join ClaWD-Mochi and open 192.168.4.1.</small>';
  const body=new URLSearchParams({ssid,password});
  try{const r=await fetch('/wifi/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});const result=await r.json();if(!r.ok)throw new Error(result.error||'could not start connection');}
  catch(e){wifiConnecting=false;button.disabled=false;button.textContent='CONNECT';toast(e.message||'connection request failed',false);return;}
  let connected=false;
  for(let i=0;i<50;i++){await new Promise(resolve=>setTimeout(resolve,1000));try{const r=await fetch('/wifi/status',{cache:'no-store'});if(!r.ok)continue;const status=await r.json();renderWifiStatus(status);if(status.connected&&status.ssid===ssid){connected=true;break;}if(status.connected&&status.ssid!==ssid)break;if(!status.changingNetwork&&status.lastError)break;}catch(e){}}
  wifiConnecting=false;button.disabled=false;button.textContent='CONNECT';
  if(connected){document.getElementById('wpass').value='';wifiSetupOpen=false;document.getElementById('wsetup').classList.remove('open');toast('wifi connected');}
  else toast('new network not connected; saved network was kept',false);
}
async function pollWifiStatus(){try{const r=await fetch('/wifi/status',{cache:'no-store'});if(r.ok)renderWifiStatus(await r.json());}catch(e){document.getElementById('wstatus').innerHTML='<strong style="color:#c96a3e">DEVICE UNREACHABLE</strong><small>Join ClaWD-Mochi and open 192.168.4.1.</small>';document.getElementById('wscanBtn').style.display='block';}}
async function toggleCanvas(){if(activeView===19)await leaveMediaForView();canvasOpen=!canvasOpen;document.getElementById('cwrap').classList.toggle('open',canvasOpen);document.querySelectorAll('.vbtn').forEach(btn=>btn.classList.toggle('active',canvasOpen&&parseInt(btn.dataset.v)===3));await req('/canvas?on='+(canvasOpen?1:0));if(canvasOpen){const bg=document.getElementById('bgCol').value;redrawCanvas(bg);await req('/draw/clear?bg='+encodeURIComponent(bg));document.querySelectorAll('.vbtn,.lbtn').forEach(b=>b.disabled=true);toast('canvas active');}else{setBusy(false);toast('canvas off');}}
const tin=document.getElementById('tin');let lastVal='';
tin.addEventListener('input',async()=>{const cur=tin.value,prev=lastVal;if(cur.length>prev.length){await req('/char?c='+encodeURIComponent(cur[cur.length-1]));}else if(cur.length<prev.length){await req('/char?c=%08');}lastVal=cur;});
async function termEnter(){await req('/char?c=%0A');tin.value='';lastVal='';tin.focus();}
tin.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();termEnter();}});
async function closeTerm(){await req('/cmd?k=q');termOpen=false;document.getElementById('twrap').classList.remove('open');document.getElementById('termPanel').classList.remove('open');setBusy(false);toast('terminal closed');}
const cvs=document.getElementById('cvs');const ctx=cvs.getContext('2d');let strokePts=[];   
function getPos(e){const r=cvs.getBoundingClientRect();const sx=cvs.width/r.width,sy=cvs.height/r.height;const s=e.touches?e.touches[0]:e;return{x:(s.clientX-r.left)*sx,y:(s.clientY-r.top)*sy};}
function redrawCanvas(hex){ctx.fillStyle=hex;ctx.fillRect(0,0,cvs.width,cvs.height);}
function startDraw(e){e.preventDefault();drawing=true;strokePts=[];const p=getPos(e);lastX=p.x;lastY=p.y;strokePts.push({x:Math.round(p.x),y:Math.round(p.y)});ctx.beginPath();ctx.arc(p.x,p.y,2,0,Math.PI*2);ctx.fillStyle=document.getElementById('penCol').value;ctx.fill();}
function moveDraw(e){if(!drawing)return;e.preventDefault();const p=getPos(e);ctx.beginPath();ctx.moveTo(lastX,lastY);ctx.lineTo(p.x,p.y);ctx.strokeStyle=document.getElementById('penCol').value;ctx.lineWidth=4;ctx.lineCap='round';ctx.stroke();strokePts.push({x:Math.round(p.x),y:Math.round(p.y)});lastX=p.x;lastY=p.y;}
async function endDraw(e){if(!drawing)return;drawing=false;if(!canvasOpen||strokePts.length<1)return;const pen=document.getElementById('penCol').value.replace('#','');const pts=strokePts.map(p=>p.x+','+p.y).join(';');await req('/draw/stroke?pen='+pen+'&pts='+encodeURIComponent(pts));strokePts=[];}
cvs.addEventListener('mousedown',startDraw);cvs.addEventListener('mousemove',moveDraw);cvs.addEventListener('mouseup',endDraw);cvs.addEventListener('mouseleave',endDraw);
cvs.addEventListener('touchstart',startDraw,{passive:false});cvs.addEventListener('touchmove',moveDraw,{passive:false});cvs.addEventListener('touchend',endDraw);
async function clearAll(){const bg=document.getElementById('bgCol').value;redrawCanvas(bg);await req('/draw/clear?bg='+encodeURIComponent(bg));toast('cleared');}
(async()=>{requestAnimationFrame(animateSalaryAmount);await loadExpressions();await loadProfile();await loadPrefs();await loadWeatherLocation();await loadOta();try{const r=await fetch('/state');const j=await r.json();const bv=typeof j.brightness==='number'?j.brightness:(j.bl===false?0:100);document.getElementById('bright').value=bv;document.getElementById('brightV').textContent=bv+'%';blOn=bv>0;updateBlButton();}catch(e){}initialLoadComplete=true;if(!testMode){pollWifiStatus();pollTimer();setInterval(pollTimer,1000);setInterval(pollSalary,1000);setInterval(pollTimetableStatus,30000);setInterval(loadOta,30000);setInterval(loadWeatherLocation,30000);setInterval(()=>{if(activeView===9&&!marketSaving&&!marketDrag)loadMarketConfig();if(activeView===10&&!stockSaving&&!stockDrag)loadStockConfig();},30000);}})();
