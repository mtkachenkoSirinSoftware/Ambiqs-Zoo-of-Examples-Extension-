#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build / verify the Ambiq KWS example-zoo tarball (phase 6).

Usage
-----
    python3 tools/pack_tarball.py              # write dist/ambiq_kws_examples.tar.gz
    python3 tools/pack_tarball.py --verify     # pack then extract-check
    python3 tools/pack_tarball.py --verify-only --archive dist/ambiq_kws_examples.tar.gz

The archive is a single top-level directory ``ambiq_kws_examples/``. It must
contain the three examples, ``kws_core``, one WAV, one ``.tflite``, host
Python, README, and LICENSE. It must not contain an AmbiqSuite SDK, a venv,
research trees, NSX clones, or build artifacts.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import os
import stat
import tarfile
from pathlib import Path

ZOO = Path(__file__).resolve().parents[1]
ARC_ROOT = "ambiq_kws_examples"
SHIPPING_SHA = "ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7"
CLIP_PCM_SHA = "341c766cf618051d7ed879fe4ac29d1e725c8f40ada914d73d431db5cc0ea677"
FIXED_MTIME = 1757462400  # 2026-09-10 00:00:00 UTC

EXCLUDE_DIR_NAMES = frozenset(
    {
        "build",
        "build-host",
        "modules",
        "boards",
        ".nsx",
        "__pycache__",
        ".git",
        "dist",
        ".venv",
        ".venv-hpx",
        ".venv-hpx-new",
        "venv",
    }
)
EXCLUDE_FILE_NAMES = frozenset(
    {
        "kws_model_manifest.json",
        ".DS_Store",
    }
)
# Path suffixes relative to the zoo root (posix).
EXCLUDE_PATH_SUFFIXES = frozenset(
    {
        "kws_clip/cmake/nsx",
        "kws_uart/cmake/nsx",
        "kws_pdm/cmake/nsx",
        "kws_uart/host/generated",
    }
)
FORBIDDEN_SUBSTR = (
    "AmbiqSuite",
    "demo_a/",
    "demo_b/",
    "apollo510_kws_demo",
    "optimizationExperiments",
    ".venv",
    "snr_mix",
    "/captures/",
)
REQUIRED = (
    "LICENSE",
    "README.md",
    "COVER.md",
    "PACKAGING.md",
    "EXAMPLES_CONTRACT.md",
    "bring_your_model.md",
    "Makefile",
    "assets/MODEL.txt",
    "assets/depgraph_r060_kd_int8.tflite",
    "tools/embed_model.py",
    "tools/embed_clip.py",
    "tools/pack_tarball.py",
    "kws_core/cmake/kws_core.cmake",
    "kws_clip/nsx.yml",
    "kws_clip/nsx.lock",
    "kws_clip/src/main.cc",
    "kws_clip/audio/generated/kws_clip_data.cc",
    "kws_clip/model/generated/kws_model_data.cc",
    "kws_clip/host/expected.json",
    "kws_uart/nsx.yml",
    "kws_uart/nsx.lock",
    "kws_uart/src/main.cc",
    "kws_uart/host/stream_wav.py",
    "kws_uart/host/synthetic.wav",
    "kws_uart/PROTOCOL.md",
    "kws_pdm/nsx.yml",
    "kws_pdm/nsx.lock",
    "kws_pdm/src/main.cc",
)


def sha256_file(path: Path) -> str:
    """Return hex SHA256 of ``path``.

    Parameters
    ----------
    path : Path
        File to hash.

    Returns
    -------
    str
        Lower-case hex digest.
    """
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()


def _is_excluded(rel: Path) -> bool:
    parts = rel.parts
    if any(p in EXCLUDE_DIR_NAMES for p in parts):
        return True
    if rel.name in EXCLUDE_FILE_NAMES:
        return True
    posix = rel.as_posix()
    if any(posix == s or posix.startswith(s + "/") for s in EXCLUDE_PATH_SUFFIXES):
        return True
    if posix.endswith(".pyc") or posix.endswith(".pyo"):
        return True
    return False


def iter_members(root: Path) -> list[Path]:
    """Sorted relative paths that belong in the tarball.

    Parameters
    ----------
    root : Path
        Zoo root.

    Returns
    -------
    list of Path
        Paths relative to ``root``.
    """
    out: list[Path] = []
    for dirpath, dirnames, filenames in os.walk(root):
        rel_dir = Path(dirpath).relative_to(root)
        dirnames[:] = sorted(
            d
            for d in dirnames
            if d not in EXCLUDE_DIR_NAMES
            and not _is_excluded(rel_dir / d if rel_dir.parts else Path(d))
        )
        for name in sorted(filenames):
            rel = rel_dir / name if rel_dir.parts else Path(name)
            if _is_excluded(rel):
                continue
            out.append(rel)
    return sorted(out)


def _file_mode(path: Path) -> int:
    mode = path.stat().st_mode
    if mode & stat.S_IXUSR or path.read_bytes()[:2] == b"#!":
        return 0o755
    return 0o644


