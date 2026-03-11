#!/bin/bash
# Usage: ./build.sh my-project.md
# Output: my-project.html

INPUT="${1:-my-project.md}"
OUTPUT="${INPUT%.md}.html"

pandoc "$INPUT" \
  --template plan-template.html \
  --lua-filter plan-filter.lua \
  --output "$OUTPUT" \
  --standalone

echo "Done! Output: $OUTPUT"
