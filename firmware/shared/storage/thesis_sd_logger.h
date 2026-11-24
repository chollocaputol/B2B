// F23D SD begin
#include "../relay/f23d_sd_contract.h"
#if THESIS_F23D_SD_EVIDENCE
#include "../relay/f23d_sd_journal.h"
#endif
// F23D SD end
#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
// F24D build-only bridge; default-disabled, never schedules SD access.
#ifndef THESIS_DTN_F24D_BUILD_ONLY
#define THESIS_DTN_F24D_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24D_BUILD_ONLY != 0 && THESIS_DTN_F24D_BUILD_ONLY != 1
#error "F24D preview flag must be 0 or 1"
#endif
#if THESIS_DTN_F24D_BUILD_ONLY == 1
#include "dtn_f24d_arduino_read_port.h"
#endif
// F24E: no physical grant and no new SD access in the build-only preview.
#ifndef THESIS_DTN_F24E_BUILD_ONLY
#define THESIS_DTN_F24E_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24E_BUILD_ONLY != 0 && THESIS_DTN_F24E_BUILD_ONLY != 1
#error "F24E build-only flag must be 0 or 1"
#endif
#if THESIS_DTN_F24E_BUILD_ONLY == 1
#if THESIS_DTN_F24D_BUILD_ONLY != 1
#error "F24E preview requires F24D build-only dependency"
#endif
#include "dtn_f24e_sd_owner_scheduler.h"
#endif
// F24F: compile-only SOS close lease. The default is never authorized.
#ifndef THESIS_DTN_F24F_BUILD_ONLY
#define THESIS_DTN_F24F_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24F_BUILD_ONLY != 0 && THESIS_DTN_F24F_BUILD_ONLY != 1
#error "F24F preview flag must be 0 or 1"
#endif
#if THESIS_DTN_F24F_BUILD_ONLY == 1
#if THESIS_DTN_F24E_BUILD_ONLY != 1
#error "F24F preview requires F24E build-only scheduler"
#endif
#include "dtn_f24f_sos_closed_lease.h"
#endif
// F24G: compile-only SOS seal candidate; all physical operations are denied.
#ifndef THESIS_DTN_F24G_BUILD_ONLY
#define THESIS_DTN_F24G_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24G_BUILD_ONLY != 0 && THESIS_DTN_F24G_BUILD_ONLY != 1
#error "F24G preview flag must be 0 or 1"
#endif
#if THESIS_DTN_F24G_BUILD_ONLY == 1
#if THESIS_DTN_F24F_BUILD_ONLY != 1
#error "F24G preview requires F24F build-only gate"
#endif
#include "dtn_f24g_sos_seal.h"
#endif
// F24H: writer retirement state model; no real SD-owner port is authorized.
#ifndef THESIS_DTN_F24H_BUILD_ONLY
#define THESIS_DTN_F24H_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24H_BUILD_ONLY != 0 && THESIS_DTN_F24H_BUILD_ONLY != 1
#error "F24H preview flag must be 0 or 1"
#endif
#if THESIS_DTN_F24H_BUILD_ONLY == 1
#if THESIS_DTN_F24G_BUILD_ONLY != 1
#error "F24H preview requires F24G build-only contract"
#endif
#include "dtn_f24h_writer_retirement.h"
#endif
// F24I: independent-witness bridge, physical owner absent, deny by default.
#ifndef THESIS_DTN_F24I_BUILD_ONLY
#define THESIS_DTN_F24I_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24I_BUILD_ONLY != 0 && THESIS_DTN_F24I_BUILD_ONLY != 1
#error "F24I preview flag must be 0 or 1"
#endif
#if THESIS_DTN_F24I_BUILD_ONLY == 1
#if THESIS_DTN_F24H_BUILD_ONLY != 1
#error "F24I preview requires F24H build-only retirement model"
#endif
#include "dtn_f24i_sd_owner_bridge.h"
#endif
// F24J: host-only arbiter; all existing physical SD paths remain unclaimed.
#ifndef THESIS_DTN_F24J_BUILD_ONLY
#define THESIS_DTN_F24J_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24J_BUILD_ONLY != 0 && THESIS_DTN_F24J_BUILD_ONLY != 1
#error "F24J preview flag must be 0 or 1"
#endif
#if THESIS_DTN_F24J_BUILD_ONLY == 1
#if THESIS_DTN_F24I_BUILD_ONLY != 1
#error "F24J requires F24I deny-only bridge"
#endif
#include "dtn_f24j_owner_coordinator.h"
#endif
#include <XPowersLib.h>
// F22R3C opt-in logger CONTRACT. NO SD mount/reads/writes in this profile.
#ifndef THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT
#define THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT 0
#endif
#if THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT != 0 && THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT != 1
#error "Invalid F22R3C logger contract flag"
#endif
#if THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT == 1
#if THESIS_F22R3B_STORAGE_POLICY_PREVIEW != 1 || THESIS_F22R3A_STANDARD_FLEET_PREVIEW != 1 || \
    THESIS_V003_F15_SD_PILOT_ARMED != 0 || THESIS_V003_F13_LIVE_OUTPUTS != 0 || THESIS_LAB_RELAY_TX != 0
#error "F22R3C logger contract requires multi-ID build-only, SD pilot OFF, GPIO/relay OFF"
#endif
#include "f22_fleet_storage_policy.h"
#include "../hardware/vessel_identity.h"
#endif
// F22R3D dry-run: no source of approved media/board consent exists yet.
#ifndef THESIS_F22_FLEET_OPERATIONAL
#define THESIS_F22_FLEET_OPERATIONAL 0
#endif
#if THESIS_F22_FLEET_OPERATIONAL == 1
#include "../hardware/vessel_identity.h"
#include "../hardware/f22_universal_capability_policy.h"
#endif
#ifndef THESIS_F22R3D_FLEET_SD_DRYRUN
#define THESIS_F22R3D_FLEET_SD_DRYRUN 0
#endif
#if THESIS_F22R3D_FLEET_SD_DRYRUN != 0 && THESIS_F22R3D_FLEET_SD_DRYRUN != 1
#error "Invalid F22R3D SD dry-run flag"
#endif
#if THESIS_F22R3D_FLEET_SD_DRYRUN == 1
#if THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT != 1 || THESIS_V003_F19_DUAL_DISPLAY != 1 || \
    THESIS_V003_F15_SD_PILOT_ARMED != 0 || THESIS_V003_F13_LIVE_OUTPUTS != 0 || THESIS_LAB_RELAY_TX != 0
#error "F22R3D dry-run needs F22R3C failclosed, F19 build-only, SD/relay/GPIO OFF"
#endif
#include "f22_fleet_storage_runtime_gate.h"
#endif
#ifndef THESIS_F07_WEATHER_LOG
#define THESIS_F07_WEATHER_LOG 0
#endif
#if THESIS_F07_WEATHER_LOG == 1
#include "../../universal/include/weather_manager.h"
#endif
#ifndef THESIS_F06_SD_HYBRID
#define THESIS_F06_SD_HYBRID 0
#endif
#if THESIS_F06_SD_HYBRID != 0 && THESIS_F06_SD_HYBRID != 1
#error "THESIS_F06_SD_HYBRID must be 0 or 1"
#endif
#if THESIS_F06_SD_HYBRID == 1
#include "sd_hybrid_policy.h"
#endif

// 0: original Vessel-002-only experimental profile; 1: identity-aware F11 profile.
#ifndef THESIS_SD_FLEET
#define THESIS_SD_FLEET 0
#endif
#if THESIS_SD_FLEET != 0 && THESIS_SD_FLEET != 1
#error "THESIS_SD_FLEET must be 0 or 1"
#endif

// F06 bench-only mode: create new per-boot SD journals in an isolated directory.
#ifndef THESIS_F06_SD_SANDBOX
#define THESIS_F06_SD_SANDBOX 0
#endif
#if THESIS_F06_SD_SANDBOX != 0 && THESIS_F06_SD_SANDBOX != 1
#error "THESIS_F06_SD_SANDBOX must be 0 or 1"
#endif
#if THESIS_F06_SD_HYBRID == 1 && (THESIS_F06_SD_SANDBOX != 1 || THESIS_SD_FLEET != 1 || THESIS_F05_RECOVERY != 1 || THESIS_SD_OLED_UI != 1)
#error "F06 hybrid requires F05 + sandbox + fleet logger + OLED UI"
#endif
#if THESIS_F06_SD_SANDBOX == 1
#if THESIS_SD_FLEET != 1 || THESIS_F05_RECOVERY != 1
#error "F06 SD sandbox requires both fleet SD logger and F05 recovery"
#endif
#if THESIS_LAB_RELAY_TX == 1
#error "F06 SD sandbox cannot enable unauthenticated lab relay TX"
#endif
#endif

