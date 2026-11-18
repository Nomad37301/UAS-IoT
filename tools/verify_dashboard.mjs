import { chromium } from '../.build/browser/node_modules/playwright-core/index.mjs';
import assert from 'node:assert/strict';
const browser = await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
try {
  const page = await browser.newPage({viewport:{width:1440,height:1100}});
  const errors=[]; page.on('pageerror',error=>errors.push(error.message));
  await page.goto('http://127.0.0.1:8000');
  await page.waitForFunction(()=>document.getElementById('state').textContent==='HEALTHY');
  await page.getByRole('button',{name:'Blokir probe C'}).click();
  await page.waitForFunction(()=>document.getElementById('state').textContent==='EXT_ISOLATED');
  assert.equal((await page.request.get('http://127.0.0.1:8000/health?probe=C')).status(),403);
  await page.getByRole('button',{name:'Service crash',exact:true}).click();
  await page.waitForFunction(()=>document.getElementById('state').textContent==='SERVICE_DOWN');
  await page.getByRole('button',{name:'Pulsa relay recovery'}).click();
  await page.waitForFunction(()=>document.getElementById('state').textContent==='HEALTHY');
  await page.route('**/api/v1/status',route=>route.abort());
  await page.waitForFunction(()=>document.getElementById('state').textContent==='DATA_STALE');
  await page.unroute('**/api/v1/status');
  await page.waitForFunction(()=>document.getElementById('state').textContent==='HEALTHY');
  await page.screenshot({path:'.build/dashboard-desktop.png',fullPage:true});
  await page.setViewportSize({width:390,height:844});
  await page.screenshot({path:'.build/dashboard-mobile.png',fullPage:true});
  assert.deepEqual(errors,[]);
  assert.equal(await page.locator('.protocol').count(),9);
  console.log('PASS: dashboard desktop/mobile, isolation, crash, recovery, API disconnect/stale, no JS errors');
} finally {await browser.close();}
