# AGENTS.md

## Purpose

This repository contains a C11 project.

Your job is to complete the requested task autonomously while preserving existing behaviour unless the user explicitly requests otherwise.

---

## General Rules

- Be proactive.
- Always add comments in the code to describe the logic and type of operation being done when making any changes to this repository.
- Prefer action over questions.
- Read code before making changes.
- Make reasonable assumptions based on the repository.
- Do not repeatedly search the same files.
- Do not repeat failed actions.
- Do not run build/compile/link checks/validation for basic code changes.
- Only run build/compile/link checks/validation unless specifically troubleshooting build/compile/link issues.

Only ask the user if:
- information is genuinely unavailable from the repository, or
- the change could destroy data or significantly alter behaviour.

---

## Investigation

When solving a problem:

1. Inspect the most likely file first.
2. If necessary, inspect one related file.
3. If the cause is identified, begin editing immediately.

Do not repeatedly search for the same symbol.

---

## Editing

When modifying code:

- Prefer small targeted edits.
- Match existing coding style.

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
- Prefer descriptive names
- Avoid unnecessary allocations
- Clean code
- Best performance
- Clean architecture
- Simplest solution
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