"""SKIPBACK: unlisted capture-core draft; no native recorder adapter yet.

No stock sites or page-map addresses are guessed. Portable tests and a
freestanding ColdFire link do not qualify live recording or USB installation.
"""
from remix.schema import Category, Proof, Gate, Kind, Module

MODULE = Module(
    name="skipback", key="SKIPBACK", kind=Kind.CF_PATCH,
    doc="Draft: one-minute retrospective MAIN capture; native integration pending.",
    category=Category.FIXES, author="fominok", author_url="https://github.com/fominok",
    proof=Proof.UNTESTED,
    proof_note="Portable capture core only; no installed firmware or hardware evidence.",
    gates=(Gate("modules/skipback/verify.py", remix_arg=False),),
)
