const fs=require('fs'),path=require('path'),assert=require('assert');
const {chromium}=require('playwright');
const output=path.resolve(__dirname,'../build_rollback_checks');fs.mkdirSync(output,{recursive:true});
(async()=>{
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 const page=await browser.newPage({viewport:{width:736,height:450}}),errors=[];
 page.on('pageerror',e=>errors.push(String(e)));
 await page.clock.install({time:new Date('2026-11-12T02:00:00Z')});
 await page.clock.pauseAt(new Date('2026-11-12T02:00:01Z'));
 await page.setContent(fs.readFileSync(process.argv[2],'utf8'));
 const root=page.locator('#deskdock-rollback-preview');
 assert.equal(await page.locator('.rb-title').nth(1).textContent(),'螢幕省電');
 const titleY=await page.locator('.rb-title').first().evaluate(el=>el.getBoundingClientRect().y-el.parentElement.getBoundingClientRect().y);
 assert(Math.abs(titleY-3)<.1); // card border + the requested 2 px label origin
 await page.locator('.rb-lcd').screenshot({path:path.join(output,'general.png')});
 await page.locator('[data-open]').click();
 assert.equal(await root.getAttribute('data-stage'),'checking');
 assert(await page.locator('[data-rollback]').isDisabled());
 await page.clock.runFor(500);
 assert(await page.locator('[data-rollback]').isEnabled());
 await page.locator('.rb-lcd').screenshot({path:path.join(output,'system.png')});
 await page.locator('[data-rollback]').click();
 assert(await page.locator('.rb-dialog').isVisible());
 assert.equal(await root.getAttribute('data-boot-selections'),'0');
 await page.locator('.rb-lcd').screenshot({path:path.join(output,'confirmation.png')});
 await page.locator('[data-cancel]').click();
 assert(await page.locator('.rb-dialog').isHidden());
 assert.equal(await root.getAttribute('data-boot-selections'),'0');
 for(const value of ['no-slot','unconfirmed','invalid','unknown','newer','pending']){
  await page.selectOption('[data-scenario]',value);await page.clock.runFor(500);
  assert(await page.locator('[data-rollback]').isDisabled(),value);
  assert(await page.locator('[data-scan]').isEnabled(),value+' can retry');
 }
 await page.locator('.rb-lcd').screenshot({path:path.join(output,'unavailable.png')});
 for(const value of ['changed','audio']){
  await page.selectOption('[data-scenario]',value);await page.clock.runFor(500);
  await page.locator('[data-rollback]').click();await page.locator('[data-confirm]').click();
  assert.equal(await root.getAttribute('data-stage'),value);
  assert.equal(await root.getAttribute('data-boot-selections'),'0');
 }
 await page.selectOption('[data-scenario]','ready');await page.clock.runFor(500);
 await page.locator('[data-rollback]').click();await page.locator('[data-confirm]').click();
 assert.equal(await root.getAttribute('data-stage'),'rebooting');
 assert.equal(await root.getAttribute('data-boot-selections'),'1');
 await page.locator('[data-confirm]').dispatchEvent('click');
 assert.equal(await root.getAttribute('data-boot-selections'),'1');
 assert(await page.locator('[data-back]').isDisabled());
 assert(await page.locator('[data-update]').isDisabled());
 for(const width of [320,736]){await page.setViewportSize({width,height:450});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));}
 assert.deepEqual(errors,[]);
 console.log('PASS: general titles, verification/disabled states, cancel, revalidation failure, audio failure, confirmation, duplicate guard, busy navigation, 320/736px layout');
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
