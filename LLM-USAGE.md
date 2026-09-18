# LLM-generated contributions guide

This document defines how artificial intelligence (AI) and machine-assisted
tools may be used when contributing to Weston. It applies to all code and
documentation under the Weston project.

AI tools can help with productivity, but they cannot replace understanding.
The scope of this document is to preserve code quality, maintain license
compliance, and ensure that human accountability remains central.

Contributors shall follow the [CONTRIBUTING](CONTRIBUTING.md)
guide regardless of whether they use AI tools or not.

## Upstream interaction rules

AI agents must not use any API, CLI, or web UI automation to:

- create, edit, or close issues ("work items"),
- post comments on merge requests, issues, or commits, nor
- open or update merge requests.

Prose written by an AI agent must not be forwarded upstream unless it is
concise, and fully reviewed and copyedited by a human first.

## Signed-off-by and Developer Certificate of Origin

AI agents MUST NOT add Signed-off-by tags.

Only humans can legally certify the [Developer Certificate of
Origin](DCO-1.1.txt). The human submitter is responsible for:

- reviewing all AI-generated materials,
- ensuring compliance with licensing requirements, and
- adding their own Signed-off-by tag to certify the DCO.

## Responsibility

The human contributor is solely responsible for all code committed to Weston
repositories. AI-generated code and comments are treated as human-written code
and must be correct, concise, readable, and maintainable. "AI suggested it"
is not an acceptable justification.

The human contributor must fully understand and validate the logic, side
effects, and security implications of any AI-suggested code before
submission.

## Reviewing and validation

All AI-assisted code must undergo the standard Weston peer-review process.

When fixing bugs or adding new features, the agent or contributor should
add or update tests to cover the change.
