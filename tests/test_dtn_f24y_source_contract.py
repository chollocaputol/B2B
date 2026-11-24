from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
read=lambda path:(root/path).read_text(encoding='utf-8')
b=read('firmware/shared/storage/dtn_f24y_checked_vfs.h')
t=read('firmware/universal/src/dtn_f24y_target_backend.cpp')
p=read('firmware/universal/platformio.ini')
l=read('firmware/shared/storage/thesis_sd_logger.h')
h=read('tests/dtn_f24y_checked_vfs_host.cpp')
checks=[]
def check(label,value):
    assert value,label
    checks.append(label)
for label,ok in [
 ('existing path grammar', 'DTNF24O::supportedPath(path,"",vessel,current)' in b),
 ('private descriptor array','Driver driver_; int fd_[2]{-1,-1}' in b),
 ('no raw descriptors exposed','int fd_[2]' in b and 'getFd' not in b),
 ('no destructor I/O','~Backend' not in b),
 ('no copy','Backend(const Backend&)=delete' in b),
 ('checked mount','if(!driver_.mount())return fail()' in b),
 ('checked unmount','if(!driver_.unmount())return fail()' in b),
 ('no unmount with descriptor','!active()||fd_[0]>=0||fd_[1]>=0' in b),
 ('fault sticky','fault_=true;return false' in b and 'mounted_&&!fault_' in b),
 ('parent required','!full(path,p)||!parent(p)' in b),
 ('only ENOENT absent','errno==ENOENT?FileKind::Missing:FileKind::Error' in b),
 ('stat distinguishes file and dir','FileKind::Regular:directory(s)?FileKind::Directory:FileKind::Error' in b),
 ('read mode no create','O_WRONLY|O_CREAT|O_APPEND:O_RDONLY' in b),
 ('no truncation','O_TRUNC' not in b),
 ('checked fd regular','!regular(s)' in b and 'fdStat(fd,&s)!=0' in b),
 ('size bounds','uint64_t(s.st_size)>uint64_t(SIZE_MAX)' in b),
 ('exact transfer','got==int64_t(n)||fail()' in b),
 ('size limit','n>size_t(INT_MAX)' in b),
 ('checked seek','Sys::seek(fd_[s],n)==int64_t(n)||fail()' in b),
 ('checked sync','Sys::sync(fd_[s])==0||fail()' in b and '::fsync(fd)' in b),
 ('checked close before release', b.index('if(Sys::close(fd_[s])!=0)return fail()')<b.index('fd_[s]=-1;return true;')),
 ('directory deliberately denied','bool child(uint8_t,uint8_t){return fail();}' in b),
 ('default off','#define THESIS_DTN_F24Y_BUILD_ONLY 0' in t),
 ('requires existing deny','THESIS_DTN_F24X_BUILD_ONLY != 1 || THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1' in t),
 ('SDK pinned','ESP_IDF_VERSION_VAL(5,5,5)' in t),
 ('checked SPI init','spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO)!=ESP_OK' in t),
 ('checked mount result','esp_vfs_fat_sdspi_mount(root(),&host,&device,&config,&card_)!=ESP_OK || !card_' in t),
 ('never format','config.format_if_mount_failed=false' in t),
 ('status check enabled','config.disk_status_check_enable=true' in t),
 ('two file budget','config.max_files=2' in t),
 ('known pins','bus.mosi_io_num=35;bus.miso_io_num=37;bus.sclk_io_num=36' in t and 'device.gpio_cs=GPIO_NUM_47' in t),
 ('known clock','host.max_freq_khz=1000' in t),
 ('checked unmount','esp_vfs_fat_sdcard_unmount(root(),card_)!=ESP_OK' in t),
 ('checked SPI free','spi_bus_free(SPI2_HOST)!=ESP_OK' in t),
 ('unmount before bus free', t.index('esp_vfs_fat_sdcard_unmount(root(),card_)')<t.index('spi_bus_free(SPI2_HOST)')),
 ('target link anchor','thesis_f24y_link_anchor __attribute__((used))' in t and '&Target::mount' in t and '&Target::unmount' in t),
 ('linker retention','-Wl,--undefined=thesis_f24y_link_anchor' in p),
 ('inherits X','extends = env:tbeam-supreme-f24x-owner-sos-deny-build-only' in p),
 ('logger stays denied','static DTNF24W::DenyMountFacade<File> interposedSD;' in l and 'DTNF24O::DenyWitness f24oNoPhysicalWitness{}' in l),
 ('no target backend in logger','DTNF24Y' not in l and 'dtn_f24y' not in l),
 ('actual host descriptor calls','NativeVfs::write(fd,p,' in h and 'NativeVfs::sync(fd)' in h),
 ('owner integration','W::MountOwner<Backend,MW>' in h and 'DTNF24X::Port<Owner,SimulatedWitness>' in h),
 ('receipt verification negative','DTNF24N::DenyReceiptVerifier{},"a","r"' in h),
 ('short write exercised','Fault::ShortWrite' in h and 'size()==44' in h),
 ('torn recovery exercised','static void torn(' in h and 'CHECK(!re.ledger().nextPending())' in h),
 ('denied witness tested','W::MountOwner<Backend,W::DenyWitness>' in h),
 ('mount failure exercised','Rig r(root,Fault::Mount)' in h),
 ('unmount failure exercised','r.c.fault=Fault::Unmount' in h),
]:check(label,ok)
# This is a scoped text check, not exhaustive active-call proof.
for path in (root/'firmware/universal/src').glob('*.cpp'):
    if path.name!='dtn_f24y_target_backend.cpp':
        check('no target runtime reference '+path.name,'thesis_f24y_link_anchor' not in path.read_text(encoding='utf-8') and 'dtn_f24y_checked_vfs' not in path.read_text(encoding='utf-8'))
print(f'F24Y checked target adapter source contract PASS: {len(checks)} checks; compile/link and physical permission separate')
