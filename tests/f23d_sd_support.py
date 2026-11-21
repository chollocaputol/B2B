"""Independent removal proof for opt-in SD additions."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def remove_sd(text):
 out=[];inside=False
 for line in text.splitlines(keepends=True):
  if line.strip() in ("// F23D SD begin","; F23D SD begin"):
   assert not inside;inside=True
  elif line.strip() in ("// F23D SD end","; F23D SD end"):
   assert inside;inside=False
  elif inside and line.startswith("// F23D SD restore: "):
   out.append(line[len("// F23D SD restore: "):])
  elif not inside:out.append(line)
 assert not inside
 return "".join(out)
def assert_protected_folder(base,folder):
 changed=subprocess.check_output(["git","diff",base,"--name-only","--",folder],cwd=ROOT,text=True).splitlines()
 for p in changed:
  if p=="firmware/shared/storage/f23d_sd_file_writer.h":
   probe=subprocess.run(["git","cat-file","-e",base+":"+p],cwd=ROOT,capture_output=True)
   assert probe.returncode!=0;continue
  assert p=="firmware/shared/storage/thesis_sd_logger.h",p
  before=subprocess.check_output(["git","show",base+":"+p],cwd=ROOT,text=True)
  assert remove_sd((ROOT/p).read_text())==before,p
