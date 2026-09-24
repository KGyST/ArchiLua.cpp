#!/usr/bin/env powershell
# githooks/lua-audit.ps1 — lexical forward-reference audit for Lua scripts.
#
# Lua binds locals at definition point: a function defined BEFORE a top-level
# `local` it references silently binds a nil global and crashes only at
# runtime (loadfile gives no warning). This has bitten try_dividers.lua five
# times (logEvent, resolvePart, lastRev, panelsDiverged, syncKey/lastSync).
#
# Usage (also wired into githooks/pre-commit, which fails the commit on hits):
#   powershell -NoProfile -ExecutionPolicy Bypass -File githooks/lua-audit.ps1 -File <path>
# Prints SUSPECT/MISSING-DECL lines, ends with AUDIT-DONE. Exit code is always
# 0 — callers grep the output.
param([string]$File)
$lines = Get-Content -LiteralPath $File
$topDefs = @{}
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^local (?:function )?(\w+)') {
        $n = $Matches[1]
        if (($n -ne 'function') -and (-not $topDefs.ContainsKey($n))) {
            $topDefs[$n] = $i + 1
        }
    }
}
foreach ($n in @($topDefs.Keys)) {
    $d = $topDefs[$n]
    $declPat = '^\s*local (?:function )?' + $n + '\b'
    for ($i = 0; $i -lt ($d - 1); $i++) {
        $code = $lines[$i] -replace '--.*$', ''
        if (($code -match "\b$n\b") -and ($code -notmatch $declPat)) {
            $txt = $lines[$i].Trim()
            if ($txt.Length -gt 70) { $txt = $txt.Substring(0, 70) }
            Write-Output "SUSPECT: $n used line $($i+1), declared $d : $txt"
            break
        }
    }
}
Write-Output "AUDIT-DONE"
