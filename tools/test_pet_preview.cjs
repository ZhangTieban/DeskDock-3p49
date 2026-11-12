const fs=require('fs');
const assert=require('assert');
const {chromium}=require('playwright');
const path=require('path');
const output=path.resolve(__dirname,'../build_pet_checks');
fs.mkdirSync(output,{recursive:true});
const preview=process.argv[2];
(async()=>{
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 const page=await browser.newPage({viewport:{width:736,height:700},deviceScaleFactor:1});
 const errors=[];page.on('pageerror',e=>errors.push(String(e)));
 await page.clock.install({time:new Date('2027-08-21T00:00:00Z')});
 await page.clock.pauseAt(new Date('2027-08-21T00:00:01Z'));
 await page.setContent(fs.readFileSync(preview,'utf8'));
 const dog=page.locator('.dock-dog');
 await page.locator('[data-action=pause]').click();
 const data=JSON.parse(fs.readFileSync(path.resolve(__dirname,'pet_scene.json'),'utf8'));
 for(const name of data.actions){
  await page.selectOption('[data-control=dog]',name);
  assert.equal(await dog.getAttribute('data-action'),name);
  await page.screenshot({path:path.join(output,name+'.png')});
  for(let i=0;i<60;i++)await page.locator('[data-action=step]').click();
  await page.screenshot({path:path.join(output,name+'-middle.png')});
 }
 await page.selectOption('[data-control=dog]','auto');
 const cases={'06:59':'sleep','07:00':'walk','07:59':'walk','08:00':'eat','08:09':'eat','08:10':'walk',
 '12:59':'walk','13:00':'sleep','13:59':'sleep','14:00':'walk','17:29':'walk','17:30':'leash','17:59':'leash',
 '18:00':'eat','18:29':'eat','18:30':'walk','21:59':'walk','22:00':'sleep','00:00':'sleep'};
 for(const [time,expected] of Object.entries(cases)){
  await page.locator('input[type=time]').fill(time);
  assert.equal(await dog.getAttribute('data-action'),expected,time);
 }
 const pixels=()=>page.locator('.dock-audio').evaluate(c=>Array.from(c.getContext('2d').getImageData(0,0,12,12).data));
 await page.selectOption('[data-control=audio]','none');assert((await pixels()).every(v=>v===0));
 await page.selectOption('[data-control=audio]','radio');const radio=await pixels();assert(radio.some(v=>v));
 await page.selectOption('[data-control=audio]','mp3');assert.notDeepEqual(await pixels(),radio);
 await page.selectOption('[data-control=audio]','paused');assert((await pixels()).every(v=>v===0));
 await page.locator('[data-control=hour12]').check();
 await page.locator('input[type=time]').fill('17:30');assert.equal(await page.locator('.dock-period').textContent(),'PM');
 assert.equal(await page.locator('.dock-clock').textContent(),'05:30');
 const layout=await page.evaluate(()=>{
  const parent=document.querySelector('.dock-lcd').getBoundingClientRect();
  const period=document.querySelector('.dock-period');
  const rect=period.getBoundingClientRect();
  return {x:rect.x-parent.x,y:rect.y-parent.y,size:parseFloat(getComputedStyle(period).fontSize),
    clockTop:parseFloat(getComputedStyle(document.querySelector('.dock-clock')).top),
    dateTop:parseFloat(getComputedStyle(document.querySelector('.dock-date')).top)};
 });
 for(const [key,expected] of Object.entries({x:247,y:78,size:14,clockTop:3,dateTop:111}))assert(Math.abs(layout[key]-expected)<.1,key);
 for(const time of ['00:00','11:11','12:22','18:58','23:59']){
  await page.locator('input[type=time]').fill(time);
  const bottoms=await page.evaluate(()=>['.dock-clock-bitmap','.dock-period-bitmap'].map(selector=>{
   const c=document.querySelector(selector),d=c.getContext('2d').getImageData(0,0,c.width,c.height).data;
   let last=-1;for(let y=0;y<c.height;y++)for(let x=0;x<c.width;x++)if(d[(y*c.width+x)*4+3])last=y;
   return parseFloat(getComputedStyle(c).top)+last;
  }));assert(Math.abs(bottoms[0]-bottoms[1])<.1,'AM/PM ink alignment '+time);
 }
 await page.locator('input[type=time]').fill('17:30');
 await page.locator('[data-action=heart]').click();
 assert(await dog.evaluate(c=>{const d=c.getContext('2d').getImageData(0,0,296,32).data;for(let i=0;i<d.length;i+=4)if(d[i]===247&&d[i+1]===139&&d[i+2]===166)return true;return false;}));
 await page.screenshot({path:path.join(output,'leash-heart.png')});
 await page.selectOption('[data-control=dog]','showcase');
 await page.locator('[data-action=pause]').click();
 for(let i=0;i<data.actions.length;i++){
  assert.equal(await dog.getAttribute('data-action'),data.actions[i]);
  await page.clock.runFor(data.frames[i].length*80);
 }
 assert.equal(await dog.getAttribute('data-action'),'walk');
 await page.locator('input[type=time]').fill('');
 assert.equal(await page.locator('.dock-clock').textContent(),'--:--');
 assert.equal(await page.locator('.dock-period').textContent(),'');
 await page.locator('input[type=date]').fill('');
 assert.equal(await page.locator('.dock-date').textContent(),'---- / -- / --');
 for(const width of [320,736]){
  await page.setViewportSize({width,height:700});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'overflow '+width);
 }
 assert.deepEqual(errors,[]);
 console.log('PASS: 11 motions and full showcase cycle, 19 schedule boundaries, source icons, 12-hour clock, heart overlay, 320/736px layout');
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
