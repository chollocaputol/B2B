#!/usr/bin/env python3
"""F26A: pinned S3 SPI mapping and scoped source checks, not physical qualification."""
import configparser
import hashlib
from pathlib import Path
import re
import sys

root=Path(__file__).resolve().parents[1]
if len(sys.argv)!=2:
    raise SystemExit('usage: test_dtn_f26a_spi_source_contract.py <Arduino-core-root>')
core=Path(sys.argv[1])
checks=0
def check(label,ok):
    global checks
    if not ok: raise AssertionError('F26A: '+label)
    checks+=1
read=lambda p:(root/p).read_text(encoding='utf-8')
pins={
 'cores/esp32/esp32-hal-spi.h':'5A8A6BF1FAE19B6DA0A72C5B8459934EDD828A56FE765C964E5C1C252D6461DF',
 'cores/esp32/esp32-hal-spi.c':'A88381169809486FF8263C7EB7E04B5C42B9ECCECEA21C256E1AD3DC9B1B4AED',
 'libraries/SPI/src/SPI.cpp':'56BD839008C808F34DEFC2AF715B76810F54F844C0643A6FBC9F2D8F3BDA00A8',
 'libraries/SPI/src/SPI.h':'4FDFA675A719087981D67CFA8D6C1719E17386121DD859B28114CCA459F892DF',
}
for f,h in pins.items():
    check('pinned core '+f,hashlib.sha256((core/f).read_bytes()).hexdigest().upper()==h)
h=(core/'cores/esp32/esp32-hal-spi.h').read_text()
c=(core/'cores/esp32/esp32-hal-spi.c').read_text()
spi=(core/'libraries/SPI/src/SPI.cpp').read_text()
check('S3 Arduino indices', '#define FSPI 0  //' in h and '#define HSPI 1  //' in h)
for prefix in ('', 'NULL, '):
    check('S3 controller array '+prefix, bool(re.search(r'#if CONFIG_IDF_TARGET_ESP32S2\s*\|\|\s*CONFIG_IDF_TARGET_ESP32S3[^\n]*\n\s*\{\(volatile spi_dev_t \*\)\(DR_REG_SPI2_BASE\), '+prefix+r'0,.*\n\s*\{\(volatile spi_dev_t \*\)\(DR_REG_SPI3_BASE\), '+prefix+r'1,',c)))
check('constructor retains index','_spi_num(spi_bus), _spi(NULL)' in spi)
check('begin starts selected bus','spiStartBus(_spi_num, _div, SPI_MODE0, SPI_MSBFIRST)' in spi)
check('global SPI is FSPI on non-classic ESP32','SPIClass SPI(FSPI);' in spi)
b=read('firmware/universal/src/dtn_f24y_target_backend.cpp')
lora=read('firmware/universal/src/lora_manager.cpp')
main=read('firmware/universal/src/main.cpp')
logger=read('firmware/shared/storage/thesis_sd_logger.h')
probe=read('firmware/shared/storage/sd_probe.h')
rt=read('firmware/shared/storage/dtn_f25a_runtime_deny.h')
board=read('firmware/shared/hardware/tbeam_pins.h')
check('S3 compile guard','#if !defined(CONFIG_IDF_TARGET_ESP32S3) || !CONFIG_IDF_TARGET_ESP32S3' in b)
check('Arduino index compile check','static_assert(FSPI==0 && HSPI==1,' in b)
check('single explicit IDF host','constexpr spi_host_device_t kSdHost=SPI3_HOST;' in b and b.count('SPI3_HOST')==1)
check('distinct host assertion','static_assert(kSdHost!=SPI2_HOST,' in b and b.count('SPI2_HOST')==1)
for label,expr in [('init','spi_bus_initialize(kSdHost,&bus,SPI_DMA_CH_AUTO)'),('host slot','host.slot=kSdHost;'),('device','device.host_id=kSdHost;'),('free','spi_bus_free(kSdHost)')]:
    check(label,b.count(expr)==1)
check('no alternate bus operations',b.count('spi_bus_initialize(')==1 and b.count('spi_bus_free(')==1 and b.count('host.slot=')==1 and b.count('device.host_id=')==1)
check('unmount before bus free',b.index('esp_vfs_fat_sdcard_unmount(root(),card_)')<b.index('spi_bus_free(kSdHost)'))
check('SD pins unchanged','bus.mosi_io_num=35;bus.miso_io_num=37;bus.sclk_io_num=36;' in b and 'device.gpio_cs=GPIO_NUM_47;' in b)
check('SD clock and no format','host.max_freq_khz=1000;' in b and 'config.format_if_mount_failed=false;' in b)
check('LoRa explicit instance','SPIClass loraSPI(FSPI);' in lora and 'Module radioModule(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);' in lora)
check('LoRa begin unchanged','loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);' in lora)
for pin,val in [('LORA_SCK',12),('LORA_MISO',13),('LORA_MOSI',11),('LORA_CS',10)]:
    check(pin,bool(re.search(r'^#define '+pin+r'\s+'+str(val)+r'\s*$',board,re.M)))
