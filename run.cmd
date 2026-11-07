:<<"::BATCH"
@echo off
rem Windows runs this cmd part; macOS and Linux run the sh part below.
set "config=%~1"
if not defined config set "config=debug"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_win.ps1" %config% || exit /b 1
"%~dp0build\windows-%config%\pontifice.exe"
exit /b %errorlevel%
::BATCH
set -eu
cd "$(dirname "$0")"

config="$(printf '%s' "${1:-debug}" | tr '[:upper:]' '[:lower:]')"
case "$(uname -s)" in
  Darwin) ./build_mac.sh "$config"; preset="macos-$config" ;;
  Linux) ./build_lin.sh "$config"; preset="linux-$config" ;;
  *) echo "Unsupported system: $(uname -s)" >&2; exit 1 ;;
esac
exec "./build/$preset/pontifice"
