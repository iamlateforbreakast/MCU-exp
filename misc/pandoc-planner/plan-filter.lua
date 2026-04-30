-- plan-filter.lua
-- Enriches YAML metadata for the plan-template.html

local function slugify(s)
  return (s or ""):lower():gsub("%s+", "-"):gsub("[^%a%d%-]", "")
end

local function today()
  return os.date("%Y-%m-%d")
end

local function str(v)
  if v == nil then return "" end
  return pandoc.utils.stringify(v)
end

-- Spine colours matched to status / progress
local function spine_color(status, progress)
  local s = slugify(status)
  local p = tonumber(progress) or 0
  if s == "completed" or p == 100 then return "var(--green)" end
  if s == "in-progress" then return "var(--warn)" end
  if s == "not-started" then return "var(--ink3)" end
  if s == "on-hold" then return "var(--red)" end
  return "var(--accent)"
end

-- Badge class matched to risk / effort label
local function badge_class(badge)
  local b = (badge or ""):lower()
  if b:find("lead only") or b:find("high") then return "pb-high" end
  if b:find("med") or b:find("supervised") then return "pb-med" end
  if b:find("low") or b:find("intern") then return "pb-low" end
  return "pb-prep"
end

-- Note box class
local function note_class(note_type)
  local t = (note_type or "rule"):lower()
  if t == "warn" or t == "warning" then return "note-warn" end
  if t == "ok"   or t == "good"    then return "note-ok"   end
  return "note-rule"
end

function Meta(meta)

  -- Default generated date
  if not meta.generated then
    meta.generated = pandoc.MetaInlines{pandoc.Str(today())}
  end

  -- Summary counts
  local phases     = meta.phases     or pandoc.MetaList{}
  local milestones = meta.milestones or pandoc.MetaList{}
  local risks      = meta.risks      or pandoc.MetaList{}

  meta["phase-count"]     = pandoc.MetaInlines{pandoc.Str(tostring(#phases))}
  meta["milestone-count"] = pandoc.MetaInlines{pandoc.Str(tostring(#milestones))}
  meta["risk-count"]      = pandoc.MetaInlines{pandoc.Str(tostring(#risks))}

  -- ── Enrich phases ────────────────────────────────────────────────────
  for i, phase in ipairs(phases) do
    local status   = str(phase.status)
    local progress = str(phase.progress)
    local badge    = str(phase.badge)

    -- Phase number (1-based)
    phase["number"] = pandoc.MetaInlines{pandoc.Str(tostring(i))}

    -- Spine colour
    phase["spine-color"] = pandoc.MetaInlines{
      pandoc.Str(spine_color(status, progress))
    }

    -- Badge class (pb-prep / pb-low / pb-med / pb-high)
    if badge ~= "" then
      phase["badge-class"] = pandoc.MetaInlines{
        pandoc.Str(badge_class(badge))
      }
    end

    -- Note class
    local note_type = str(phase["note-type"])
    if note_type ~= "" then
      phase["note-class"] = pandoc.MetaInlines{
        pandoc.Str(note_class(note_type))
      }
    else
      phase["note-class"] = pandoc.MetaInlines{pandoc.Str("note-rule")}
    end

    phases[i] = phase
  end
  meta.phases = phases

  -- ── Enrich milestones ────────────────────────────────────────────────
  local td = today()
  for i, m in ipairs(milestones) do
    local done = str(m.done)
    local date = str(m.date)

    if done == "true" then
      m["done-class"] = pandoc.MetaInlines{pandoc.Str("done")}
      m["icon"]       = pandoc.MetaInlines{pandoc.Str("&#10003;")}
    elseif date ~= "" and date < td then
      m["done-class"] = pandoc.MetaInlines{pandoc.Str("overdue")}
      m["icon"]       = pandoc.MetaInlines{pandoc.Str("!")}
    else
      m["done-class"] = pandoc.MetaInlines{pandoc.Str("")}
      m["icon"]       = pandoc.MetaInlines{pandoc.Str("&#9711;")}
    end
    milestones[i] = m
  end
  meta.milestones = milestones

  -- ── Enrich risks ─────────────────────────────────────────────────────
  for i, risk in ipairs(risks) do
    local lvl = str(risk.level)
    risk["level-class"] = pandoc.MetaInlines{pandoc.Str(slugify(lvl))}
    risks[i] = risk
  end
  meta.risks = risks

  return meta
end
