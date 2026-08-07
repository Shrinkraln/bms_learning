---
name: bq76940-datasheet
description: >
  BQ76940 3-Series to 15-Series Cell Battery Monitor (AFE) datasheet skill. Provides register map, I2C commands, timing parameters, and electrical characteristics for AI-assisted embedded driver development. When the user mentions BQ76940, BQ76930, BQ76920, BQ769x0, battery monitor, battery AFE, cell monitor, TI BMS, coulomb counter, battery management in the context of embedded programming, driver development, register configuration, or hardware validation, use this skill. Even if the user does not explicitly name bq76940, but describes a device matching its capabilities, suggest this skill.
compatibility: requires_read_permission
---

# BQ76940 3-Series to 15-Series Cell Battery Monitor (AFE) Datasheet Skill

This skill provides the complete datasheet for **BQ76940 3-Series to 15-Series Cell Battery Monitor (AFE)** structured for AI-assisted embedded development.

## Three-Way Operation

When the user asks a question related to bq76940, determine which mode(s) apply:

### Mode 1: Reference Query

Triggered by questions like "what is register X?", "what does bit Y do?", "what is the I2C address?".

**Workflow:**
1. Identify the topic: register -> `references/registers.md`, command -> `references/instructions.md`, pin -> `references/pinout.md`, timing -> `references/timing.md`, electrical -> `references/electrical.md`, procedure -> `references/operations.md`
2. Read the relevant reference file(s)
3. Return the exact datasheet definition with page/table reference

### Mode 2: Code Generation

Triggered by requests like "write a driver for X", "generate init code", "implement cell voltage reading".

**Workflow:**
1. Read `references/instructions.md` for the I2C command table
2. Read `references/operations.md` for the canonical sequence
3. Read `references/registers.md` for register configurations
4. Generate code following the exact command sequences and register settings from the datasheet
5. Annotate generated code with datasheet references (table/register name)

### Mode 3: Parameter Validation

Triggered by questions like "can I run at X voltage?", "is this cell voltage safe?", "check this protection threshold config".

**Workflow:**
1. Identify the parameter category:
   - Voltage/current/power -> `references/electrical.md`
   - Timing/protection delays -> `references/timing.md`
2. Read the relevant reference file(s)
3. Compare the user's value against the datasheet limits
4. Return exactly one of:
   - **COMPLIANT** — value safely within datasheet limits
   - **BORDERLINE** — value meets but does not exceed the limit; marginal for production
   - **VIOLATION** — value exceeds datasheet limits; may cause malfunction or permanent damage

## Reference Files

Read reference files on demand — never load everything at once:

| File | When to Read |
|------|-------------|
| [references/pinout.md](references/pinout.md) | Pin function, package, GPIO assignment questions |
| [references/electrical.md](references/electrical.md) | Voltage, current, power constraint validation |
| [references/instructions.md](references/instructions.md) | I2C command/register address questions |
| [references/registers.md](references/registers.md) | Register bit-field definitions |
| [references/timing.md](references/timing.md) | Timing, protection delay validation |
| [references/operations.md](references/operations.md) | Init/boot/SHIP mode/cell balancing sequences |
