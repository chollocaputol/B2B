#pragma once
// F19R1: Vessel-003 OLED presentation policy ONLY. No SOS, radio, SD or GPIO.
#include <stdint.h>
#include <math.h>
namespace V003F19Display {
enum class Mode : uint8_t { Operational=0, Development=1 };
enum class HomeChoice : uint8_t { ScreenOff=0, Development=1, Back=2, None=3 };
static constexpr uint8_t kOperationalPageCount=4;
static constexpr uint8_t kDevelopmentPageCount=8;
static constexpr uint32_t kShortTapMaxMs=900U;
static constexpr uint32_t kModeToggleMinMs=2000U;
inline bool emergencyActive(bool localActive, unsigned receivedActive) {
    return localActive || receivedActive != 0U;
}
// True-north cardinal; NOT vessel-relative heading/compass pointing.
inline const char* cardinal8(double degrees) {
    if (!isfinite(degrees) || degrees < 0.0 || degrees >= 360.0) return "--";
    static const char* const names[8]={"N","NE","E","SE","S","SW","W","NW"};
    unsigned sector = static_cast<unsigned>((degrees + 22.5) / 45.0) % 8U;
    return names[sector];
}
class State {
    Mode mode_=Mode::Operational;
    uint8_t operationalPage_=0;
    bool homeMenuOpen_=false;
    uint8_t homeMenuCursor_=0;
    bool asleep_=false;
public:
    Mode mode() const { return mode_; }
    bool operational() const { return mode_==Mode::Operational; }
    uint8_t operationalPage() const { return operationalPage_; }
    bool homeMenuOpen() const { return homeMenuOpen_; }
    uint8_t homeMenuCursor() const { return homeMenuCursor_; }
    bool screenAsleep() const { return asleep_; }
    bool wake() { if (!asleep_) return false; asleep_=false; return true; }
    bool openHomeMenu(uint32_t heldMs, bool sosActive) {
        if (sosActive || asleep_ || !operational() || operationalPage_!=0 ||
            homeMenuOpen_ || heldMs < kShortTapMaxMs) return false;
        homeMenuOpen_=true; homeMenuCursor_=0; return true;
    }
    bool moveHomeCursor(uint32_t heldMs, bool sosActive) {
        if (sosActive || asleep_ || !homeMenuOpen_ || heldMs >= kShortTapMaxMs) return false;
        homeMenuCursor_=static_cast<uint8_t>((homeMenuCursor_+1U)%3U);return true;
    }
    HomeChoice confirmHomeMenu(uint32_t heldMs, bool sosActive) {
        if (sosActive || asleep_ || !homeMenuOpen_ || heldMs < kShortTapMaxMs)
            return HomeChoice::None;
        homeMenuOpen_=false;
        const HomeChoice choice=static_cast<HomeChoice>(homeMenuCursor_);
        if (choice==HomeChoice::ScreenOff) asleep_=true;
        if (choice==HomeChoice::Development) { mode_=Mode::Development; operationalPage_=0; }
        return choice;
    }
    bool tapOperational(uint32_t heldMs, bool sosActive) {
        if (!operational() || asleep_ || homeMenuOpen_ || sosActive ||
            heldMs>=kShortTapMaxMs) return false;
        operationalPage_=static_cast<uint8_t>((operationalPage_+1U)%kOperationalPageCount);
        return true;
    }
    bool returnFromDevelopment(bool firstPage, uint32_t heldMs, bool sosActive) {
        if (sosActive || operational() || !firstPage || heldMs < kModeToggleMinMs) return false;
        mode_=Mode::Operational;
        operationalPage_=0;
        homeMenuOpen_=false;
        asleep_=false;
        return true;
    }
};
}