check('legacy HSPI excluded','#if THESIS_DTN_F25A_BUILD_ONLY != 1\nstatic SPIClass sdSPI(HSPI);\n#endif' in logger)
check('probe conditional','#if THESIS_SD_PROBE == 1\n#include "../../shared/storage/sd_probe.h"' in main and 'static SPIClass sdSPI(HSPI);' in probe)
check('legacy init bypass','LEGACY_SD_INIT_BYPASSED; NO_SESSION_AUTHORITY' in main)
check('actual runtime immutable witnesses','const DTNF24W::DenyWitness mountWitness_{};' in rt and 'const DTNF25B::DenyWitness evidenceWitness_{};' in rt and 'service_.start(' not in rt)
# Inventory explicit SPI owners in the scoped firmware, not a complete library or preprocessor proof.
owners=[];buscalls=[]
for f in sorted(f for base in ('firmware/shared','firmware/universal/src','firmware/universal/include') for f in (root/base).rglob('*')):
    if f.suffix not in ('.h','.hpp','.cpp','.c','.cc','.ino'): continue
    # Ignore PIO generated/downloaded sources on Windows.
    rel=f.relative_to(root).as_posix()
    if any(part.startswith('.') for part in f.relative_to(root).parts): continue
    text=f.read_text(encoding='utf-8',errors='strict')
    for match in re.finditer(r'^\s*(?:static\s+)?SPIClass\s+(\w+)\s*\((FSPI|HSPI)\)\s*;',text,re.M): owners.append((rel,match[1],match[2]))
    if re.search(r'\bspi_bus_(?:initialize|free)\s*\(',text): buscalls.append(rel)
    check_global=not re.search(r'\bSPI\s*\.\s*begin\s*\(',text)
    if not check_global: raise AssertionError('F26A: direct global SPI.begin in '+rel)
check('known explicit Arduino owner inventory',set(owners)=={('firmware/universal/src/lora_manager.cpp','loraSPI','FSPI'),('firmware/shared/storage/thesis_sd_logger.h','sdSPI','HSPI'),('firmware/shared/storage/sd_probe.h','sdSPI','HSPI')} and len(owners)==3)
check('known IDF bus operation owner',buscalls==['firmware/universal/src/dtn_f24y_target_backend.cpp'])
check('no scoped direct global SPI.begin',True)
cfg=configparser.ConfigParser(interpolation=None);cfg.read_string(read('firmware/universal/platformio.ini'))
profile='env:tbeam-supreme-f26a-spi-separation-deny-build-only'
prior='env:tbeam-supreme-f25d-integrated-acceptance-deny-build-only'
p=cfg[profile]
check('same flags only',set(p)=={'extends','build_flags'} and p['extends']==prior and p['build_flags'].strip()=='${'+prior+'.build_flags}')
def flags(section): return re.sub(r'\$\{([^}]+)\.build_flags\}',lambda m:flags(m[1]),cfg[section]['build_flags'])
effective={}
for name,value,undef in re.findall(r'-D\s+(\w+)=([01])\b|-U(\w+)',flags(profile)):
    if undef: effective.pop(undef,None)
    else: effective[name]=int(value)
for f in ['THESIS_DTN_F24Y_BUILD_ONLY','THESIS_DTN_F24Z_BUILD_ONLY','THESIS_DTN_F25A_BUILD_ONLY','THESIS_DTN_F25B_BUILD_ONLY','THESIS_DTN_F25C_BUILD_ONLY','THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY','THESIS_SD_LOGGER']:
    check('required '+f,effective.get(f)==1)
for f in ['THESIS_SD_PROBE','THESIS_F23D_SD_ROLE','THESIS_F23D_CANDIDATE','THESIS_F23D_PHYSICAL_CANDIDATE','THESIS_MULTIHOP_DEV_TEST','THESIS_MULTIHOP_BENCH_RF','THESIS_LAB_RELAY_TX','THESIS_FLEET_USB_ENROLLMENT','THESIS_F22_FLEET_OPERATIONAL','THESIS_V003_F15_SD_PILOT_ARMED','THESIS_V003_F13_LIVE_OUTPUTS']:
    check('disabled '+f,not effective.get(f,0))
print(f'F26A SPI separation source contract PASS: {checks} checks; scoped mapping and denial only; no physical ownership')
