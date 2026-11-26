import test from 'node:test';
import assert from 'node:assert/strict';
import {entryRoute} from './routes.ts';
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
