import test from 'node:test';
import assert from 'node:assert/strict';
import { studyAccessMode, externalBasemapEnabled } from './studyAccessMode.ts';
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

test('fictional demo keeps its existing basemap in every access mode', () => {
  for (const mode of ['hosted', 'local', null] as const) {
    for (const optedIn of [false, true]) assert.equal(externalBasemapEnabled(false, mode, optedIn), true);
  }
});
test('synchronized hosted map requires an explicit opt-in', () => {
  const mode = studyAccessMode(site, 'https:', hosted);
  assert.equal(externalBasemapEnabled(true, mode, false), false);
  assert.equal(externalBasemapEnabled(true, mode, true), true);
});
test('synchronized local and unapproved environments never request tiles', () => {
  for (const mode of [studyAccessMode('127.0.0.1', 'http:', local), studyAccessMode('preview-' + site, 'https:', hosted), studyAccessMode(site, 'https:', local), null]) {
    for (const optedIn of [false, true]) assert.equal(externalBasemapEnabled(true, mode, optedIn), false);
  }
});
