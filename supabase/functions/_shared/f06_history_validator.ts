// Historical F06 SOS journal validator. Offline evidence only.
// This module cannot authorize a live SOS, device identity, or cloud upload.
const SOS_KINDS = new Set([
  "sos_rx", "sos_ack_tx", "sos_ack_rx", "sos_local_start",
  "sos_local_cancel", "sos_tx_attempt", "cancel_rx", "cancel_ack_rx",
  "cancel_ack_tx", "cancel_tx_attempt", "relay_tx"
]);
const U32 = 4294967295;

function fail(message) { throw new Error(message); }
function uint32(value, positive = false) {
  return typeof value === "number" && Number.isSafeInteger(value) &&
    value >= (positive ? 1 : 0) && value <= U32;
}

function strictFlatObject(text) {
  let i = 0;
  const space = () => { while (/[ \t\r\n]/.test(text[i] ?? "") && i < text.length) i++; };
  const readString = () => {
    const start = i;
    if (text[i++] !== '"') fail("expected JSON string");
    let escape = false;
    for (; i < text.length; i++) {
      const c = text[i];
      if (!escape && c === '"') {
        i++;
        try { return JSON.parse(text.slice(start, i)); }
        catch { fail("invalid JSON string"); }
      }
      if (!escape && c === "\\") { escape = true; continue; }
      escape = false;
    }
    fail("unterminated JSON string");
  };
  const value = () => {
    if (text[i] === '"') return readString();
    const remaining = text.slice(i);
    const match = /^(?:true|false|null|-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)/.exec(remaining);
    if (!match) fail("invalid JSON scalar");
    i += match[0].length;
    const v = JSON.parse(match[0]);
    if (typeof v === "number" && !Number.isFinite(v)) fail("nonfinite number");
    return v;
  };
  const obj = Object.create(null);
  const seen = new Set();
  space();
  if (text[i++] !== "{") fail("expected flat JSON object");
  space();
  if (text[i] === "}") fail("empty JSON object");
  let count = 0;
  while (i < text.length) {
    const key = readString();
    if (typeof key !== "string" || !/^[A-Za-z0-9_]{1,47}$/.test(key)) fail("unsupported JSON key");
    if (seen.has(key)) fail("duplicate JSON key");
    if (++count > 48) fail("too many JSON keys");
    seen.add(key);
    space();
    if (text[i++] !== ":") fail("missing JSON colon");
    space();
    obj[key] = value();
    space();
    const delimiter = text[i++];
    if (delimiter === "}") {
      space();
      if (i !== text.length) fail("trailing JSON content");
      return obj;
    }
    if (delimiter !== ",") fail("invalid JSON separator");
    space();
  }
  fail("unterminated JSON object");
}

export function parseF06SosJournal(content, vesselId, session) {
  if (typeof content !== "string" || content.length > 65536) fail("SOS file exceeds the staging size limit");
  if (!Number.isInteger(vesselId) || vesselId < 1 || vesselId > 9999) fail("invalid vessel number");
  if (typeof session !== "string" || !/^S[0-9A-F]{7}$/.test(session)) fail("closed F06 session required");
  if (content && !content.endsWith("\n")) fail("incomplete final JSONL record");
  for (let n = 0; n < content.length; n++) {
    const x = content.charCodeAt(n);
    if (x > 126 || (x < 32 && x !== 10 && x !== 13)) fail("non-ASCII/nonprintable journal byte");
  }
  const records = [];
  const identities = new Set();
  const lines = content ? content.slice(0, -1).split("\n") : [];
  if (lines.length > 128) fail("too many SOS events for a single staged file");
  for (const [index, rawLine] of lines.entries()) {
    const raw = rawLine.endsWith("\r") ? rawLine.slice(0, -1) : rawLine;
    if (raw.length < 2 || raw.length > 1024 || raw[0] !== "{" || raw[raw.length - 1] !== "}") {
      fail("invalid SOS JSONL framing");
    }
    const parsed = strictFlatObject(raw);
    if (!SOS_KINDS.has(parsed.kind) || parsed.vessel_id !== vesselId ||
        !uint32(parsed.boot_token) || !uint32(parsed.event_index, true) ||
        !uint32(parsed.uptime_ms)) fail("unsupported SOS kind or event identity");
    const identity = String(parsed.boot_token) + ":" + String(parsed.event_index);
    if (identities.has(identity)) fail("duplicate SOS event identity");
    identities.add(identity);
    records.push({ordinal: index + 1, raw_line: raw, body: parsed,
      boot_token: parsed.boot_token, event_index: parsed.event_index});
  }
  return {record_count: records.length, records};
}
