# Retrom azahar maintenance

- Read `retrom-fork.json` and `docs/RETROM.md` before source or packaging changes.
- Keep the upstream mirror untouched. Retrom changes belong on `retrom/g62cfcfb5e488`;
  use `feat/*`, `fix/*`, or `build/*` branches from that maintenance branch.
- This fork owns native source, the pinned Emscripten/RetroArch build, adapter ABI,
  release assets and licenses. Runtime aggregation must not compile or patch cores.
- Run the native receipt regression, Python packaging tests, and
  `.github/rpg-runtime/build-candidate.sh` before review. Validate changed behavior
  through Retrom PFB import, review, gameplay/input, save and new-session restore.
- Stable annotated `retrom-core-g62cfcfb5e488-rN` tags must belong to the maintenance
  branch. Releases are immutable; never move tags or overwrite assets. Candidate
  validation does not authorize a release, merge, or production deployment.
- Never commit games, BIOS, credentials, build outputs, dependency caches or host paths.
