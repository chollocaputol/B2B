#pragma once
// F24V: source-audited Arduino-ESP32 3.3.12 close-order guard declaration.
// This header does not instantiate, grant or enable a real SD mount owner.
// The core FS FileImpl uses shared_ptr and its VFSFileImpl destructor may
// invoke fclose/closedir when the last alias dies after lease revocation.
// This risk CANNOT be solved by the F24Q software lease alone.
#include <stdint.h>
namespace DTNF24V {
inline constexpr bool kPhysicalOwnerGranted=false;
inline constexpr bool kRawFileDestructionCertified=false;
inline constexpr bool kImplicitCloseDurable=false;
inline constexpr uint16_t kAuditedArduinoCoreMajor=3;
inline constexpr uint16_t kAuditedArduinoCoreMinor=3;
inline constexpr uint16_t kAuditedArduinoCorePatch=12;
} // namespace DTNF24V
