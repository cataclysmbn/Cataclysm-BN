"""Run the game with an isolated profile and prove it resolved to that profile."""

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def profile_arguments(profile):
    return [
        "--userdir", str(profile) + "/",
        "--configdir", str(profile / "config") + "/",
    ]


def verify_profile_paths(command, profile, evidence=None):
    result = subprocess.run(
        [*command, *profile_arguments(profile), "--paths"], cwd=ROOT, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, timeout=30,
    )
    expected_user = f"User Directory: {profile}/"
    expected_config = f"Config Directory: {profile / 'config'}/"
    if expected_user not in result.stdout or expected_config not in result.stdout:
        raise RuntimeError(
            "Profile isolation failed; expected "
            f"{expected_user!r} and {expected_config!r} in --paths"
        )
    if evidence is not None:
        evidence.write_text(result.stdout)
