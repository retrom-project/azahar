# Retrom azahar Web port

`retrom-fork.json` owns the upstream baseline, maintenance branch, adapter ABI and
closed release asset set. The upstream checkout and RetroArch frontend are pinned;
`.github/retrom/prepare.py` verifies the frontend commit before compiling.

The native content receipt stays pending until the deferred `System::Load` finishes.
Encrypted content returns -2, other rejected content -1, and accepted content 1.
The host must not treat the EmulatorJS startup event as content acceptance.
The owned state receipt is reset by `retrom_state_load_begin` and completed only by
`retro_unserialize`; `retrom_state_load_result` returns 0, 1 or -1. The adapter waits
for this result before entering RUNNING. Container builds pass the actual source
revision explicitly so saved states never carry an unknown build revision.

Build with Python 3, Git, Docker, a C compiler, Node.js, and 7-Zip installed.
Emscripten is selected by immutable image digest in the candidate build script.
Azahar also requires CMake 3.26 or newer; CI uses 3.31.6. Set
`RETROM_CMAKE_ROOT` to the installation directory containing `bin/cmake`.
Initialize recursive Git submodules before building.

```sh
python3 -m unittest discover -s .github/rpg-runtime -p 'test_*.py'
bash .github/rpg-runtime/build-candidate.sh .cache/candidate
```

The candidate output directory must be empty. Source fingerprints cover tracked and
untracked source, including initialized submodules, while ignoring generated caches.
A dirty candidate is for local PFB validation only. Release packaging requires a
clean candidate at the tag commit and validates every declared file and checksum.

CI builds the same candidate and uploads it for review. The release workflow only
publishes existing annotated tags reachable from the maintenance branch. Product
acceptance and explicit publication authorization remain required before tagging.
