#pragma once
#include "f23d_sd_contract.h"
#include "f23d_sd_journal.h"
#include "f23d_lab_diagnostics.h"
#if THESIS_F23D_SD_EVIDENCE
extern bool thesisF23dSDCapture(const F23DSD::Record&);
extern void thesisF23dSDFormatLoss();
extern bool thesisF23dSDHealthy();
extern bool thesisF23dSDCommand(const char*);
namespace F23DSD {
inline bool describe(Record& row,const char* stage,const char* validation="",const char* state="") {
 return token(row.record,sizeof(row.record),stage) &&
        token(row.validation,sizeof(row.validation),validation) &&
        token(row.state,sizeof(row.state),state);
}
inline void packet(Record& row,const VesselPacket& p) {
 row.origin=p.originNodeId;row.event=p.sequenceNumber;row.type=p.packetType;row.hop=p.hopCount;
 row.bytes=sizeof(p);row.wire=F23DReadiness::fingerprint(reinterpret_cast<const uint8_t*>(&p),sizeof(p));
 row.canonical=F23DReadiness::fingerprint(reinterpret_cast<const uint8_t*>(&p),sizeof(p),true);
}
}
#endif
