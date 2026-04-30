# Pandoc Project Planner

Generate beautiful HTML project plans from a simple Markdown file.

## Files

| File | Purpose |
|---|---|
| `my-project.md` | Your project plan (edit this) |
| `plan-template.html` | Pandoc HTML template (the visual design) |
| `plan-filter.lua` | Lua filter (processes metadata) |
| `build.bat` | One-click build on Windows |
| `build.sh` | One-click build on Linux/macOS |

## Requirements

Install Pandoc from https://pandoc.org/installing.html

Verify it works:
```
pandoc --version
```

## Usage

### Windows
```
build.bat my-project.md
```

### Linux / macOS
```
./build.sh my-project.md
```

### Manual command
```
pandoc my-project.md --template plan-template.html --lua-filter plan-filter.lua --output my-project.html --standalone
```

## Writing your plan

Your Markdown file has two parts:

### 1. YAML front matter (between the --- lines)
This is your structured data: phases, milestones, risks, metadata.

```yaml
---
title: "My Project"
description: "Short description"
manager: "Jane Smith"
start: "2025-01-01"
end: "2025-12-31"
status: "Active"        # Planning | Active | On Hold | Completed
priority: "High"        # Low | Medium | High
budget: "$50,000"
objectives: "What success looks like."

phases:
  - name: "Phase 1"
    description: "What happens in this phase."
    status: "Completed"   # Not Started | In Progress | Completed | On Hold
    progress: 100

milestones:
  - name: "Go Live"
    date: "2025-06-01"
    done: false

risks:
  - title: "A risk"
    level: "High"         # Low | Medium | High
    mitigation: "How you handle it."
---
```

### 2. Free-form Markdown body
Anything below the --- is rendered as the "Details" section.
Use headings, bullet lists, tables — standard Markdown.

## Adding more plans

Just copy `my-project.md`, rename it, edit the content, and run the build script with the new filename:
```
build.bat another-project.md
```
