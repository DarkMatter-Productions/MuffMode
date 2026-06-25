# MuffMode Copilot Instructions

Read and follow the repository-wide agent guide in [`AGENTS.md`](../AGENTS.md). It is the canonical source for architecture, build/test commands, documentation rules, versioning, and multi-agent workflow.

Bootstrap reminders for VS Code and GitHub Copilot:

- Treat existing worktree changes as user work unless asked to clean them up.
- Keep edits scoped and avoid broad formatting or unrelated refactors.
- Prefer `rg` for discovery.
- Use `src/sgame/muffmode/` for MuffMode server feature bodies, with thin hooks in upstream-style game files.
- Use the scripts in `scripts/ci/` for build, test, analysis, and release checks.
- Do not commit, push, or open PRs unless explicitly asked.
