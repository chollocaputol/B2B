import test from 'node:test';
import assert from 'node:assert/strict';
import { studyAccessMode } from './studyAccessMode.ts';
const hosted = 'https://twqumqiplqqfhixjvxeo.supabase.co';
const site = 'bayanihan-b2b.netlify.app';
const local = 'http://127.0.0.1:54321';
test('existing local study access is preserved', () => {
  assert.equal(studyAccessMode('127.0.0.1', 'http:', local), 'local');
});
test('approved Netlify site uses the approved HTTPS project', () => {
  assert.equal(studyAccessMode(site, 'https:', hosted), 'hosted');
  assert.equal(studyAccessMode(site, 'https:', ` ${hosted} `), 'hosted');
});
test('wrong host, wrong project, insecure or missing URL fail closed', () => {
  for (const [host, protocol, url] of [
    [site, 'https:', undefined], [site, 'http:', hosted], [site, 'https:', local],
    [site, 'https:', hosted + '.example.org'], ['preview-' + site, 'https:', hosted],
    ['localhost', 'http:', local], ['127.0.0.1', 'http:', hosted],
    ['example.org', 'https:', hosted], [site, 'https:', 'https://example.org'],
  ]) assert.equal(studyAccessMode(host!, protocol!, url), null);
});
