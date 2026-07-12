# AGENTS.md

## Purpose

This repository contains a C11 project.

Your job is to complete the requested task autonomously while preserving existing behaviour unless the user explicitly requests otherwise.

---

## General Rules

- Be proactive.
- Prefer action over questions.
- Read code before making changes.
- Make reasonable assumptions based on the repository.
- Do not repeatedly search the same files.
- Do not repeat failed actions.

Only ask the user if:
- information is genuinely unavailable from the repository, or
- the change could destroy data or significantly alter behaviour.

---

## Investigation

When solving a problem:

1. Inspect the most likely file first.
2. If necessary, inspect one related file.
3. If the cause is identified, begin editing immediately.
4. Avoid broad searches across the repository unless requested.

Do not repeatedly search for the same symbol.

---

## Editing

When modifying code:

- Prefer small targeted edits.
- Match existing coding style.
- Preserve behaviour unless fixing a bug.
- Do not rewrite unrelated code.

If an edit fails because the file changed:

- Re-read the file.
- Retry once.
- If it still fails, rewrite the affected function instead of looping.

Never retry the same failed patch more than once.

---

## Build System

This project is written in C11.

Never use:
- npm
- yarn
- pnpm
- bun

Before building:

1. Read `.vscode/tasks.json`.
2. Use the defined VS Code build task.

Do not invent build commands.

---

## Project Structure

Look in:

- `.vscode/` for build tasks
- `include/` for headers
- `src/` for implementations

---

## Coding Style

- C11
- Standard library unless project already uses another library
- Keep functions small
- Prefer descriptive names
- Avoid unnecessary allocations

---

## Tool Usage

Use tools instead of asking questions.

Read files before editing them.

After making code changes:

1. Build.
2. Fix compile errors.
3. Stop when the build succeeds.

Do not repeatedly attempt the same command.

---

## Behaviour

Do not narrate every step.

Do not explain your plan unless asked.

Focus on completing the task.

If multiple solutions are possible, choose the simplest correct implementation.