"""Fail publication until actual native integration has been qualified."""
raise SystemExit(
    "[FAIL] Skipback is a development draft, not installable firmware. "
    "Run test_core.py for synthetic PCM tests. Recorder ownership, MAIN source, "
    "project menu/settings, AED publication, native/browser parity and USB "
    "lifecycle are pending. No emulator or hardware acceptance is claimed."
)
