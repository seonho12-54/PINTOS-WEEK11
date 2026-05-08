---
name: gh-review-pr
description: Review GitHub pull requests using the GitHub CLI and local repository context, and post line-specific review comments directly on the provided GitHub PR diff. Use when the user provides a GitHub PR URL or number and asks Codex to review it, inspect PR risk, find bugs, validate existing review comments, or leave code-line review feedback on GitHub.
---

# GitHub PR Review

## Default Behavior

When this skill is invoked with a GitHub PR URL or PR number, review the PR and post actionable findings directly as line-specific GitHub PR review comments on the diff. Do not stop at a local prose-only review unless GitHub posting is blocked or the user explicitly asks for local-only analysis.

If there are no correctness findings worth posting, do not invent comments. Report that no line-specific findings were found and mention the overall PR risk level, any residual risks, and tests not run.

## Workflow

1. Identify the repository and PR number from the user's URL, branch, or text.
2. Confirm GitHub access with `gh auth status` if auth is uncertain.
3. Fetch PR metadata:

```bash
gh pr view <number> --repo <owner>/<repo> --json number,title,body,state,headRefName,baseRefName,author,files,commits,additions,deletions,url,reviews,comments
```

4. Read the diff:

```bash
gh pr diff <number> --repo <owner>/<repo>
```

5. Compare changed code against local context. Use `rg`, `nl -ba`, and focused file reads to inspect call sites, invariants, tests, and adjacent implementations.
6. If needed, fetch the base/head refs with `git fetch origin <branch>` and compare locally with `git diff origin/<base>...origin/<head>`.
7. Run targeted tests only when they are relevant, affordable, and safe in the current repository. If tests are not run, say so.
8. Assess the overall PR risk level based on blast radius, touched subsystems, correctness/security impact, migration or data-loss potential, concurrency risk, and test coverage.
9. Post each actionable finding as a line-specific GitHub PR review comment on the relevant diff line.
10. Report posted findings first, ordered by severity. Keep findings grounded in exact changed files and tight line references, then state the overall PR risk level.

## Review Standards

- Prioritize correctness bugs, regressions, security issues, data loss, race conditions, broken tests, and missing required behavior.
- Do not lead with style, naming, formatting, or broad refactor suggestions unless they hide a real risk.
- Treat PR descriptions as claims to verify, not proof.
- Prefer changed lines for findings. Reference unchanged lines only when the bug is created by an interaction with changed code.
- Distinguish confirmed bugs from questions or assumptions.
- Assign a risk level to the PR as a whole: `Critical`, `High`, `Medium`, or `Low`.
- Use `Critical` for likely production-breaking, security-sensitive, data-loss, or broadly blocking changes; `High` for serious regressions in important paths; `Medium` for contained behavioral risk or moderate test gaps; `Low` for small, well-tested, localized changes.
- For each finding, make the practical risk clear in the title or body, especially when severity and likelihood differ.
- If there are no findings, say that clearly and mention any residual risk or tests not run.

## Output Shape

For a conversational review, use this order:

1. Findings
2. Overall PR risk level
3. Open questions or assumptions
4. Brief test notes
5. Short summary only if useful

For in-app code review comments, emit one `::code-comment{...}` directive per finding with:

- `title`: short severity-oriented title
- `body`: one paragraph explaining the risk and the failing scenario
- `file`: absolute path or workspace-relative path
- `start` and optional `end`: tight 1-based line range
- `priority`: `0` for blocking, `1` high, `2` medium, `3` low
- `confidence`: decimal estimate from `0` to `1`

When writing local review text, include a short risk line such as `Risk: High` for the PR overall. If there are findings, each finding should make the risk obvious with phrasing like `[High risk]` in the title or a first sentence that names the failure mode.

## GitHub Posting

For this user workflow, invoking `$gh-review-pr` with a GitHub PR link is an explicit request to post review comments on that PR's code lines. Post actionable findings directly on the PR diff by default.

Do not approve or request changes unless the user explicitly asks for that outcome. Use line-specific comments for findings. Use a general summary comment only when there are useful review notes that cannot be attached to a changed line.

All GitHub review bodies posted with this skill must start with `AI) `. This applies to line-specific PR review comments and general PR review comments. For example, post `AI) 여기도 size == 0일 때...` instead of `여기도 size == 0일 때...`.

When posting the review:

1. Re-check the target repository, PR number, and current review findings.
2. Post each actionable finding against the most relevant specific diff line. If a finding cannot be attached to a changed line, include it in a short general summary comment instead.
3. Prefer `--comment` for a neutral summary review unless the user asks to approve or request changes.
4. Use `--request-changes` only when the user asks for that outcome or the review policy for the task clearly requires it.
5. Include the same findings that were shown locally, with enough file and line context for the author to act. Each posted finding body should include a compact `위험도: <Critical|High|Medium|Low>` or `Risk: <Critical|High|Medium|Low>` label after the `AI) ` prefix.
6. After posting, report that the review was submitted and mention any command/test limitations.

Post a line-specific PR review comment:

```bash
gh api -X POST repos/<owner>/<repo>/pulls/<number>/comments \
  -f body="<comment body>" \
  -f commit_id="<head commit sha>" \
  -f path="<changed file path>" \
  -F line=<new-file-line-number> \
  -f side=RIGHT
```

Use `side=RIGHT` for added or current lines in the PR diff. Use `side=LEFT` only when commenting on removed lines. Fetch the head commit SHA with:

```bash
gh pr view <number> --repo <owner>/<repo> --json headRefOid
```

Post a general PR review comment:

```bash
gh pr review <number> --repo <owner>/<repo> --comment --body "<review body>"
```

Request changes only when appropriate:

```bash
gh pr review <number> --repo <owner>/<repo> --request-changes --body "<review body>"
```

Approve only when the user explicitly asks and the review found no blocking issues:

```bash
gh pr review <number> --repo <owner>/<repo> --approve --body "<approval note>"
```

## Common Commands

```bash
gh pr view <url-or-number> --json number,title,body,headRefName,baseRefName,files,commits,url
gh pr diff <url-or-number>
gh pr checkout <number>
git diff --stat <base>...HEAD
rg -n "symbol_or_error" .
nl -ba path/to/file | sed -n 'start,endp'
```