// F24K: actual ThesisSDLogger SD entrypoints pass through one named facade.
// Legacy profiles alias SD byte-for-byte. Only the F24K build-only profile
// selects the permanently denied implementation; other SD owners not covered.
#ifndef THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY
#define THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 0 && THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1
#error "Invalid F24K build-only flag"
#endif
#if THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY == 1
#if THESIS_DTN_F24J_BUILD_ONLY != 1 || THESIS_DTN_F24I_BUILD_ONLY != 1 || THESIS_DTN_F24H_BUILD_ONLY != 1
#error "F24K requires F24J/I/H build-only model chain"
#endif
#if THESIS_V003_F15_SD_PILOT_ARMED != 0 || THESIS_F23D_SD_EVIDENCE != 0 || THESIS_F22_FLEET_OPERATIONAL != 0
#error "F24K forbids physical pilot, F23D independent writer, and fleet SD activation"
#endif
#if THESIS_LAB_RELAY_TX != 0 || THESIS_V003_F13_LIVE_OUTPUTS != 0
#error "F24K requires legacy relay/GPIO outputs off"
#endif
#include "dtn_f24k_sd_interposition.h"
#endif
// F24M: real firmware SOS packet capture seams, but no authorized SD owner.
// Only the permanently denied F24K SD profile may activate this build-only
// preview. Never enable on an operational or F23D physical SD profile.
#ifndef THESIS_DTN_F24M_BUILD_ONLY
#define THESIS_DTN_F24M_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24M_BUILD_ONLY != 0 && THESIS_DTN_F24M_BUILD_ONLY != 1
#error "F24M build-only flag must be zero or one"
#endif
#if THESIS_DTN_F24M_BUILD_ONLY == 1
#if THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1 || THESIS_F23D_SD_EVIDENCE != 0 || THESIS_SD_LOGGER != 1
#error "F24M requires F24K DENY SD, logger enabled, independent F23D writer OFF"
#endif
#include "dtn_f24m_sos_packet_store.h"
#endif
// F24N: incremental full-frame cold-restart reconstruction in the actual
// logger main-loop path. Reuses permanently denied F24K/F24M build profile.
#ifndef THESIS_DTN_F24N_BUILD_ONLY
#define THESIS_DTN_F24N_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24N_BUILD_ONLY != 0 && THESIS_DTN_F24N_BUILD_ONLY != 1
#error "F24N build-only flag must be zero or one"
#endif
#if THESIS_DTN_F24N_BUILD_ONLY == 1
#if THESIS_DTN_F24M_BUILD_ONLY != 1 || THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1 || THESIS_F23D_SD_EVIDENCE != 0 || THESIS_F06_SD_SANDBOX != 1
#error "F24N requires F24M/F24K permanently denied F06 sandbox, F23D OFF"
#endif
#include "dtn_f24n_sos_restart_recovery.h"
#endif
// F24O: owner-bound packet/recovery adapter, host-executable and firmware
// typechecked against a DENY witness; no physical SD permission is installed.
#ifndef THESIS_DTN_F24O_BUILD_ONLY
#define THESIS_DTN_F24O_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24O_BUILD_ONLY != 0 && THESIS_DTN_F24O_BUILD_ONLY != 1
#error "F24O build-only flag must be zero or one"
#endif
#if THESIS_DTN_F24O_BUILD_ONLY == 1
#if THESIS_DTN_F24N_BUILD_ONLY != 1 || THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1 || THESIS_F23D_SD_EVIDENCE != 0 || THESIS_F22_FLEET_OPERATIONAL != 0
#error "F24O only extends F24N/F24K SD denial; F23D and fleet storage must be OFF"
#endif
#include "dtn_f24o_owned_sos_port.h"
#endif
// F24Q: source-only cross-writer arbitration model + F23D volume entrypoint
// naming. NO physical observer or File handle interception is provided.
#ifndef THESIS_DTN_F24Q_BUILD_ONLY
#define THESIS_DTN_F24Q_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24Q_BUILD_ONLY != 0 && THESIS_DTN_F24Q_BUILD_ONLY != 1
#error "Invalid F24Q build-only flag"
#endif
#if THESIS_DTN_F24Q_BUILD_ONLY == 1
#if THESIS_DTN_F24P_BUILD_ONLY != 1 || THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1 || \
    THESIS_F23D_SD_EVIDENCE != 0 || THESIS_F22_FLEET_OPERATIONAL != 0 || \
    THESIS_LAB_RELAY_TX != 0 || THESIS_V003_F13_LIVE_OUTPUTS != 0
#error "F24Q requires F24P/K DENY; F23D, fleet, legacy relay and GPIO OFF"
#endif
#include "dtn_f24q_cross_writer_arbiter.h"
#endif
// F24P: integrate F24N root discovery with the SAME owner-bound F24O port.
// This is still NOT global physical all-file-handle ownership; the legacy
// logger and F23D remain separate and the F24K facade permanently denies SD.
#ifndef THESIS_DTN_F24P_BUILD_ONLY
#define THESIS_DTN_F24P_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24P_BUILD_ONLY != 0 && THESIS_DTN_F24P_BUILD_ONLY != 1
#error "Invalid F24P build-only flag"
#endif
#if THESIS_DTN_F24P_BUILD_ONLY == 1
#if THESIS_DTN_F24O_BUILD_ONLY != 1 || THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1 || \
    THESIS_F23D_SD_EVIDENCE != 0 || THESIS_F22_FLEET_OPERATIONAL != 0 || \
    THESIS_LAB_RELAY_TX != 0 || THESIS_V003_F13_LIVE_OUTPUTS != 0
