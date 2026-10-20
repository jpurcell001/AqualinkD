# AqualinkD — patch fork

Fork of [aqualinkd/AqualinkD](https://github.com/aqualinkd/AqualinkD) that carries the
patches our own install runs until they land upstream. Read
`~/projects/memory/home_assistant/integration_patches.md` § aqualinkd before changing
anything here: why each patch exists, what is deployed, how it is built and installed, and
the retire condition all live there, not in this repo.

## Branches and tags

- **`master`** mirrors upstream `master`. Never commit to it. Refresh with
  `git fetch upstream && git push origin upstream/master:master`.
- **`ha-prod`** is what runs: the upstream release the install is pinned to, plus one
  commit per patch. `source/version.h` carries a local marker
  (`<release>+<topic>.<n>`) so the running binary identifies itself, and the commit that
  produced each deployed build is tagged `v<marker>` (e.g. `v3.1.0+negedge.1`). Moving to
  a new upstream release means a fresh `ha-prod` from that tag with the patches
  cherry-picked; upstream releases from 3.1.1 ship their own `AGENTS.md`, so fold this
  section into it.
- **`fix/<topic>`** holds one change each, based on upstream `master`, without the
  version marker: the upstream PR candidates. Opening a PR is the owner's call.

Upstream's own AI development and review guidance is `AGENTS.md` on `master`
(`git show master:AGENTS.md`). Follow it for any source change, on any branch.
