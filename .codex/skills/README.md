# Codex Skills

This repository carries three GitHub workflow skills:

- `gh-create-issue-branch`: create a Korean GitHub issue and switch to a matching `<type>-#<issue>` branch.
- `gh-create-pr`: create a Korean GitHub pull request from an issue-linked branch.
- `gh-review-pr`: review GitHub pull requests and post line-specific review comments.

## Install

macOS or Linux:

```sh
./scripts/install-skills.sh
```

Windows PowerShell:

```powershell
.\scripts\install-skills.ps1
```

Both scripts install into `$CODEX_HOME/skills` when `CODEX_HOME` is set. Otherwise they install into the default Codex home at `~/.codex/skills`.
