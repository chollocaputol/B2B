// Run: node --experimental-strip-types --test tests/dtn_f06_history_validator.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import {parseF06SosJournal} from "../supabase/functions/_shared/f06_history_validator.ts";

const s = (extra = "") =>
  '{"kind":"sos_rx","vessel_id":2,"boot_token":12,"event_index":4,"uptime_ms":123' + extra + '}\n';
const read = (content, vessel = 2, session = "S58BAFE2") =>
  parseF06SosJournal(content, vessel, session);

test("historical SOS: accepts strict single record with correct identity", () => {
  const parsed = read(s());
  assert.equal(parsed.record_count, 1);
  assert.equal(parsed.records[0].body.kind, "sos_rx");
  assert.equal(parsed.records[0].event_index, 4);
});
test("historical SOS: accepts empty journal without fabricating an emergency", () => {
  assert.equal(read("").record_count, 0);
});
test("historical SOS: rejects truncated end of file", () => {
  assert.throws(() => read(s().slice(0, -1)), /incomplete final/);
});
test("historical SOS: rejects wrong vessel", () => {
  assert.throws(() => read(s(), 3), /identity/);
});
test("historical SOS: rejects flat JSON duplicate property", () => {
  assert.throws(() => read(s(',"vessel_id":2')), /duplicate JSON key/);
});
test("historical SOS: rejects nested JSON and invalid scalars", () => {
  assert.throws(() => read(s(',"extra":{"nested":true}')), /invalid JSON scalar/);
  assert.throws(() => read(s(',"extra":NaN')), /invalid JSON scalar/);
});
test("historical SOS: rejects repeat identity in same session", () => {
  assert.throws(() => read(s() + s()), /duplicate SOS event/);
});
test("historical SOS: rejects disguised telemetry", () => {
  assert.throws(() => read(s().replace("sos_rx", "telemetry_tx")), /unsupported SOS kind/);
});
test("historical SOS: requires isolated F06 session; LEGACY excluded", () => {
  assert.throws(() => read(s(), 2, "LEGACY"), /closed F06 session/);
});
test("historical SOS: enforces unsigned 32-bit identity", () => {
  assert.throws(() => read(s().replace('"boot_token":12', '"boot_token":4294967296')), /identity/);
  assert.throws(() => read(s().replace('"event_index":4', '"event_index":0')), /identity/);
});
test("historical SOS: accepts CRLF but not blank lines", () => {
  assert.equal(read(s().replace(/\n/g, "\r\n")).record_count, 1);
  assert.throws(() => read(s() + "\n"), /framing/);
});
