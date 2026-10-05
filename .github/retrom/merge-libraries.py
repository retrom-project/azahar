from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
build=root/'.cache/build'
archives=sorted(p for p in build.rglob('*.a') if 'CMakeFiles' not in p.parts and not p.name.startswith(('libCatch','libgtest','libgmock')))
commands=['create '+str(root/'.cache/retroarch/libretro_emscripten.bc')]
commands.extend('addlib '+str(p) for p in archives)
commands.extend(['save','end',''])
subprocess.run(['emar','-M'],input='\n'.join(commands),text=True,check=True)
