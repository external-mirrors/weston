# Agent Guidelines for the Weston Project

Follow [CONTRIBUTING.md](CONTRIBUTING.md) and [LLM-USAGE.md](LLM-USAGE.md)
before producing or submitting changes.

- Never add a Signed-off-by tag.
- Do not use automation to create, edit, close, or comment on issues, merge
  requests, or commits.
- Do not write documentation, commit messages, merge-request text, or code
  comments for the human unless they explicitly ask you to draft a rough version
  for review. The human must write the final commit message and any code
  comments they want to keep.
- Keep changes small, focused, and reviewable.
- Prefer the smallest relevant Weston validation for the area you touched, and
  report what you verified and what remains unverified.
- If a task is blocked or incomplete, say so clearly and tell the human what
  remains to be done.
- Before finalizing the work, explicitly check that the human has written the
  final commit message, any desired code comments, and the final summary of the
  work. Do not treat the task as complete until those human-authored elements
  are present and reviewed by the human.
- Verify any human-written text against the actual code before treating it as
  final. If the text does not match the implementation, tell the human and do
  not mark the task as complete.
- The human submitter is responsible for correctness, licensing, and final
  submission.

When proposing a fix or feature, prefer adding or updating tests that cover the
change.
