// Rend le casque avec three.js (mode A, B, C ou D : voir README.md) dans Chromium et écrit
// $OUT_DIR/three<mode>_<n>.rgba (512 x 512, RGBA, lumière linéaire). Serveur HTTP sur le port 8765.
const {chromium}=require(process.env.PLAYWRIGHT || 'playwright');
const fs=require('fs');
(async()=>{
 const b=await chromium.launch({args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader','--ignore-gpu-blocklist']});
 const p=await b.newPage();
 p.on('console',m=>console.log('console:',m.text()));p.on('pageerror',e=>console.log('pageerror',e.message));
 await p.goto('http://127.0.0.1:8765/three/'+(process.env.PAGE||'page.html')+'?mode='+process.argv[2]+(process.env.EXTRA||''));
 await p.waitForFunction('window.__result',null,{timeout:240000});
 console.log(await p.evaluate('window.__info'));
 const r=await p.evaluate('window.__result');
 r.forEach((s,i)=>fs.writeFileSync(`${process.env.OUT_DIR || '.'}/${process.env.PREFIX||'three'}${process.argv[2]}_${i}.rgba`,Buffer.from(s,'base64')));
 await b.close();
})();
