#include "dtn_f06_record_gate.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using DTNJsonlSnapshot::Journal;
using namespace DTNF06RecordGate;
static Verdict check(Journal j,uint32_t id,const char* text){
 Fields f{};return inspect(j,id,text,strlen(text),f);
}
int main(){
 const char* event="{\"kind\":\"rx_sos\",\"vessel_id\":2,\"boot_token\":42,\"event_index\":3,\"uptime_ms\":234,\"origin_id\":1,\"seq\":7,\"rssi\":-87,\"snr_x10\":12}";
 Fields f{};
 assert(inspect(Journal::SosEvents,2,event,strlen(event),f)==Verdict::CandidateUnverified);
 assert(f.vesselId==2U&&f.bootToken==42U&&f.eventIndex==3U&&f.uptimeMs==234U);
 assert(check(Journal::SosEvents,3,event)==Verdict::WrongVessel);
 assert(check(Journal::Telemetry,2,"{\"kind\":\"weather\",\"vessel_id\":2,\"boot_token\":0,\"event_index\":1,\"uptime_ms\":0,\"gps_utc\":null,\"pressure\":1008.1}")==Verdict::CandidateUnverified);
 assert(check(Journal::Diagnostics,2,"{\"kind\":\"boot\",\"vessel_id\":2,\"uptime_ms\":5}")==Verdict::DiagnosticsOnly);
 assert(check(Journal::Diagnostics,2,"{\"kind\":\"heartbeat\",\"vessel_id\":2}")==Verdict::DiagnosticsOnly);
 assert(check(Journal::SosEvents,2,"{broken}")==Verdict::InvalidJson);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"a\",\"kind\":\"b\",\"vessel_id\":2,\"event_index\":1,\"boot_token\":1,\"uptime_ms\":0}")==Verdict::InvalidJson);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":2,\"boot_token\":1,\"uptime_ms\":0}")==Verdict::InvalidFields);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":2,\"boot_token\":1,\"event_index\":0,\"uptime_ms\":0}")==Verdict::InvalidFields);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":02,\"boot_token\":1,\"event_index\":1,\"uptime_ms\":0}")==Verdict::InvalidJson);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":4294967296,\"boot_token\":1,\"event_index\":1,\"uptime_ms\":0}")==Verdict::InvalidJson);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":2,\"boot_token\":1,\"event_index\":1,\"uptime_ms\":0,\"foo\":1e3}")==Verdict::CandidateUnverified);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":2,\"boot_token\":1,\"event_index\":1,\"uptime_ms\":0,\"foo\":1e}")==Verdict::InvalidJson);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":2,\"boot_token\":1,\"event_index\":1,\"uptime_ms\":0,\"extra\":{\"nested\":true}}")==Verdict::InvalidJson);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":2,\"boot_token\":1,\"event_index\":1,\"uptime_ms\":0,\"data\":\"hello\\nworld\"}")==Verdict::CandidateUnverified);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":2,\"boot_token\":1,\"event_index\":1,\"uptime_ms\":0,\"data\":\"bad\nstring\"}")==Verdict::InvalidJson);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":2,\"boot_token\":1,\"event_index\":1,\"uptime_ms\":0,\"data\":\"bad\\x\"}")==Verdict::InvalidJson);
 assert(check(Journal::SosEvents,2,"{\"kind\":\"x\",\"vessel_id\":2,\"boot_token\":1,\"event_index\":1,\"uptime_ms\":0}trailing")==Verdict::InvalidJson);
 assert(check(static_cast<Journal>(99),2,event)==Verdict::UnsupportedInput);
 DTNJsonlSnapshot::Record r{};r.ordinal=1;r.start=29;r.end=76;r.fnv1a=0xCABC1234UL;
 char locator[160];
 assert(localLocator(2,"S400ADB0",Journal::SosEvents,r,locator,sizeof(locator)));
 assert(strcmp(locator,"V0002/S400ADB0/sos_events.jsonl#1@29-76:CABC1234")==0);
 assert(!localLocator(2,"../S400ADB0",Journal::Telemetry,r,locator,sizeof(locator)));
 assert(!localLocator(2,"S400ADB0",Journal::SosEvents,r,locator,3));
 std::puts("DTN F06 strict flat-JSON / wrong-vessel / local locator: PASS");
}
