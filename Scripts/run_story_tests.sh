#!/bin/bash
# Headless story tests (no GPU / audio needed).
#   Scripts/run_story_tests.sh [Filter] [LogFile]
# Filter defaults to IronManArena.Story (Data + Chapter.1..9 + FullRun).
# Needs a built editor (IronManArenaEditor Development). UE_ROOT defaults to /home/box/UnrealEngine/5.8.
UE_ROOT=${UE_ROOT:-/home/box/UnrealEngine/5.8}
PROJECT=${PROJECT:-$(cd "$(dirname "$0")/.." && pwd)/IronManArena.uproject}
FILTER=${1:-IronManArena.Story}
LOG=${2:-$(dirname "$PROJECT")/Saved/Logs/StoryTests.log}
"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" "$PROJECT" -game -nullrhi -nosound -unattended -nopause -nosplash \
  -ExecCmds="Automation RunTests $FILTER" -TestExit="Automation Test Queue Empty" -abslog="$LOG" -stdout > /dev/null 2>&1
# (-TestExit always quits with code 1, so the verdict comes from the log.)
grep -E "Test Completed|StoryTest: (RESULT|ch[0-9]|prologue|FAIL)|Error:" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//'
OK=$(grep -c "Test Completed. Result={Success}" "$LOG")
ALL=$(grep -c "Test Completed. Result=" "$LOG")
echo "tests passed: $OK / $ALL   (full log: $LOG)"
[ "$ALL" -gt 0 ] && [ "$OK" = "$ALL" ]
