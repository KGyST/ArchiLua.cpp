@echo off
REM Generate compile_commands.json for clang-tidy static analysis
REM Delegates to PowerShell script for proper JSON generation.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0generate-compile-commands.ps1"