#error "F24P requires F24O/K DENY, F23D/fleet/legacy relay/physical GPIO OFF"
#endif
#endif
namespace ThesisSDLogger {
#if THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY == 1
// This object has NO SD backend: no mount, open or physical File handle.
static DTNF24K::DenySdFacade<File> interposedSD;
#if THESIS_DTN_F24O_BUILD_ONLY == 1
// Denial is permanent even if the outer port is accidentally requested.
static const DTNF24O::DenyWitness f24oNoPhysicalWitness{};
static DTNF24O::Port<decltype(interposedSD),DTNF24O::DenyWitness>
    f24oSosPort(interposedSD,f24oNoPhysicalWitness);
static bool f24oBindAttempted=false;
inline void f24oBindOnce(uint32_t vessel,const char* dir){
  if(f24oBindAttempted)return;
  f24oBindAttempted=true;
  (void)f24oSosPort.bind(vessel,dir); // MUST deny; never retries
}
#endif
#else
// Preserve existing physical logger behavior for every non-F24K profile.
// This alias is NOT an exclusive owner, a lease or a replay permission.
static auto& interposedSD = SD;
#endif
#if THESIS_DTN_F24D_BUILD_ONLY == 1
// Compiler-only seam. Gate always rejects; NO SD.open and no runtime caller.
struct F24DNoPhysicalPermission {
  bool allowClosedJournal(const DTNAckLedger::Stream&, const char*) const {return false;}
};
using F24DBuildOnlyJob=DTNF24D::Job<DTNF24DArduino::ReadFile,DTNF24DArduino::ReadVolume>;
inline DTNF24D::Result f24dBuildOnlyDeny(const DTNAckLedger::Stream& stream){
  DTNF24DArduino::ReadVolume volume;
  F24DBuildOnlyJob job;
  F24DNoPhysicalPermission deny;
  const auto first=job.begin(volume,stream,deny);
  (void)job.step(deny,true,DTNF24D::kChunkBytes);
  return first;
}
#endif
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
inline bool f23dStorageFrozen();
#endif
// F23D SD end

static SPIClass sdSPI(HSPI);
static bool ready = false;
static uint32_t lastHeartbeat = 0;
static uint32_t vesselId = 0;

static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000UL;

// Runtime paths are derived from validated NVS identity. For ID 2,
// they remain byte-for-byte identical to the existing SD journal paths.
static char DIRECTORY[64] = "/THESIS/VESSEL_002";
static char DIAGNOSTICS[64] = "/THESIS/VESSEL_002/diagnostics.jsonl";
inline bool appendRecord(const char* path, const char* record);
static char TELEMETRY[64] = "/THESIS/VESSEL_002/telemetry.jsonl";
static char SOS_EVENTS[64] = "/THESIS/VESSEL_002/sos_events.jsonl";
#if THESIS_DTN_F24M_BUILD_ONLY == 1
// Actual radio packet ingress -> bounded RAM queue; NO storage in RX/TX path.
// F24K facade is denied, therefore the preview cannot mount or write a card.
static DTNF24M::PendingQueue<8> f24mSosCapture;
static DTNF24M::SessionWriter f24mSosJournal;
inline void pollF24MOne(){
  if(!ready || !f24mSosCapture.pending() || f24mSosCapture.faulted())return;
  const DTNF24M::PacketEntry* item=f24mSosCapture.front();
  if(!item)return;
#if THESIS_DTN_F24O_BUILD_ONLY == 1
  f24oBindOnce(vesselId,DIRECTORY);
  auto& selectedSosPort=f24oSosPort;
#else
  auto& selectedSosPort=interposedSD;
#endif
  if(!f24mSosJournal.appendChecked(selectedSosPort,DIRECTORY,vesselId,*item,
                                   FILE_APPEND,FILE_READ)){
    f24mSosCapture.latchStorageFault();
    Serial.println("[F24M] SOS_PACKET_STORE_UNCONFIRMED; RAM_PENDING; SD_OWNER_DENIED");
    return;
  }
  (void)f24mSosCapture.acknowledgeDurableAppend();
}
#endif

#if THESIS_DTN_F24N_BUILD_ONLY == 1
// A real code path, but unreachable in the only permitted F24N image:
// F24K's SD.begin() always rejects, hence 'ready' never becomes true.
// Host tests prove the same Replay object against real disposable files.
// This is NOT independent media proof, an approved reader lease or RF replay.
static DTNF24N::Replay<32,12> f24nClosedReplay;
static bool f24nDiscoveryTried=false;
static bool f24nDiscoveryFailed=false;
inline bool f24nDiscoverClosedSessions(){
  if(f24nDiscoveryTried || !ready)return false;
  f24nDiscoveryTried=true;
#if THESIS_DTN_F24O_BUILD_ONLY == 1
  // F24O cannot promote the legacy root directory scanner into an owner.
  // Until its directory handles are fully interposed, fail closed before
  // even attempting discovery through interposedSD.
  f24oBindOnce(vesselId,DIRECTORY);
  if(!f24oSosPort.bound())return false;
#endif
  if(!f24nClosedReplay.begin(vesselId,DIRECTORY))return false;
  char root[48]{};
  const int n=snprintf(root,sizeof(root),"/THESIS/F06TEST/V%03lu",
                       (unsigned long)vesselId);
  if(n<=0 || size_t(n)>=sizeof(root))return false;
#if THESIS_DTN_F24P_BUILD_ONLY == 1
  // F24P uses F24O's same single owner port, not the raw logger File.
  // DenyWitness makes this unreachable on the board; host tests inject
  // a synthetic witness and inspect the bounded, read-only traversal.
  return f24oSosPort.scanClosedSessions(root,FILE_READ,
    [](const char* session){return f24nClosedReplay.addClosed(session);}) &&
    f24nClosedReplay.lock();
#else
  File folder=interposedSD.open(root,FILE_READ);
  if(!folder || !folder.isDirectory()){
    if(folder)folder.close();return false;
  }
  bool healthy=true;
  bool exhausted=false;
  for(unsigned visited=0;visited<64;++visited){
    File child=folder.openNextFile();
    if(!child){exhausted=true;break;}
    // FS::File.name() can be full path or basename; accept only a complete
    // eight-character SXXXXXXX session name after extracting the basename.
    const char* raw=child.name();
    const char* last=raw?strrchr(raw,'/'):nullptr;
    const char* name=last?last+1:raw;
    char candidate[64]{};
    const bool isDir=child.isDirectory();
    const bool isSession=isDir && DTNF24M::validSession(name);
    if(isDir && !isSession)healthy=false; // no silently omitted session dirs
    if(isSession){
      const int length=snprintf(candidate,sizeof(candidate),"%s/%s",root,name);
      if(length<=0 || size_t(length)>=sizeof(candidate))healthy=false;
    }
    child.close();
    if(!healthy)break;
    if(!isSession)continue; // other F06 research files are not SOS sessions
    if(strcmp(candidate,DIRECTORY)==0)continue;
    if(!f24nClosedReplay.addClosed(candidate)){healthy=false;break;}
  }
  folder.close();
  // Never silently omit a 65th directory entry or a prior journal.
  return exhausted && healthy && f24nClosedReplay.lock();
#endif // F24P owner-bound session directory traversal
}
// Defined after EventQueue and urgentQueue below; keep urgent SOS priority.
inline bool f24nUrgentQueuePending();
inline void pollF24NOne(){
  if(!ready || f24nDiscoveryFailed || f24nClosedReplay.completed() ||
     f24mSosCapture.pending() || f24nUrgentQueuePending())return;
  if(!f24nDiscoveryTried){
    if(!f24nDiscoverClosedSessions()){
      f24nDiscoveryFailed=true;
      Serial.println("[F24N] CLOSED_SOS_RECOVERY_DENIED; OWNER_OR_SCAN_UNKNOWN");
      return;
    }
  }
  // ONE 88-byte SOS frame or 40-byte receipt per poll. All reader handles
  // close before returning. Without a trusted shore verifier, EVERY stored
  // receipt remains UNVERIFIED and cannot promote an SOS to delivered.
#if THESIS_DTN_F24O_BUILD_ONLY == 1
  f24oBindOnce(vesselId,DIRECTORY);
  auto& replayPort=f24oSosPort;
#else
  auto& replayPort=interposedSD;
#endif
  const auto result=f24nClosedReplay.poll(replayPort,FILE_READ,
                                         DTNF24N::DenyReceiptVerifier{},
                                         DTNF24N::DenyReadAuthority{});
  if(result==DTNF24N::Poll::Failed){
    f24nDiscoveryFailed=true;
    Serial.println("[F24N] SOURCE_CORRUPT_OR_UNKNOWN; REPLAY_DENIED");
  }else if(result==DTNF24N::Poll::Completed){
    Serial.printf("[F24N] HOST-REVIEW-ONLY recovered=%u pending=%u delivered=%u unverified_receipts=%lu physical_permission=NO\n",
      unsigned(f24nClosedReplay.ledger().count()),
      unsigned(f24nClosedReplay.ledger().pending()),
      unsigned(f24nClosedReplay.ledger().delivered()),
      (unsigned long)f24nClosedReplay.unverified());
  }
}
#endif

inline bool isAuthorizedVessel(uint32_t id) {
#if THESIS_F22R3D_FLEET_SD_DRYRUN == 1
    // No hardware/media approval input in this phase. The following decision
    // ALWAYS denies real SD mounting, for every valid and invalid vessel ID.
    const auto d = F22FleetStorageRuntimeGate::unreviewed(
        id, VesselIdentity::ready() && VesselIdentity::nodeId() == id);
    return F22FleetStorageRuntimeGate::mountAllowed(d,id);
#elif THESIS_F22_FLEET_OPERATIONAL == 1
    return VesselIdentity::ready() && VesselIdentity::nodeId()==id &&
           F22UniversalCapability::validIdentity(true,id);
#elif THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT == 1
    // Read ONLY the RAM-cached, already-loaded NVS identity. All unverified
    // board/media/SD-consent fields deliberately remain false, so the policy
    // DENIES mounting for every vessel, including valid IDs 1..9999.
    F22FleetStoragePolicy::Evidence evidence{};
    evidence.vesselId = id;
    evidence.provisionedIdentity = VesselIdentity::ready() && VesselIdentity::nodeId() == id;
    const auto decision = F22FleetStoragePolicy::evaluate(evidence);
    return decision.mountEligible; // ALWAYS false in this build-only phase.
#elif THESIS_SD_FLEET == 1
    return id >= 1 && id <= 3;
#else
    return id == 2;
#endif
}

// All SD operations run only in main loop, never in radio RX/TX handlers.
// Two bounded FIFOs allow SOS events to take precedence over telemetry.
static constexpr uint8_t EVENT_CAPACITY = 24;
static constexpr size_t EVENT_LINE_CAPACITY = THESIS_F07_WEATHER_LOG == 1 ? 512U : 320U;
struct EventRecord { char line[EVENT_LINE_CAPACITY]; };
struct EventQueue {
    EventRecord entries[EVENT_CAPACITY];
    uint8_t read = 0, write = 0, count = 0;
};
static EventQueue urgentQueue;
static EventQueue telemetryQueue;
#if THESIS_DTN_F24N_BUILD_ONLY == 1
// F24N poll is declared above the queues; inspect only after full declaration.
inline bool f24nUrgentQueuePending(){ return urgentQueue.count != 0; }
#endif
static uint32_t bootToken = 0;
static uint32_t eventCounter = 0;
static uint32_t lostUrgent = 0, lostTelemetry = 0;
static uint32_t lastDropNotice = 0;
#if THESIS_F06_SD_HYBRID == 1
static SDHybrid::State hybridState;
// Mode is intentionally volatile; every power-on starts SOS ARMED.
inline bool hybridIsContinuous() { return hybridState.continuous; }
inline bool hybridSOSOverride() { return hybridState.sosOverride(millis()); }
inline uint32_t hybridSkippedTelemetry() { return hybridState.skippedRoutine; }
inline void hybridObserveSOSPresence(bool active) {
    const bool wasActive = hybridState.activeSOS;
    hybridState.observeSOS(active, millis());
    if (active != wasActive)
        Serial.printf("[SD-HYBRID] SOS_STATE=%s tail_sec=600\n",
                      active ? "ACTIVE" : "TAIL");
}
inline void hybridSetContinuous(bool enabled) {
    if (hybridState.continuous == enabled) return;
    hybridState.continuous = enabled;
    Serial.printf("[SD-HYBRID] MANUAL_MODE=%s\n",
                  enabled ? "CONTINUOUS" : "SOS_ARMED");
}
#endif
#if THESIS_SD_OLED_UI == 1
// Cached sizes from EXISTING 30-second read-only SD audit. OLED draw never
// accesses SPI; the logger remains the only owner of SD I/O.
static uint64_t oledTelemetryBytes = 0;
static uint64_t oledSOSBytes = 0;
static bool oledAuditValid = false;
#endif

inline bool queueEvent(bool urgent, const char* kind, uint32_t origin,
                       uint32_t seq, int32_t result, int16_t rssi = 0,
                       int16_t snrX10 = 0, int32_t latE7 = 0,
                       int32_t lonE7 = 0, uint8_t flags = 0,
                       uint8_t hops = 0, uint8_t attempt = 0) {
    if (!ready) return false;
#if THESIS_F06_SD_HYBRID == 1
    // All urgent events always pass (including RX SOS, ACK and CANCEL).
    // Only routine telemetry is intentionally suppressed in standby.
    if (!hybridState.shouldRecord(urgent, millis())) return true;
#endif
    EventQueue &q = urgent ? urgentQueue : telemetryQueue;
    if (q.count >= EVENT_CAPACITY) {
        if (urgent) ++lostUrgent; else ++lostTelemetry;
        return false;
    }
    char &first = q.entries[q.write].line[0];
    const int len = snprintf(&first, sizeof(q.entries[q.write].line),
        "{\"kind\":\"%s\",\"vessel_id\":%lu,\"boot_token\":%lu,"
        "\"event_index\":%lu,\"uptime_ms\":%lu,\"origin_id\":%lu,"
        "\"seq\":%lu,\"result\":%ld,\"rssi\":%d,\"snr_x10\":%d,"
        "\"lat_e7\":%ld,\"lon_e7\":%ld,\"flags\":%u,"
        "\"hops\":%u,\"attempt\":%u}",
        kind, (unsigned long)vesselId, (unsigned long)bootToken,
        (unsigned long)(++eventCounter), (unsigned long)millis(),
        (unsigned long)origin, (unsigned long)seq, (long)result,
        (int)rssi, (int)snrX10, (long)latE7, (long)lonE7,
        (unsigned)flags, (unsigned)hops, (unsigned)attempt);
    if (len < 0 || (size_t)len >= sizeof(q.entries[q.write].line)) {
        if (urgent) ++lostUrgent; else ++lostTelemetry;
        return false;
    }
    q.write = (q.write + 1) % EVENT_CAPACITY;
    ++q.count;
    return true;
}

#if THESIS_V003_F19_DUAL_DISPLAY == 1 && THESIS_V003_F15_SD_PILOT_ARMED == 1
// F19R3 research snapshots: RAM queue ONLY, SD writes remain in pollEvents().
// Only explicit CONTINUOUS mode can produce these new research records.
// Urgent SOS/ACK/CANCEL journaling is independent and retains priority.
struct ResearchSystemSnapshot {
    uint32_t uptimeMs=0, txOk=0, txFail=0, rxTelemetry=0, invalidPackets=0;
    uint32_t peerCount=0, satellites=0;
    bool gpsFix=false, utcValid=false, positionValid=false;
    bool satellitesValid=false, hdopValid=false, speedValid=false;
    double latitude=0.0, longitude=0.0, hdop=0.0, speedKmph=0.0;
    char gpsUtc[21]={};  // ISO8601 Z from validated numeric GPS fields only.
};
inline void researchNumber(char* dst,size_t capacity,double value,bool valid,uint8_t digits=2) {
    if (!valid || !isfinite(value)) { snprintf(dst,capacity,"null"); return; }
    snprintf(dst,capacity,digits==6?"%.6f":"%.2f",value);
}
inline bool queueResearchSystem(const ResearchSystemSnapshot& s) {
#if THESIS_F22_FLEET_OPERATIONAL == 1
    if (!ready || !isAuthorizedVessel(vesselId) || !hybridState.continuous) return false;
#elif THESIS_F22R3D_FLEET_SD_DRYRUN == 1
    const auto d = F22FleetStorageRuntimeGate::unreviewed(vesselId, VesselIdentity::ready() && VesselIdentity::nodeId() == vesselId);
    if (!ready || !F22FleetStorageRuntimeGate::researchAllowed(
            d,vesselId,hybridState.continuous)) return false;
#else
    if (!ready || vesselId!=3U || !hybridState.continuous) return false;
#endif
    EventQueue& q=telemetryQueue;
    if (q.count>=EVENT_CAPACITY) { ++lostTelemetry;return false; }
    char lat[24],lon[24],hdop[20],speed[20],sats[16],utc[25]="null";
    researchNumber(lat,sizeof(lat),s.latitude,s.positionValid,6);
    researchNumber(lon,sizeof(lon),s.longitude,s.positionValid,6);
    researchNumber(hdop,sizeof(hdop),s.hdop,s.hdopValid);
    researchNumber(speed,sizeof(speed),s.speedKmph,s.speedValid);
    if (s.satellitesValid) snprintf(sats,sizeof(sats),"%lu",(unsigned long)s.satellites);
    else strcpy(sats,"null");
    if (s.utcValid && strlen(s.gpsUtc)==20)
        snprintf(utc,sizeof(utc),"\"%s\"",s.gpsUtc);
    char* dst=q.entries[q.write].line;
    const int n=snprintf(dst,sizeof(q.entries[q.write].line),
        "{\"kind\":\"research_system\",\"vessel_id\":%lu,\"boot_token\":%lu,"
        "\"event_index\":%lu,\"uptime_ms\":%lu,\"gps_fix\":%u,\"gps_utc\":%s,"
        "\"lat\":%s,\"lon\":%s,\"satellites\":%s,\"hdop\":%s,\"speed_kmph\":%s,"
        "\"tx_ok\":%lu,\"tx_fail\":%lu,\"rx_telemetry\":%lu,\"invalid_packets\":%lu,"
        "\"known_peers\":%lu,\"sd_drop_tlm\":%lu,\"sd_drop_sos\":%lu}",
        (unsigned long)vesselId,(unsigned long)bootToken,
        (unsigned long)(eventCounter+1UL),(unsigned long)s.uptimeMs,
        unsigned(s.gpsFix),utc,lat,lon,sats,hdop,speed,
        (unsigned long)s.txOk,(unsigned long)s.txFail,(unsigned long)s.rxTelemetry,
        (unsigned long)s.invalidPackets,(unsigned long)s.peerCount,
        (unsigned long)lostTelemetry,(unsigned long)lostUrgent);
    if (n<0 || (size_t)n>=sizeof(q.entries[q.write].line)) {
        ++lostTelemetry;return false;
    }
    ++eventCounter;q.write=(q.write+1U)%EVENT_CAPACITY;++q.count;
    return true;
}
inline bool queueResearchPeer(uint32_t id,uint32_t session,uint32_t received,
        uint32_t unique,uint32_t duplicates,uint32_t missing,double pdrEstimate,
        int16_t rssi,double snr,uint32_t ageSeconds,bool active) {
#if THESIS_F22_FLEET_OPERATIONAL == 1
    if (!ready || !isAuthorizedVessel(vesselId) || !hybridState.continuous) return false;
#elif THESIS_F22R3D_FLEET_SD_DRYRUN == 1
    const auto d = F22FleetStorageRuntimeGate::unreviewed(vesselId, VesselIdentity::ready() && VesselIdentity::nodeId() == vesselId);
    if (!ready || !F22FleetStorageRuntimeGate::researchAllowed(
            d,vesselId,hybridState.continuous)) return false;
#else
    if (!ready || vesselId!=3U || !hybridState.continuous) return false;
#endif
    EventQueue& q=telemetryQueue;
    if (q.count>=EVENT_CAPACITY) { ++lostTelemetry;return false; }
    char pdr[20],snrText[20];
    researchNumber(pdr,sizeof(pdr),pdrEstimate,true);
    researchNumber(snrText,sizeof(snrText),snr,true);
    char* dst=q.entries[q.write].line;
    const int n=snprintf(dst,sizeof(q.entries[q.write].line),
        "{\"kind\":\"research_peer\",\"vessel_id\":%lu,\"boot_token\":%lu,"
        "\"event_index\":%lu,\"uptime_ms\":%lu,\"peer_id\":%lu,\"session\":%lu,"
        "\"received\":%lu,\"unique\":%lu,\"duplicates\":%lu,\"missing_est\":%lu,"
        "\"pdr_est_pct\":%s,\"rssi_dbm\":%d,\"snr_db\":%s,\"age_s\":%lu,\"active\":%u}",
        (unsigned long)vesselId,(unsigned long)bootToken,
        (unsigned long)(eventCounter+1UL),(unsigned long)millis(),
        (unsigned long)id,(unsigned long)session,(unsigned long)received,
        (unsigned long)unique,(unsigned long)duplicates,(unsigned long)missing,
        pdr,(int)rssi,snrText,(unsigned long)ageSeconds,unsigned(active));
    if (n<0 || (size_t)n>=sizeof(q.entries[q.write].line)) {
        ++lostTelemetry;return false;
    }
    ++eventCounter;q.write=(q.write+1U)%EVENT_CAPACITY;++q.count;
    return true;
}
#endif // F19R3 V003 CONTINUOUS research snapshots

#if THESIS_F07_WEATHER_LOG == 1
inline void weatherJsonNumber(char* dst, size_t size, float value) {
    if (isfinite(value)) snprintf(dst, size, "%.2f", (double)value);
    else snprintf(dst, size, "null");
}

// One complete real sensor cycle goes to the *existing* telemetry journal.
// This intentionally bypasses SOS_ARMED's routine-radio logging filter:
// research weather samples must be available regardless of SOS activity.
// Queuing is RAM-only; the main-loop SD poll remains the only SPI writer.
inline bool queueWeatherSample(const WeatherMeasurement& s) {
    if (!ready) return false;
#if THESIS_V003_F19_DUAL_DISPLAY == 1
    // F19R2: default SOS_ARMED suppresses routine weather journal writes.
    // CONTINUOUS or authoritative SOS+10-minute tail may enable them.
    // Return false to report NOT QUEUED; this is a policy skip, not SD failure.
    if (!hybridState.routineEnabled(millis())) return false;
#endif
    EventQueue& q = telemetryQueue;
    if (q.count >= EVENT_CAPACITY) { ++lostTelemetry; return false; }

    char dhtT[20], dhtH[20], bmeT[20], bmeH[20], bmeRawP[20];
    char staP[20], selT[20], selH[20], mslP[20], dP[20], dT[20], dH[20];
    weatherJsonNumber(dhtT, sizeof(dhtT), s.dhtTempC);
    weatherJsonNumber(dhtH, sizeof(dhtH), s.dhtRH);
    weatherJsonNumber(bmeT, sizeof(bmeT), s.bmeTempC);
    weatherJsonNumber(bmeH, sizeof(bmeH), s.bmeRH);
    weatherJsonNumber(bmeRawP, sizeof(bmeRawP), s.bmeRawPressureHpa);
    weatherJsonNumber(staP, sizeof(staP), s.stationPressureHpa);
    weatherJsonNumber(selT, sizeof(selT), s.selectedTempC);
    weatherJsonNumber(selH, sizeof(selH), s.selectedRH);
    weatherJsonNumber(mslP, sizeof(mslP), s.pressureMSLHpa);
    weatherJsonNumber(dP, sizeof(dP), s.pressureDeltaHpa);
    weatherJsonNumber(dT, sizeof(dT), s.tempDeltaC);
    weatherJsonNumber(dH, sizeof(dH), s.humidityDeltaPoints);
    char gpsUtcJson[24] = "null";
    if (s.gpsUtcValid && strlen(s.gpsUtcIso8601) == 20) {
        // The string is formatted solely from GNSS numeric date/time fields.
        snprintf(gpsUtcJson, sizeof(gpsUtcJson), "\"%s\"", s.gpsUtcIso8601);
    }

    char* const dest = q.entries[q.write].line;
    const int size = snprintf(dest, sizeof(q.entries[q.write].line),
        "{\"kind\":\"weather\",\"vessel_id\":%lu,\"boot_token\":%lu,\"event_index\":%lu,"
        "\"uptime_ms\":%lu,\"gps_utc\":%s,\"dht_t_c\":%s,\"dht_rh_pct\":%s,"
        "\"bme_t_c\":%s,\"bme_rh_pct\":%s,\"bme_raw_hpa\":%s,"
        "\"station_hpa\":%s,\"t_c\":%s,\"rh_pct\":%s,\"msl_hpa\":%s,"
        "\"dp_hpa\":%s,\"dt_c\":%s,\"drh_points\":%s,"
        "\"history_count\":%u,\"weather_state\":%u,\"zambretti\":%u,"
        "\"source\":%u,\"dht_ok\":%u,\"bme_th_ok\":%u,\"bme_p_ok\":%u,\"inputs_ok\":%u}",
        (unsigned long)vesselId, (unsigned long)bootToken,
        (unsigned long)(eventCounter + 1UL), (unsigned long)s.uptimeMs, gpsUtcJson,
        dhtT, dhtH, bmeT, bmeH, bmeRawP, staP, selT, selH, mslP,
        dP, dT, dH,
        (unsigned)s.historyCount, (unsigned)s.weatherState, (unsigned)s.zambretti,
        (unsigned)s.tempSource, (unsigned)s.dhtOK, (unsigned)s.bmeTHOK,
        (unsigned)s.bmePressureOK, (unsigned)s.inputsOK);
    if (size < 0 || (size_t)size >= sizeof(q.entries[q.write].line)) {
        ++lostTelemetry;
        return false;
    }
    ++eventCounter;
    q.write = (q.write + 1) % EVENT_CAPACITY;
    ++q.count;
    return true;
}
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
// One experimental near-term outlook per F10 attempt. Separate JSON "kind"
// from F11 macro weather, same isolated F06 session and bounded queue.
// This is not a calibrated forecast and never alters an SOS or LoRa packet.
inline bool queueMicroOutlook(uint32_t uptimeMs, bool inputValid, uint8_t historyCount,
                              const char* state, const char* pattern,
                              float dt, float drh, float dp, float dewGap,
                              bool reusedMacro) {
#if THESIS_F22_FLEET_OPERATIONAL == 1
    if (!ready || !isAuthorizedVessel(vesselId)) return false;
#elif THESIS_F22R3D_FLEET_SD_DRYRUN == 1
    const auto d = F22FleetStorageRuntimeGate::unreviewed(vesselId, VesselIdentity::ready() && VesselIdentity::nodeId() == vesselId);
    if (!ready || !F22FleetStorageRuntimeGate::routineAllowed(d,vesselId)) return false;
#else
    if (!ready || vesselId != 3U) return false;
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1
    // Match macro weather: experimental micro records also require consent
    // or authoritative distress/tail; forecast processing itself continues.
    if (!hybridState.routineEnabled(millis())) return false;
#endif
    EventQueue& q = telemetryQueue;
    if (q.count >= EVENT_CAPACITY) { ++lostTelemetry; return false; }
    char dT[20], dRH[20], dP[20], gap[20];
    weatherJsonNumber(dT, sizeof(dT), dt);
    weatherJsonNumber(dRH, sizeof(dRH), drh);
    weatherJsonNumber(dP, sizeof(dP), dp);
    weatherJsonNumber(gap, sizeof(gap), dewGap);
    char* dest = q.entries[q.write].line;
    const int size = snprintf(dest, sizeof(q.entries[q.write].line),
        "{\"kind\":\"micro_outlook\",\"vessel_id\":%lu,\"boot_token\":%lu,"
        "\"event_index\":%lu,\"uptime_ms\":%lu,\"input_valid\":%u,"
        "\"history_count\":%u,\"state\":\"%s\",\"pattern\":\"%s\","
        "\"dt10_c\":%s,\"drh10_pp\":%s,\"dp10_hpa\":%s,"
        "\"dew_gap_c\":%s,\"source\":\"%s\","
        "\"horizon\":\"EXPERIMENTAL_10_30_MIN\"}",
        (unsigned long)vesselId, (unsigned long)bootToken,
        (unsigned long)(eventCounter + 1UL), (unsigned long)uptimeMs,
        unsigned(inputValid), unsigned(historyCount), state, pattern,
        dT, dRH, dP, gap, reusedMacro ? "MACRO_REUSED" : "DIRECT");
    if (size < 0 || (size_t)size >= sizeof(q.entries[q.write].line)) {
        ++lostTelemetry;
        return false;
    }
    ++eventCounter;
    q.write = (q.write + 1U) % EVENT_CAPACITY;
    ++q.count;
    return true;
}
#endif // F18
#endif // THESIS_F07_WEATHER_LOG

inline void pollEvents() {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return;
#endif
// F23D SD end
    if (!ready) return;
    EventQueue &q = urgentQueue.count ? urgentQueue : telemetryQueue;
    if (q.count) {
        const char* path = urgentQueue.count ? SOS_EVENTS : TELEMETRY;
        // One append+flush per loop iteration: no SPI writes in radio callbacks.
        if (!appendRecord(path, q.entries[q.read].line)) {
            Serial.println("[SD-EVENT] WRITE_FAILED; LOGGER_DISABLED");
            ready = false;
            return;
        }
        q.read = (q.read + 1) % EVENT_CAPACITY;
        --q.count;
    }
    const uint32_t now = millis();
    if ((lostUrgent || lostTelemetry) &&
        uint32_t(now - lastDropNotice) >= 5000UL) {
        lastDropNotice = now;
        Serial.printf("[SD-EVENT] DROPPED urgent=%lu telemetry=%lu\n",
          (unsigned long)lostUrgent, (unsigned long)lostTelemetry);
    }
}


inline bool appendRecord(const char* path, const char* record) {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return false;
#endif
// F23D SD end
    if (!ready) return false;

    File file = interposedSD.open(path, FILE_APPEND);

    if (!file) {
        Serial.println("[SD-LOG] OPEN_FAIL");
        return false;
    }

    const size_t expected = strlen(record) + 1;
    const size_t written = file.print(record) + file.print('\n');

    file.flush();
    file.close();

    if (written != expected) {
        Serial.println("[SD-LOG] WRITE_FAIL");
        return false;
    }

    return true;
}

inline bool begin(XPowersAXP2101& pmu, uint32_t id) {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return false;
#endif
// F23D SD end
    Serial.println("[SD-LOG] START");

    vesselId = id;

    // Never initialize storage using an unprovisioned/unknown ID.
    if (!isAuthorizedVessel(vesselId)) {
        Serial.println("[SD-LOG] WRONG_VESSEL_ID");
        return false;
    }

    // Separate journals prevent a moved SD card from mixing vessels.
    snprintf(DIRECTORY, sizeof(DIRECTORY), "/THESIS/VESSEL_%03lu",
             (unsigned long)vesselId);
    snprintf(DIAGNOSTICS, sizeof(DIAGNOSTICS), "%s/diagnostics.jsonl", DIRECTORY);
    snprintf(TELEMETRY, sizeof(TELEMETRY), "%s/telemetry.jsonl", DIRECTORY);
    snprintf(SOS_EVENTS, sizeof(SOS_EVENTS), "%s/sos_events.jsonl", DIRECTORY);

    if (!pmu.setBLDO1Voltage(3300)) {
        Serial.println("[SD-LOG] REGULATOR_CONFIG_FAIL");
        return false;
    }

    pmu.enableBLDO1();
    delay(200);

    if (!pmu.isEnableBLDO1()) {
        Serial.println("[SD-LOG] POWER_FAIL");
        return false;
    }

    pinMode(34, OUTPUT);
    digitalWrite(34, HIGH);

    pinMode(47, OUTPUT);
    digitalWrite(47, HIGH);

    sdSPI.begin(36, 37, 35, 47);

    // Last argument false: do not format on mount failure.
    if (!interposedSD.begin(47, sdSPI, 1000000, "/sd", 5, false)) {
        Serial.println("[SD-LOG] MOUNT_FAIL");
        return false;
    }

    Serial.printf(
        "[SD-LOG] MOUNT_OK capacity_mib=%llu\n",
        (unsigned long long)(interposedSD.cardSize() / 1048576ULL)
    );

    if (!interposedSD.exists("/THESIS") && !interposedSD.mkdir("/THESIS")) {
        Serial.println("[SD-LOG] ROOT_CREATE_FAIL");
        return false;
    }

#if THESIS_F06_SD_SANDBOX == 1
    // Do not touch /THESIS/VESSEL_XXX journals. Every boot creates its own
    // empty session directory. Collisions are skipped, never reopened.
    const char* f06Root = "/THESIS/F06TEST";
    if (!interposedSD.exists(f06Root) && !interposedSD.mkdir(f06Root)) {
        Serial.println("[SD-F06] TEST_ROOT_CREATE_FAIL; NO JOURNAL WRITES");
        return false;
    }
    char f06VesselRoot[40];
    snprintf(f06VesselRoot, sizeof(f06VesselRoot), "%s/V%03lu",
             f06Root, (unsigned long)vesselId);
    if (!interposedSD.exists(f06VesselRoot) && !interposedSD.mkdir(f06VesselRoot)) {
        Serial.println("[SD-F06] VESSEL_TEST_ROOT_CREATE_FAIL; NO JOURNAL WRITES");
        return false;
    }
    bool isolatedDirCreated = false;
    for (uint8_t tryNumber = 0; tryNumber < 16; ++tryNumber) {
        char candidate[sizeof(DIRECTORY)];
        const uint32_t token = esp_random() & 0x0FFFFFFFUL;
        const int length = snprintf(candidate, sizeof(candidate), "%s/S%07lX",
                                    f06VesselRoot, (unsigned long)token);
        if (length <= 0 || (size_t)length >= sizeof(candidate)) {
            Serial.println("[SD-F06] SESSION_PATH_TOO_LONG; NO JOURNAL WRITES");
            return false;
        }
        if (interposedSD.exists(candidate)) continue;
        if (!interposedSD.mkdir(candidate)) {
            Serial.println("[SD-F06] SESSION_DIR_CREATE_FAIL; NO JOURNAL WRITES");
            return false;
        }
        strcpy(DIRECTORY, candidate);
        isolatedDirCreated = true;
        break;
    }
    if (!isolatedDirCreated) {
        Serial.println("[SD-F06] NO_UNIQUE_SESSION; NO JOURNAL WRITES");
        return false;
    }
    snprintf(DIAGNOSTICS, sizeof(DIAGNOSTICS), "%s/diagnostics.jsonl", DIRECTORY);
    snprintf(TELEMETRY, sizeof(TELEMETRY), "%s/telemetry.jsonl", DIRECTORY);
    snprintf(SOS_EVENTS, sizeof(SOS_EVENTS), "%s/sos_events.jsonl", DIRECTORY);
    Serial.printf("[SD-F06] SANDBOX_READY path=%s legacy_journals=UNTOUCHED\n", DIRECTORY);
#endif

    if (!interposedSD.exists(DIRECTORY) && !interposedSD.mkdir(DIRECTORY)) {
        Serial.println("[SD-LOG] VESSEL_DIR_CREATE_FAIL");
        return false;
    }

    File dir = interposedSD.open(DIRECTORY, FILE_READ);

    if (!dir || !dir.isDirectory()) {
        Serial.println("[SD-LOG] VESSEL_DIR_INVALID");
        if (dir) dir.close();
        return false;
    }

    dir.close();

    // Inspect the existing thesis journal before adding a new record.
    // Read at most 64 KiB to keep startup work bounded.
    uint64_t bytesBefore = 0;
    uint32_t priorLines = 0;
    bool lastLineComplete = false;

    if (interposedSD.exists(DIAGNOSTICS)) {
        File previous = interposedSD.open(DIAGNOSTICS, FILE_READ);

        if (!previous) {
            Serial.println("[SD-VERIFY] PRIOR_OPEN_FAIL");
            return false;
        }

        bytesBefore = previous.size();

        if (bytesBefore <= 65536ULL) {
            char current[192] = {};
            char last[192] = {};
            size_t used = 0;

            while (previous.available()) {
                int ch = previous.read();

                if (ch < 0) break;

                if (ch == '\n') {
                    current[used] = '\0';
                    strncpy(last, current, sizeof(last) - 1);
                    last[sizeof(last) - 1] = '\0';
                    used = 0;
                    priorLines++;
                    lastLineComplete = true;
                } else if (ch != '\r') {
                    if (used < sizeof(current) - 1) {
                        current[used++] = (char)ch;
                    }
                    lastLineComplete = false;
                }
            }

            Serial.printf(
                "[SD-VERIFY] PRIOR_LINES=%lu complete=%u\n",
                (unsigned long)priorLines,
                (unsigned)lastLineComplete
            );

            if (priorLines > 0) {
                Serial.printf("[SD-VERIFY] LAST_RECORD=%s\n", last);
            }
        } else {
            Serial.println("[SD-VERIFY] SCAN_SKIPPED_OVER_64K");
        }

        previous.close();
    }

    Serial.printf(
        "[SD-VERIFY] BEFORE_BYTES=%llu\n",
        (unsigned long long)bytesBefore
    );

    bootToken = esp_random();
    eventCounter = 0;
    urgentQueue = EventQueue{};
    telemetryQueue = EventQueue{};
    lostUrgent = lostTelemetry = 0;
    lastDropNotice = millis();
#if THESIS_F06_SD_HYBRID == 1
    hybridState = SDHybrid::State{};
#endif
#if THESIS_SD_OLED_UI == 1
    oledTelemetryBytes = oledSOSBytes = 0;
    oledAuditValid = false;
#endif
    ready = true;

    char record[160];

    snprintf(
        record,
        sizeof(record),
        "{\"kind\":\"boot\",\"vessel_id\":%lu,\"uptime_ms\":%lu}",
        (unsigned long)vesselId,
        (unsigned long)millis()
    );

    if (!appendRecord(DIAGNOSTICS, record)) {
        ready = false;
        Serial.println("[SD-LOG] BOOT_WRITE_FAIL");
        return false;
    }

    File verification = interposedSD.open(DIAGNOSTICS, FILE_READ);

    if (!verification) {
        ready = false;
        Serial.println("[SD-VERIFY] READBACK_OPEN_FAIL");
        return false;
    }

    uint64_t bytesAfter = verification.size();
    verification.close();

    if (bytesAfter <= bytesBefore) {
        ready = false;
        Serial.println("[SD-VERIFY] FILE_NOT_GROWING");
        return false;
    }

    Serial.printf(
        "[SD-VERIFY] AFTER_BYTES=%llu DELTA=%llu\n",
        (unsigned long long)bytesAfter,
        (unsigned long long)(bytesAfter - bytesBefore)
    );

    lastHeartbeat = millis();

    Serial.println("[SD-LOG] BOOT_WRITE_OK");
#if THESIS_F06_SD_HYBRID == 1
    Serial.println("[SD-HYBRID] INIT mode=SOS_ARMED routine=OFF emergency=ON tail_sec=600");
#endif
    Serial.printf("[SD-LOG] READY path=%s\n", DIRECTORY);

    return true;
}


// USB-only readback: exact byte chunks as hexadecimal, never modifies SD contents.
// File sizes are snapshotted when requested. One chunk is emitted per loop.
static const char* exportPaths[] = { SOS_EVENTS, TELEMETRY, DIAGNOSTICS };
static const char* exportNames[] = { "sos_events.jsonl", "telemetry.jsonl", "diagnostics.jsonl" };
static bool exportActive = false;
static uint8_t exportIndex = 0;
static uint32_t exportOffset = 0;
static uint32_t exportLimit = 0;
static uint32_t exportChecksum = 2166136261UL; // FNV-1a for transfer check
static File exportFile;

inline void requestExport() {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return;
#endif
// F23D SD end
    if (!ready || !isAuthorizedVessel(vesselId)) {
        Serial.println("[SDX] ERROR LOGGER_NOT_READY");
        return;
    }
    if (exportActive) {
        Serial.println("[SDX] ERROR BUSY");
        return;
    }
    if (urgentQueue.count || telemetryQueue.count) {
        Serial.println("[SDX] ERROR QUEUE_NOT_EMPTY_RETRY");
        return;
    }
    exportIndex = 0;
    exportOffset = 0;
    exportLimit = 0;
    exportChecksum = 2166136261UL;
    exportActive = true;
    Serial.println("[SDX] SESSION_START files=3 encoding=HEX checksum=FNV1A32");
}

inline void pollExport() {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return;
#endif
// F23D SD end
    if (!exportActive) return;
    if (!ready) {
        if (exportFile) exportFile.close();
        exportActive = false;
        Serial.println("[SDX] ERROR LOGGER_DISABLED");
        return;
    }
    if (!exportFile) {
        if (exportIndex >= 3) {
            Serial.println("[SDX] SESSION_END");
            exportActive = false;
            return;
        }
        exportFile = interposedSD.open(exportPaths[exportIndex], FILE_READ);
        if (!exportFile) {
            // A missing file is valid only when it does not exist.
            if (interposedSD.exists(exportPaths[exportIndex])) {
                Serial.printf("[SDX] ERROR OPEN_FAILED index=%u\n", (unsigned)exportIndex);
                exportActive = false;
                return;
            }
            Serial.printf("[SDX] BEGIN %u %s 0\n", (unsigned)exportIndex, exportNames[exportIndex]);
            Serial.printf("[SDX] END %u 0 %08lX\n", (unsigned)exportIndex, 2166136261UL);
            ++exportIndex;
            return;
        }
        if (exportFile.size() > 1048576UL) {
            Serial.printf("[SDX] ERROR FILE_TOO_LARGE index=%u\n", (unsigned)exportIndex);
            exportFile.close();
            exportActive = false;
            return;
        }
        exportOffset = 0;
        exportLimit = (uint32_t)exportFile.size();
        exportChecksum = 2166136261UL;
        Serial.printf("[SDX] BEGIN %u %s %lu\n", (unsigned)exportIndex,
                      exportNames[exportIndex], (unsigned long)exportLimit);
        return;
    }
    if (exportOffset >= exportLimit) {
        Serial.printf("[SDX] END %u %lu %08lX\n", (unsigned)exportIndex,
                      (unsigned long)exportOffset, (unsigned long)exportChecksum);
        exportFile.close();
        ++exportIndex;
        return;
    }
    static const char hex[] = "0123456789ABCDEF";
    char encoded[129];
    const size_t wanted = (exportLimit - exportOffset > 64) ? 64 : (size_t)(exportLimit - exportOffset);
    size_t actual = 0;
    for (; actual < wanted; ++actual) {
        const int v = exportFile.read();
        if (v < 0) break;
        const uint8_t byte = (uint8_t)v;
        encoded[actual*2] = hex[byte >> 4];
        encoded[actual*2+1] = hex[byte & 15];
        exportChecksum = (exportChecksum ^ byte) * 16777619UL;
    }
    if (actual != wanted) {
        Serial.printf("[SDX] ERROR EARLY_EOF index=%u\n", (unsigned)exportIndex);
        exportFile.close();
        exportActive = false;
        return;
    }
    encoded[actual*2] = '\0';
    Serial.printf("[SDX] DATA %u %lu %s\n", (unsigned)exportIndex,
                  (unsigned long)exportOffset, encoded);
    exportOffset += (uint32_t)actual;
}

inline void poll() {
#if THESIS_DTN_F24Q_BUILD_ONLY == 1
    // Compile the real main-loop seam against an UNGRANTABLE witness.
    // No SD/FS/SPI/relay action, no ledger credit, and no simulated owner
    // state is installed into any physical File lifetime.
    static const DTNF24Q::DenyWitness noPhysicalOwner{};
    static DTNF24Q::Arbiter<DTNF24Q::DenyWitness> noGrant(noPhysicalOwner);
    (void)noGrant.acquire(vesselId,DTNF24Q::Lane::Sos);
#endif
#if THESIS_DTN_F24E_BUILD_ONLY == 1
    // This does NOT queue a session or touch the SD bus; the grant is always denied.
    // Compiles the actual main-loop owner hook without enabling replay or RF.
    static DTNF24E::Scheduler<DTNF24DArduino::ReadFile,DTNF24DArduino::ReadVolume> preview;
    const F24DNoPhysicalPermission deny{};
    (void)preview.poll(deny,false,true);
#endif
#if THESIS_DTN_F24F_BUILD_ONLY == 1
    // Typecheck only: no proof, no open, no SD access and no new transmissions.
    static DTNF24F::SosClosedLease alwaysDeny;
    const DTNAckLedger::Stream noStream{};
    (void)alwaysDeny.allowClosedJournal(noStream,"");
#endif
#if THESIS_DTN_F24G_BUILD_ONLY == 1
    // Compiler-only candidate check. ALWAYS denied; no seal writing or SD I/O.
    const DTNF24G::CloseFacts noClose{};
    const DTNF24F::OwnerNow noOwner{};
    DTNF24G::Seal unapproved{};
    (void)DTNF24G::prepare(DTNAckLedger::Stream{},noClose,noOwner,
                            DTNF24D::Result::Denied,0U,unapproved);
#endif
#if THESIS_DTN_F24H_BUILD_ONLY == 1
    // Observe actual RAM queue counts ONLY; no owner evidence, no Port, no
    // retire/flush/close/SD access, and NO grant. Existing logging unchanged.
    static DTNF24H::Controller denyPreview;
    DTNF24H::Snapshot noAuthority{};
    noAuthority.urgentQueued = urgentQueue.count;
    noAuthority.researchQueued = telemetryQueue.count;
    (void)denyPreview.stillCandidate(noAuthority);
#endif
#if THESIS_DTN_F24I_BUILD_ONLY == 1
    // RAM-only bridge typecheck. Permanently denied: no physical owner, no
    // actual SD Port, no session identity and absolutely no SD I/O.
    static DTNF24I::NoPhysicalPort noPhysicalPort;
    static const DTNF24I::NoPhysicalObserver noPhysicalWitness{};
    static DTNF24I::GuardedPort<DTNF24I::NoPhysicalPort,
                                DTNF24I::NoPhysicalObserver> denyBridge(
                                    noPhysicalPort,noPhysicalWitness);
    noPhysicalPort.observedUrgent = urgentQueue.count;
    noPhysicalPort.observedResearch = telemetryQueue.count;
    (void)denyBridge.arm(DTNF24H::SessionKey{});
#if THESIS_DTN_F24J_BUILD_ONLY == 1
    // F24J never handles SD; no unreviewed runtime authority exists.
    static DTNF24J::Coordinator<decltype(denyBridge),
                                DTNF24I::NoPhysicalObserver> denyCoordinator(
                                    denyBridge,noPhysicalWitness);
    (void)denyCoordinator.bind(DTNF24H::SessionKey{});
#endif
#endif
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return;
#endif
// F23D SD end
    if (!ready) return;
#if THESIS_DTN_F24M_BUILD_ONLY == 1
    // Only main-loop owner path may access SD; one SOS frame per iteration.
    // This compile-only profile inherits the F24K permanently denied facade.
    pollF24MOne();
#endif
    pollEvents();
    if (!ready) return;
#if THESIS_DTN_F24N_BUILD_ONLY == 1
    // SOS log write path retains priority; no replay or SD activity during
    // pending urgent capture. Physical F24N SD owner remains hard-denied.
    pollF24NOne();
#endif

    // Read-only storage audit, once every 30 seconds.
    static uint32_t lastAuditMs = 0;
    if (uint32_t(millis() - lastAuditMs) >= 30000UL) {
        lastAuditMs = millis();

        uint64_t telemetryBytes = 0;
        uint64_t sosBytes = 0;

        File telemetryFile = interposedSD.open(TELEMETRY, FILE_READ);
        if (telemetryFile) {
            telemetryBytes = telemetryFile.size();
            telemetryFile.close();
        }

        File sosFile = interposedSD.open(SOS_EVENTS, FILE_READ);
        if (sosFile) {
            sosBytes = sosFile.size();
            sosFile.close();
        }

#if THESIS_SD_OLED_UI == 1
        oledTelemetryBytes = telemetryBytes;
        oledSOSBytes = sosBytes;
        oledAuditValid = true;
#endif
        Serial.printf(
            "[SD-AUDIT] telemetry_bytes=%llu sos_bytes=%llu queued_telemetry=%u queued_sos=%u dropped_telemetry=%lu dropped_sos=%lu events=%lu\n",
            (unsigned long long)telemetryBytes,
            (unsigned long long)sosBytes,
            (unsigned)telemetryQueue.count,
            (unsigned)urgentQueue.count,
            (unsigned long)lostTelemetry,
            (unsigned long)lostUrgent,
            (unsigned long)eventCounter
        );
    }

    uint32_t now = millis();
#if THESIS_F06_SD_HYBRID == 1
    // Keep the mandatory boot journal but suppress periodic diagnostics
    // in SOS ARMED standby. The normal SD audit remains read-only.
    if (!hybridState.routineEnabled(now)) {
        lastHeartbeat = now;
        return;
    }
#endif
    if (uint32_t(now - lastHeartbeat) < HEARTBEAT_INTERVAL_MS) {
        return;
    }

    lastHeartbeat = now;

    char record[160];

    snprintf(
        record,
        sizeof(record),
        "{\"kind\":\"heartbeat\",\"vessel_id\":%lu,\"uptime_ms\":%lu}",
        (unsigned long)vesselId,
        (unsigned long)now
    );

    if (appendRecord(DIAGNOSTICS, record)) {
        Serial.println("[SD-LOG] HEARTBEAT_WRITE_OK");
    } else {
        ready = false;
        Serial.println("[SD-LOG] DISABLED_AFTER_WRITE_FAIL");
    }
}

#if THESIS_F06_SD_SANDBOX == 1
// F06 historical inspection: USB-only, allowlisted and read-only.
// F19R2 additionally allows V003 F06 sessions when compiled with dual-display.
// No caller-supplied path or filename is ever sent to interposedSD.open().
// Exactly two selectors are allowed: LEGACY or S + 7 uppercase hex digits.
// These routines NEVER use FILE_APPEND, FILE_WRITE, mkdir, remove or rename.
static constexpr uint32_t HISTORY_MAX_FILE_BYTES = 1048576UL;
static constexpr size_t HISTORY_READ_CHUNK = 64;
static const char* const historyNames[3] = {
    "sos_events.jsonl", "telemetry.jsonl", "diagnostics.jsonl"
};
static bool historyActive = false;
static uint8_t historyIndex = 0;
static uint32_t historyOffset = 0;
static uint32_t historySizes[3] = {};
static uint32_t historyChecksum = 2166136261UL;
static char historyRoot[72] = {};
static File historyFile;

inline bool historyReadActive() { return historyActive; }

inline bool validHistorySession(const char* id) {
    if (!id || strlen(id) != 8 || id[0] != 'S') return false;
    for (size_t i = 1; i < 8; ++i) {
        const char c = id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

inline bool historyPath(char* output, size_t capacity, uint8_t index) {
    if (index >= 3) return false;
    const int n = snprintf(output, capacity, "%s/%s", historyRoot, historyNames[index]);
    return n > 0 && static_cast<size_t>(n) < capacity;
}

inline void historyStop(const char* reason) {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return;
#endif
// F23D SD end
    if (historyFile) historyFile.close();
    historyActive = false;
    Serial.printf("[SDH] ERROR %s\n", reason);
}

inline void listHistory() {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return;
#endif
// F23D SD end
#if THESIS_F22_FLEET_OPERATIONAL == 1
    if (!ready || !isAuthorizedVessel(vesselId) || historyActive || exportActive) {
#elif THESIS_F22R3D_FLEET_SD_DRYRUN == 1
    const auto d = F22FleetStorageRuntimeGate::unreviewed(vesselId, VesselIdentity::ready() && VesselIdentity::nodeId() == vesselId);
    if (!ready || !F22FleetStorageRuntimeGate::ownHistoryAllowed(d,vesselId) || historyActive || exportActive) {
#elif THESIS_V003_F19_DUAL_DISPLAY == 1
    if (!ready || (vesselId != 2 && vesselId != 3) || historyActive || exportActive) {
#else
    if (!ready || vesselId != 2 || historyActive || exportActive) {
#endif
        Serial.println("[SDH] ERROR LOGGER_NOT_READY_OR_BUSY"); return;
    }
    char root[48];
    snprintf(root, sizeof(root), "/THESIS/F06TEST/V%03lu", (unsigned long)vesselId);
    File directory = interposedSD.open(root, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) directory.close();
        Serial.println("[SDH] ERROR SESSION_DIRECTORY_MISSING"); return;
    }
#if THESIS_V003_F19_DUAL_DISPLAY == 1
    Serial.printf("[SDH] LIST_BEGIN scope=V%03lu sessions_only=1\n",
                  (unsigned long)vesselId);
#else
    Serial.println("[SDH] LIST_BEGIN scope=V002 sessions_only=1");
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1
    // V003 cannot read the legacy path; do not advertise an invalid target.
    if (vesselId == 2) Serial.println("[SDH] TARGET LEGACY");
#else
    Serial.println("[SDH] TARGET LEGACY");
#endif
    uint8_t shown = 0;
    bool truncated = false;
    while (File entry = directory.openNextFile()) {
        const char* name = entry.name();
        const char* slash = strrchr(name, '/');
        if (slash) name = slash + 1;
        if (entry.isDirectory() && validHistorySession(name)) {
            if (shown < 48) {
                Serial.printf("[SDH] TARGET %s\n", name);
                ++shown;
            } else {
                truncated = true;
                entry.close();
                break;
            }
        }
        entry.close();
    }
    directory.close();
    Serial.printf("[SDH] LIST_END shown=%u truncated=%u\n",
                  (unsigned)shown, (unsigned)truncated);
}

inline void requestHistory(const char* selector) {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return;
#endif
// F23D SD end
#if THESIS_F22_FLEET_OPERATIONAL == 1
    if (!ready || !isAuthorizedVessel(vesselId)) {
#elif THESIS_F22R3D_FLEET_SD_DRYRUN == 1
    const auto d = F22FleetStorageRuntimeGate::unreviewed(vesselId, VesselIdentity::ready() && VesselIdentity::nodeId() == vesselId);
    if (!ready || !F22FleetStorageRuntimeGate::ownHistoryAllowed(d,vesselId)) {
#elif THESIS_V003_F19_DUAL_DISPLAY == 1
    if (!ready || (vesselId != 2 && vesselId != 3)) {
#else
    if (!ready || vesselId != 2) {
#endif
        Serial.println("[SDH] ERROR LOGGER_NOT_READY_OR_WRONG_VESSEL"); return;
    }
    if (historyActive || exportActive) {
        Serial.println("[SDH] ERROR EXPORT_BUSY"); return;
    }
    if (strcmp(selector, "LEGACY") == 0) {
#if THESIS_F22_FLEET_OPERATIONAL == 1
        // Old V002 historical journal is READ-ONLY; other vessels are denied.
        if (vesselId != 2U) {
            Serial.println("[SDH] ERROR LEGACY_V002_ONLY"); return;
        }
#elif THESIS_F22R3D_FLEET_SD_DRYRUN == 1
        if (!F22FleetStorageRuntimeGate::legacyV002Allowed(d,vesselId)) {
            Serial.println("[SDH] ERROR LEGACY_REVIEW_REQUIRED"); return;
        }
#elif THESIS_V003_F19_DUAL_DISPLAY == 1
        // V003 may read only its F06 isolated sessions, never legacy paths.
        if (vesselId == 3) {
            Serial.println("[SDH] ERROR LEGACY_V003_DISABLED"); return;
        }
#endif
        snprintf(historyRoot, sizeof(historyRoot), "/THESIS/VESSEL_%03lu",
                 (unsigned long)vesselId);
    } else if (validHistorySession(selector)) {
        snprintf(historyRoot, sizeof(historyRoot),
                 "/THESIS/F06TEST/V%03lu/%s", (unsigned long)vesselId, selector);
    } else {
        Serial.println("[SDH] ERROR SELECTOR_REJECTED"); return;
    }
    File root = interposedSD.open(historyRoot, FILE_READ);
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        Serial.println("[SDH] ERROR TARGET_DIRECTORY_MISSING"); return;
    }
    root.close();

    // Preflight all files before sending any data. The only SD.opens are READ.
    for (uint8_t i = 0; i < 3; ++i) {
        char path[96];
        if (!historyPath(path, sizeof(path), i)) {
            Serial.println("[SDH] ERROR PATH_TOO_LONG"); return;
        }
        File file = interposedSD.open(path, FILE_READ);
        if (!file) {
            if (interposedSD.exists(path)) {
                Serial.println("[SDH] ERROR FILE_OPEN_FAILED"); return;
            }
            historySizes[i] = 0; // Missing SOS file is valid: zero bytes.
        } else {
            if (file.isDirectory() || file.size() > HISTORY_MAX_FILE_BYTES) {
                file.close();
                Serial.println("[SDH] ERROR FILE_INVALID_OR_OVER_1MIB"); return;
            }
            historySizes[i] = static_cast<uint32_t>(file.size());
            file.close();
        }
    }
    historyIndex = 0;
    historyOffset = 0;
    historyChecksum = 2166136261UL;
    historyActive = true;
    Serial.printf("[SDH] SESSION_START target=%s files=3\n", selector);
}

inline void pollHistory() {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(f23dStorageFrozen())return;
#endif
// F23D SD end
    if (!historyActive) return;
#if THESIS_F22_FLEET_OPERATIONAL == 1
    if (!ready || !isAuthorizedVessel(vesselId)) {
#elif THESIS_F22R3D_FLEET_SD_DRYRUN == 1
    const auto d = F22FleetStorageRuntimeGate::unreviewed(vesselId, VesselIdentity::ready() && VesselIdentity::nodeId() == vesselId);
    if (!ready || !F22FleetStorageRuntimeGate::ownHistoryAllowed(d,vesselId)) {
#elif THESIS_V003_F19_DUAL_DISPLAY == 1
    if (!ready || (vesselId != 2 && vesselId != 3)) {
#else
    if (!ready || vesselId != 2) {
#endif
        historyStop("LOGGER_NOT_READY"); return;
    }
    if (historyIndex == 3) {
        Serial.println("[SDH] SESSION_END");
        historyActive = false;
        return;
    }
    const uint8_t i = historyIndex;
    if (!historyFile && historyOffset == 0) {
        Serial.printf("[SDH] BEGIN %u %s %lu\n", (unsigned)i,
                      historyNames[i], (unsigned long)historySizes[i]);
        if (historySizes[i] == 0) {
            Serial.printf("[SDH] END %u 0 %08lX\n", (unsigned)i,
                          (unsigned long)2166136261UL);
            ++historyIndex;
            return;
        }
        char path[96];
        if (!historyPath(path, sizeof(path), i)) {
            historyStop("PATH_TOO_LONG"); return;
        }
        historyFile = interposedSD.open(path, FILE_READ);
        if (!historyFile || historyFile.isDirectory() ||
            historyFile.size() != historySizes[i]) {
            historyStop("FILE_CHANGED_OR_OPEN_FAILED"); return;
        }
        historyChecksum = 2166136261UL;
    }
    if (historyOffset == historySizes[i]) {
        Serial.printf("[SDH] END %u %lu %08lX\n", (unsigned)i,
                      (unsigned long)historyOffset, (unsigned long)historyChecksum);
        historyFile.close();
        historyOffset = 0;
        ++historyIndex;
        return;
    }
    const size_t remain = historySizes[i] - historyOffset;
    const size_t wanted = remain < HISTORY_READ_CHUNK ? remain : HISTORY_READ_CHUNK;
    static const char digits[] = "0123456789ABCDEF";
    char hex[HISTORY_READ_CHUNK * 2 + 1];
    for (size_t j = 0; j < wanted; ++j) {
        const int value = historyFile.read();
        if (value < 0) { historyStop("EARLY_EOF"); return; }
        const uint8_t byte = static_cast<uint8_t>(value);
        hex[j * 2] = digits[byte >> 4];
        hex[j * 2 + 1] = digits[byte & 15];
        historyChecksum = (historyChecksum ^ byte) * 16777619UL;
    }
    hex[wanted * 2] = '\0';
    Serial.printf("[SDH] DATA %u %lu %s\n", (unsigned)i,
                  (unsigned long)historyOffset, hex);
    historyOffset += static_cast<uint32_t>(wanted);
}
#endif // THESIS_F06_SD_SANDBOX
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
// The named facade is SD for ordinary profiles, DENY for F24K.
// F23D still retains long-lived File handles: no global lease asserted.
#define F23D_SD_ACCESS interposedSD
#include "f23d_sd_file_writer.h"
#undef F23D_SD_ACCESS
static F23DSD::Journal f23dJournal;
static F23DSDFileWriter f23dWriter;
static bool f23dInitialized=false;
static bool f23dOwnerIOAllowed=false;
inline bool f23dStorageFrozen(){return f23dJournal.captureActive() && !f23dOwnerIOAllowed;}
inline void pollF23DEvidence(bool radioBusy) {
  if(!f23dInitialized){
    if(radioBusy)return;
    f23dInitialized=true;
    const char* session=strrchr(DIRECTORY,'/');
    const bool authorized=ready && isAuthorizedVessel(vesselId) && vesselId==THESIS_F23D_SD_ROLE;
    if(!f23dJournal.initialize(vesselId,session?session+1:"",authorized,millis()))return;
    if(!f23dWriter.begin(DIRECTORY,vesselId)){f23dJournal.fail(F23DSD::Storage);return;}
  }
  if(!ready || !isAuthorizedVessel(vesselId) || vesselId!=THESIS_F23D_SD_ROLE){
    f23dJournal.fail(F23DSD::Storage|F23DSD::Identity);return;
  }
  if(f23dJournal.failed() || f23dJournal.sealed() || radioBusy || f23dJournal.defersStorage())return;
  f23dJournal.operationalLoss(lostUrgent);
  struct OwnerLease {
    OwnerLease(){f23dOwnerIOAllowed=true;}
    ~OwnerLease(){f23dOwnerIOAllowed=false;}
  } ownerLease;
  if(urgentQueue.count){pollEvents();return;}
  if(f23dJournal.pending() || f23dJournal.captureActive() ||
     (!f23dJournal.readyToStart() && f23dJournal.written())){
    f23dJournal.poll(f23dWriter,millis(),false);return;
  }
  poll();pollHistory();pollExport();
}
#endif
// F23D SD end
}
