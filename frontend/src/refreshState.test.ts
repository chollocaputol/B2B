import test from 'node:test';
import assert from 'node:assert/strict';
import {snapshotPhase,selectionFor,isAccessFailure} from './refreshState.ts';
test('loading takes precedence over empty and previous error',()=>{assert.equal(snapshotPhase(true,false,0),'loading');assert.equal(snapshotPhase(true,true,0),'loading');});
test('successful empty and failed fetch are distinct',()=>{assert.equal(snapshotPhase(false,false,0),'empty');assert.equal(snapshotPhase(false,true,0),'error');assert.equal(snapshotPhase(false,false,2),'ready');});
test('empty snapshot does not invent a vessel',()=>assert.equal(selectionFor([],'3'),''));
test('valid preferred vessel survives refresh; absent vessel falls back',()=>{assert.equal(selectionFor([{id:'2'},{id:'3'}],'3'),'3');assert.equal(selectionFor([{id:'2'}],'3'),'2');});
test('access failures are not retryable data-only errors',()=>{for(const status of [401,403])assert.equal(isAccessFailure(status),true);assert.equal(isAccessFailure(400,'42501'),true);assert.equal(isAccessFailure(500),false);});
