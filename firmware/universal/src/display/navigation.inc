// OLED BUTTON NAVIGATION
// ============================================================
// GPIO0 user button is active LOW. Debouncing prevents one physical
// press from skipping multiple pages. Page changes only on a new press.
void handleOLEDButton() {
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  // Observe transitions and restore SOS after 20 seconds without a GPIO0 tap.
  // Neither this check nor the OLED button can initiate/cancel real distress.
  if(f21r42SOSBrowse.observe(V003F19Display::emergencyActive(
       hasActiveLocalSOS(),getActiveSOSCount()),millis())) {
    if(!f21r42SOSBrowse.emergency() || f21r42SOSBrowse.forceSOS()) sosMenuOpen=false;
    if(f21r42SOSBrowse.forceSOS()) {
      f19Display.dismissHomeMenuForSOS();
      oledPeerMenuOpen=false;
    }
    lastOLEDUpdate=0;
  }
  // Selection must not obscure an SOS indefinitely when left unattended.
  if(sosMenuOpen && f21r42SOSBrowse.idleForReturn(millis())) {
    sosMenuOpen=false;
    f21r42SOSBrowse.returnToSOS(millis());
    lastOLEDUpdate=0;
  }
#endif
  const int reading = digitalRead(USER_BUTTON_PIN);
  if (OLEDButton::stableEdge(reading, buttonLastReading, buttonStableState,
                             buttonLastDebounceTime, millis(), BUTTON_DEBOUNCE_MS)) {
    if (buttonStableState == LOW) {
      buttonPressStartedAt = millis();
    } else {
      unsigned long duration = millis() - buttonPressStartedAt;
#if THESIS_V003_F19_DUAL_DISPLAY == 1
      // SOS still preempts the display initially and after idle. GPIO0 can
      // VIEW other pages or choose a received SOS without altering distress.
      // GPIO2 remains the only physical SOS start/cancel input.
      const bool f19ActiveSOS=f21r42SOSBrowse.emergency();
      if (f19ActiveSOS) {
        // F21R4.3: SOS is always visible first, but during deliberate
        // browsing GPIO0 retains each operational page's own hold menu.
        // Menus are view-only; GPIO2 and the LoRa SOS state are untouched.
        if(sosMenuOpen) {
          if(duration>=BUTTON_LONG_PRESS_MS) confirmSOSMenu();
          else advanceSOSMenu();
          f21r42SOSBrowse.noteInput(millis());
        } else if(f19Display.homeMenuOpen()) {
          if(duration<BUTTON_LONG_PRESS_MS)
            f19Display.moveHomeCursorForSOS(duration);
          else {
            const auto choice=f19Display.confirmHomeMenuForSOS(duration);
            if(choice==V003F19Display::HomeChoice::Portrait ||
               choice==V003F19Display::HomeChoice::Landscape)
              Serial.printf("[F21R43-SOS-UI] ORIENTATION=%s DISPLAY_ONLY\n",
                  f19Display.portrait()?"PORTRAIT":"LANDSCAPE");
            else if(choice==V003F19Display::HomeChoice::None)
              Serial.println("[F21R43-SOS-UI] HOME_CHOICE_LOCKED_WHILE_SOS");
          }
          f21r42SOSBrowse.noteInput(millis());
        } else if(oledPeerMenuOpen) {
          if(duration>=BUTTON_LONG_PRESS_MS) confirmOLEDPeerMenu();
          else advanceOLEDPeerMenu();
          f21r42SOSBrowse.noteInput(millis());
        } else if(duration>=BUTTON_LONG_PRESS_MS) {
          if(f21r42SOSBrowse.forceSOS() || f21r42SOSBrowse.page()==3U) {
            openSOSMenu();
            f21r42SOSBrowse.noteInput(millis());
          } else if(f21r42SOSBrowse.page()==0U) {
            f19Display.openHomeMenuForSOS(duration);
            f21r42SOSBrowse.noteInput(millis());
          } else if(f21r42SOSBrowse.page()==2U) {
            openOLEDPeerMenu();
            f21r42SOSBrowse.noteInput(millis());
          } else {
            f21r42SOSBrowse.returnToSOS(millis()); // WX: hold = SOS shortcut
          }
        } else {
          f21r42SOSBrowse.tap(millis()); // forced SOS -> Home -> WX -> Peers -> SOS
        }
        lastOLEDUpdate=0;return;
      }
      if (f19Display.wake()) {
        Serial.println("[F19-UI] OLED_WAKE DISPLAY_ONLY");
        lastOLEDUpdate=0; return;
      }
      if (f19Display.operational()) {
        if (f19Display.homeMenuOpen()) {
          if (f19Display.moveHomeCursor(duration,false)) {
            Serial.printf("[F19-UI] HOME_MENU_CURSOR=%u\n",
                unsigned(f19Display.homeMenuCursor()));
          } else {
            const auto choice=f19Display.confirmHomeMenu(duration,false);
            if (choice==V003F19Display::HomeChoice::ScreenOff)
              Serial.println("[F19-UI] OLED_SCREEN_OFF ONLY; CORE_RUNNING=1");
            else if (choice==V003F19Display::HomeChoice::Development) {
              oledPage=0; selectedOLEDPeerId=0; oledPeerMenuOpen=false;
              sosMenuOpen=false; alertDemoMode=0; selectedSOSOrigin=0; selectedSOSEvent=0;
#if THESIS_SD_OLED_UI == 1
              sdOLEDSubview=0;
#if THESIS_F06_SD_HYBRID == 1
              sdStorageMenuOpen=false; sdHybridMenuOpen=false;
#endif
#endif
              Serial.println("[F19-UI] MODE=DEVELOPMENT DISPLAY_ONLY OUTPUTS=OFF");
            } else if (choice==V003F19Display::HomeChoice::Back)
              Serial.println("[F19-UI] HOME_MENU_BACK");
            else if (choice==V003F19Display::HomeChoice::Portrait ||
                     choice==V003F19Display::HomeChoice::Landscape)
              Serial.printf("[F21-UI] ORIENTATION_SELECTED=%s VOLATILE=1 DISPLAY_ONLY\n",
                  f19Display.portrait()?"PORTRAIT":"LANDSCAPE");
          }
        } else if (f19Display.operationalPage()==2U && oledPeerMenuOpen) {
          if (duration>=BUTTON_LONG_PRESS_MS) confirmOLEDPeerMenu();
          else advanceOLEDPeerMenu();
        } else if (f19Display.openHomeMenu(duration,false)) {
          Serial.println("[F19-UI] HOME_MENU_OPEN");
        } else if (f19Display.operationalPage()==2U &&
                   duration>=BUTTON_LONG_PRESS_MS) {
          openOLEDPeerMenu();
          Serial.println("[F19-UI] OP_PEER_MENU_OPEN");
        } else if (f19Display.tapOperational(duration,false)) {
          if (f19Display.operationalPage()!=2U) oledPeerMenuOpen=false;
          Serial.printf("[F19-UI] OP_PAGE=%u/4 DISPLAY_ONLY\n",
                        unsigned(f19Display.operationalPage()+1U));
        }
        lastOLEDUpdate=0; return;
      }
      // Development page 1 long hold returns to Operational; all other eight
      // existing F18 development page controls remain unchanged below.
      if (f19Display.returnFromDevelopment(oledPage==0U,duration,false)) {
        oledPage=0; oledPeerMenuOpen=false; sosMenuOpen=false; alertDemoMode=0;
#if THESIS_SD_OLED_UI == 1
        sdOLEDSubview=0;
#if THESIS_F06_SD_HYBRID == 1
        sdHybridMenuOpen=false; sdStorageMenuOpen=false;
#endif
#endif
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
        f18WeatherMicroView=false;
#endif
#if THESIS_STUDY_SF_CLOUD == 1
        studyShore.clearSimulation();
        thesisStudySfSetShoreAllowed(studyShore.allowed());
#endif
        Serial.println("[F19-UI] MODE=OPERATIONAL DISPLAY_ONLY OUTPUTS=OFF");
        lastOLEDUpdate=0; return;
      }
#endif
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
      if (oledPage == 1 && duration >= BUTTON_LONG_PRESS_MS) {
        f18WeatherMicroView = !f18WeatherMicroView;
        Serial.printf("[F18-WX-UI] VIEW=%s OUTPUTS=OFF\n",
                      f18WeatherMicroView ? "MICRO_EXPERIMENTAL" : "MACRO_EMPIRICAL");
      } else
#endif
      if (oledPage == 2 && duration >= BUTTON_LONG_PRESS_MS) {
        if (oledPeerMenuOpen) confirmOLEDPeerMenu();
        else openOLEDPeerMenu();
      } else if (oledPeerMenuOpen && oledPage == 2 && duration < BUTTON_LONG_PRESS_MS) {
        advanceOLEDPeerMenu();
      } else if (oledPage == 6 && duration >= BUTTON_LONG_PRESS_MS) {
        if (sosMenuOpen) confirmSOSMenu(); else openSOSMenu();
      } else if (oledPage == 6 && sosMenuOpen && duration < BUTTON_LONG_PRESS_MS) {
        advanceSOSMenu();
#if THESIS_SD_OLED_UI == 1
#if THESIS_F06_SD_HYBRID == 1
      } else if (oledPage == 7 && sdHybridMenuOpen) {
        // Mode submenu: TAP moves, HOLD confirms. BACK returns to SD menu.
        if (duration >= BUTTON_LONG_PRESS_MS) {
          if (sdHybridMenuCursor < 2) {
#if THESIS_STUDY_SF_BUILD_ONLY == 1
            thesisStudySfSetContinuous(sdHybridMenuCursor == 1);
            Serial.printf("[STUDY-SF] RECORD_MODE=%s; VOLATILE=1\n",
                thesisStudySfIsContinuous() ? "CONTINUOUS" : "SOS_ARMED");
#else
            ThesisSDLogger::hybridSetContinuous(sdHybridMenuCursor == 1);
            Serial.printf("[OLED-SD] MODE_CONFIRMED=%s\n",
                sdHybridMenuCursor == 1 ? "CONTINUOUS" : "SOS_ARMED");
#endif
            sdOLEDSubview = 0;
          } else {
            sdStorageMenuOpen = true;
            sdStorageMenuCursor = 2;
            Serial.println("[OLED-SD] MODE_BACK_TO_SD_MENU");
          }
          sdHybridMenuOpen = false;
        } else {
          sdHybridMenuCursor = (sdHybridMenuCursor + 1U) % 3U;
          Serial.printf("[OLED-SD] MODE_CURSOR=%u\n", (unsigned)sdHybridMenuCursor);
        }
      } else if (oledPage == 7 && sdStorageMenuOpen) {
        // SD menu: Journals, USB, Recording Mode, Back. No direct file writes.
        if (duration >= BUTTON_LONG_PRESS_MS) {
          switch (sdStorageMenuCursor) {
            case 0:
              sdOLEDSubview = 1; // Journals: cached counters only
              sdStorageMenuOpen = false;
              Serial.println("[OLED-SD] OPEN_JOURNALS");
              break;
            case 1:
              sdOLEDSubview = 2; // USB: instructional text only
              sdStorageMenuOpen = false;
              Serial.println("[OLED-SD] OPEN_USB_READBACK");
              break;
            case 2:
              sdStorageMenuOpen = false;
              sdHybridMenuOpen = true;
#if THESIS_STUDY_SF_BUILD_ONLY == 1
              sdHybridMenuCursor = thesisStudySfIsContinuous() ? 1 : 0;
#else
              sdHybridMenuCursor = ThesisSDLogger::hybridIsContinuous() ? 1 : 0;
#endif
              Serial.println("[OLED-SD] MODE_MENU_OPEN");
              break;
            default:
              sdStorageMenuOpen = false;
              sdOLEDSubview = 0;
              Serial.println("[OLED-SD] STORAGE_MENU_BACK");
              break;
          }
        } else {
          sdStorageMenuCursor = (sdStorageMenuCursor + 1U) % 4U;
          Serial.printf("[OLED-SD] STORAGE_CURSOR=%u\n", (unsigned)sdStorageMenuCursor);
        }
      } else if (oledPage == 7 && duration >= BUTTON_LONG_PRESS_MS) {
        // Hold on status, journals or USB always opens the same options menu.
        sdStorageMenuOpen = true;
        sdStorageMenuCursor = 0;
        Serial.println("[OLED-SD] STORAGE_MENU_OPEN");
#else
      } else if (oledPage == 7 && duration >= BUTTON_LONG_PRESS_MS) {
        // Keep original three-view hold navigation for the non-hybrid UI.
        sdOLEDSubview = (sdOLEDSubview + 1U) % 3U;
        Serial.printf("[OLED-SD] VIEW=%u DISPLAY_ONLY\n", (unsigned)sdOLEDSubview);
#endif
#endif
#if THESIS_STUDY_SF_CLOUD == 1
      } else if (oledPage == 8 && duration >= BUTTON_LONG_PRESS_MS) {
        studyShore.cycle(!f19Display.operational() && !hasActiveLocalSOS() && getActiveSOSCount()==0);
        thesisStudySfSetShoreAllowed(studyShore.allowed());
        Serial.printf("[SHORE] MODE=%s GATE=%s; VOLATILE=1; GPS_UNCHANGED=1\n",StudyShore::modeName(studyShore.mode()),studyShore.allowed()?"ALLOWED":"BLOCKED");
#endif
      } else if (oledPage == 4 && duration >= BUTTON_LONG_PRESS_MS) {
        alertDemoMode = (alertDemoMode + 1) % 4;
        Serial.printf("[ALERT-DEMO] OLED-only mode: %u (0=LIVE,1=NORMAL,2=FRONT,3=SQUALL)\n", alertDemoMode);
      } else if (duration < BUTTON_LONG_PRESS_MS) {
        oledPage = (oledPage + 1) % kOLEDPageCount;
#if THESIS_SD_OLED_UI == 1
        if (oledPage != 7) sdOLEDSubview = 0;
#endif
        // Do not leave a simulated alert active after departing test page.
        if (oledPage != 4) alertDemoMode = 0;
        Serial.printf("[OLED] Page changed to %u\n", oledPage);
      }
      lastOLEDUpdate = 0;
    }
  }
}

