import test from 'node:test';
import assert from 'node:assert/strict';
import {entryRoute,sections,sectionPath,sectionAt,sectionFromLocation,operatorDestination} from './routes.ts';
const host = 'bayanihan-b2b.netlify.app';
test('public landing never selects fictional or protected data',()=>{
 assert.equal(entryRoute('/','',host,false),'landing');
 assert.equal(entryRoute('/','?study=0',host,false),'landing');
});
test('professional routes and trailing slash select operator gate',()=>{
 for(const p of ['/login','/dashboard','/login/','/dashboard/']) assert.equal(entryRoute(p,'',host,false),'operator');
});
test('legacy link remains compatible only at root',()=>{
 assert.equal(entryRoute('/','?study=1&utm_source=chatgpt.com',host,false),'legacy');
 assert.equal(entryRoute('/unknown','?study=1',host,false),'not-found');
});
test('demo is only available in loopback development',()=>{
 assert.equal(entryRoute('/demo','','127.0.0.1',true),'demo');
 for(const [h,d] of [[host,true],[host,false],['127.0.0.1',false],['localhost',true]] as const) assert.equal(entryRoute('/demo','',h,d),'not-found');
});
test('unrecognized paths never select an operator or demo entry',()=>{
 for(const p of ['/dashboard/unknown','/admin','/Login']) assert.equal(entryRoute(p,'',host,false),'not-found');
});

test('all five exact dashboard sections and trailing slashes share operator gate',()=>{
 for(const s of sections){const path=sectionPath(s);assert.equal(entryRoute(path,'',host,false),'operator');assert.equal(entryRoute(path+'/','',host,false),'operator');assert.equal(sectionAt(path),s);}
});
test('demo section paths remain loopback development only',()=>{
 for(const s of sections){const path=sectionPath(s,true);assert.equal(entryRoute(path,'','127.0.0.1',true),'demo');assert.equal(entryRoute(path,'',host,false),'not-found');assert.equal(entryRoute(path,'','127.0.0.1',false),'not-found');}
});
test('legacy dashboard anchors resolve to canonical sections',()=>{
 for(const [hash,s] of [['#overview','overview'],['#devices','vessels'],['#weather','weather'],['#alerts','sos'],['#readings','records']] as const)assert.equal(operatorDestination('/dashboard',hash),sectionPath(s));
});
test('explicit section overrides obsolete fragment and login defaults to overview',()=>{
 assert.equal(operatorDestination('/dashboard/sos','#devices'),'/dashboard/sos');assert.equal(operatorDestination('/login',''),'/dashboard');assert.equal(sectionFromLocation('/demo/records','#weather',true),'records');
});
test('unknown nested dashboard paths are not authorized entry routes',()=>{
 for(const path of ['/dashboard/weather/extra','/dashboard/overview','/dashboard/Weather','/demo/unknown'])assert.equal(entryRoute(path,'',host,false),'not-found');
});

test('geolocation and logs are canonical routes while old links remain authorized aliases',()=>{
 assert.equal(sectionPath('vessels'),'/dashboard/geolocation');
 assert.equal(sectionPath('records'),'/dashboard/logs');
 for(const [old,next] of [['vessels','geolocation'],['records','logs']] as const){
  assert.equal(entryRoute('/dashboard/'+old,'',host,false),'operator');
  assert.equal(operatorDestination('/dashboard/'+old,''),'/dashboard/'+next);
  assert.equal(operatorDestination('/dashboard/'+next+'/', ''),'/dashboard/'+next);
 }
});
test('renamed demo aliases remain restricted to loopback development',()=>{
 for(const path of ['/demo/geolocation','/demo/logs','/demo/vessels','/demo/records']){
  assert.equal(entryRoute(path,'','127.0.0.1',true),'demo');
  assert.equal(entryRoute(path,'',host,false),'not-found');
  assert.equal(entryRoute(path,'','127.0.0.1',false),'not-found');
 }
 assert.equal(entryRoute('/dashboard/logs/extra','',host,false),'not-found');
});
