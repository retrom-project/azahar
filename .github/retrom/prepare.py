from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / '.cache/retroarch'
COMMIT = '5a21e08a5de7649cfa6416d6843c0c40de27714e'
if not SOURCE.exists():
    subprocess.run(['git','clone','--filter=blob:none','--no-checkout','https://github.com/EmulatorJS/RetroArch',str(SOURCE)],check=True)
    subprocess.run(['git','-C',str(SOURCE),'checkout','--detach',COMMIT],check=True)
if subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','HEAD'],text=True).strip()!=COMMIT:
    raise RuntimeError('RETROARCH_SOURCE_MISMATCH')
subprocess.run(['git','-C',str(SOURCE),'diff','--quiet','HEAD','--'],check=True)