def pack(root: Path, archive: Path, mtime: int = FIXED_MTIME) -> dict[str, object]:
    """Write a deterministic gzip tarball.

    Parameters
    ----------
    root : Path
        Zoo root.
    archive : Path
        Output ``.tar.gz``.
    mtime : int
        Unix mtime stored on every member.

    Returns
    -------
    dict
        ``n_files``, ``sha256``, ``archive``.
    """
    members = iter_members(root)
    missing = [r for r in REQUIRED if not (root / r).is_file()]
    if missing:
        raise SystemExit("pack: missing required files:\n  " + "\n  ".join(missing))
    tflite_sha = sha256_file(root / "assets/depgraph_r060_kd_int8.tflite")
    if tflite_sha != SHIPPING_SHA:
        raise SystemExit(
            f"pack: shipping .tflite SHA {tflite_sha} != {SHIPPING_SHA}"
        )

    archive.parent.mkdir(parents=True, exist_ok=True)
    manifest_lines = [
        f"shipping_tflite_sha256={SHIPPING_SHA}",
        f"clip_pcm_sha256={CLIP_PCM_SHA}",
        f"n_files={len(members) + 1}",
        "",
    ]
    for rel in members:
        manifest_lines.append(rel.as_posix())
    manifest_blob = ("\n".join(manifest_lines) + "\n").encode()

    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.PAX_FORMAT) as tar:
        for rel in members:
            src = root / rel
            info = tarfile.TarInfo(f"{ARC_ROOT}/{rel.as_posix()}")
            info.size = src.stat().st_size
            info.mtime = mtime
            info.uid = 0
            info.gid = 0
            info.uname = "root"
            info.gname = "root"
            info.mode = _file_mode(src)
            with src.open("rb") as fh:
                tar.addfile(info, fh)
        minfo = tarfile.TarInfo(f"{ARC_ROOT}/PACK_MANIFEST.txt")
        minfo.size = len(manifest_blob)
        minfo.mtime = mtime
        minfo.uid = 0
        minfo.gid = 0
        minfo.uname = "root"
        minfo.gname = "root"
        minfo.mode = 0o644
        tar.addfile(minfo, io.BytesIO(manifest_blob))

    raw = buf.getvalue()
    out_buf = io.BytesIO()
    with gzip.GzipFile(fileobj=out_buf, mode="wb", mtime=mtime, compresslevel=9) as gzf:
        gzf.write(raw)
    archive.write_bytes(out_buf.getvalue())
    digest = sha256_file(archive)
    (archive.parent / (archive.name + ".sha256")).write_text(
        f"{digest}  {archive.name}\n"
    )
    return {"n_files": len(members) + 1, "sha256": digest, "archive": str(archive)}


def verify_archive(archive: Path) -> None:
    """Fail if the tarball contains forbidden paths or misses required files.

    Parameters
    ----------
    archive : Path
        ``.tar.gz`` to inspect.

    Raises
    ------
    SystemExit
        On any inclusion / exclusion / identity failure.
    """
    if not archive.is_file():
        raise SystemExit(f"verify: missing {archive}")
    names: list[str] = []
    with tarfile.open(archive, "r:gz") as tar:
        for m in tar.getmembers():
            if not m.name.startswith(ARC_ROOT + "/"):
                raise SystemExit(f"verify: member not under {ARC_ROOT}/: {m.name}")
            if m.name.endswith("/") or not m.isfile():
                continue
            rel = m.name[len(ARC_ROOT) + 1 :]
            names.append(rel)
            for bad in FORBIDDEN_SUBSTR:
                if bad in m.name:
                    raise SystemExit(f"verify: forbidden substring {bad!r} in {m.name}")
            parts = Path(rel).parts
            if any(p in EXCLUDE_DIR_NAMES for p in parts):
                raise SystemExit(f"verify: excluded directory in {m.name}")
            if Path(rel).name in EXCLUDE_FILE_NAMES:
                raise SystemExit(f"verify: excluded file {m.name}")
        tflite = tar.extractfile(f"{ARC_ROOT}/assets/depgraph_r060_kd_int8.tflite")
        if tflite is None:
            raise SystemExit("verify: shipping .tflite missing")
        got = hashlib.sha256(tflite.read()).hexdigest()
        if got != SHIPPING_SHA:
            raise SystemExit(f"verify: tflite SHA {got} != {SHIPPING_SHA}")
        wav_m = tar.getmember(f"{ARC_ROOT}/kws_uart/host/synthetic.wav")
        wav = tar.extractfile(wav_m)
        assert wav is not None
        wav_bytes = wav.read()
        if len(wav_bytes) < 44:
            raise SystemExit("verify: synthetic.wav too small")
    missing = [r for r in REQUIRED if r not in names]
    if missing:
        raise SystemExit("verify: missing:\n  " + "\n  ".join(missing))
    leak = [n for n in names if any(x in n.lower() for x in ("ambiqsuite", ".venv"))]
    if leak:
        raise SystemExit("verify: leak:\n  " + "\n  ".join(leak))


def main(argv: list[str] | None = None) -> int:
    """CLI entry.

    Parameters
    ----------
    argv : list of str, optional
        Arguments excluding the program name.

    Returns
    -------
    int
        Process exit status.
    """
    p = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0])
    p.add_argument(
        "--archive",
        type=Path,
        default=ZOO / "dist" / "ambiq_kws_examples.tar.gz",
        help="Output (or existing) archive path",
    )
    p.add_argument("--verify", action="store_true", help="Pack, then verify")
    p.add_argument(
        "--verify-only",
        action="store_true",
        help="Verify --archive without packing",
    )
    args = p.parse_args(argv)
    if args.verify_only:
        verify_archive(args.archive)
        print(f"verify ok  {args.archive}")
        return 0
    info = pack(ZOO, args.archive)
    print(
        f"wrote {info['archive']}  files={info['n_files']}  sha256={info['sha256']}"
    )
    if args.verify:
        verify_archive(args.archive)
        print("verify ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
