#!/usr/bin/env python3
"""Description.xml dependency helper for Foundation builds.

Commands:
  validate          Validate Description.xml against Description.xsd
  resolve           Print shell assignments for the selected variant
  list-components   List components for a build_target
  fetch             Fetch/replace components managed by Description (https/git/fs/…)
  check             Validate and recurse check into component Description.xml
  clean             Remove fetched component trees (https/git/fs/…) under <path>

fetch always refreshes: existing dest trees/files are removed then re-fetched.
Use Build.bat gen (not fetch) when dependencies must stay untouched.

Examples:
  python tools/Dependencies.py -d Description.xml validate
  python tools/Dependencies.py -d Description.xml resolve --shell bat
  python tools/Dependencies.py -d Description.xml fetch
"""

from __future__ import annotations

import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Iterable, Optional
from urllib.parse import urlparse


SOURCE_TYPES_FETCHABLE = frozenset(
    {"https", "git", "git-module", "git-submodules", "fs", "nuget"}
)


def _die(msg: str, code: int = 1) -> None:
    print(f"[ERROR] {msg}", file=sys.stderr)
    raise SystemExit(code)


def _info(msg: str) -> None:
    print(f"[INFO] {msg}", file=sys.stderr)


def _warn(msg: str) -> None:
    print(f"[WARN] {msg}", file=sys.stderr)


def normalize_path(path: str | Path) -> str:
    return os.path.normcase(os.path.normpath(os.path.abspath(str(path))))


def load_tree(description: Path) -> ET.ElementTree:
    if not description.is_file():
        _die(f"Description not found: {description}")
    try:
        return ET.parse(description)
    except ET.ParseError as exc:
        _die(f"XML parse error in {description}: {exc}")


def text_of(parent: ET.Element, tag: str, default: str = "") -> str:
    child = parent.find(tag)
    if child is None or child.text is None:
        return default
    return child.text.strip()


def validate(description: Path, schema: Optional[Path] = None) -> None:
    schema_path = schema or description.with_name("Description.xsd")
    if not schema_path.is_file():
        _die(f"XSD not found: {schema_path}")
    try:
        import xmlschema
    except ImportError:
        _die(
            "Package 'xmlschema' is required. "
            "Install with: pip install -r tools/requirements.txt"
        )
    xs = xmlschema.XMLSchema(str(schema_path))
    errors = list(xs.iter_errors(str(description)))
    if errors:
        for err in errors:
            print(f"[XSD] {err}", file=sys.stderr)
        _die(f"Validation failed for {description} ({len(errors)} error(s))")
    _info(f"Validated {description} against {schema_path.name}")


def iter_variants(root: ET.Element) -> list[ET.Element]:
    variants = root.find("variants")
    if variants is None:
        return []
    return list(variants.findall("variant"))


def variant_targets(variant: ET.Element) -> list[str]:
    return [t.text.strip() for t in variant.findall("build_target") if t.text]


def resolve_variant(
    root: ET.Element, cwd: Path, build_target_override: Optional[str] = None
) -> Optional[ET.Element]:
    """Resolve a <variant>, or None when the Description has no variants.

    Nested library Description.xml files often list only <components>
    (e.g. libusb) and inherit BUILD_TARGET from the parent fetch.
    """
    variants = iter_variants(root)
    if not variants:
        return None

    env_target = build_target_override or os.environ.get("BUILD_TARGET", "").strip()
    if env_target:
        for variant in variants:
            if env_target in variant_targets(variant):
                return variant
        _die(f"No variant matches BUILD_TARGET={env_target}")

    cwd_norm = normalize_path(cwd)
    for variant in variants:
        build_path = text_of(variant, "build_path")
        if not build_path:
            continue
        bp = normalize_path(build_path)
        if cwd_norm == bp or cwd_norm.startswith(bp + os.sep):
            return variant
        # Also match if build_path is under cwd (clone root vs nested)
        if bp.startswith(cwd_norm + os.sep):
            return variant

    _die(
        "No variant resolved: set BUILD_TARGET or add a <build_path> matching "
        f"this project ({cwd_norm})"
    )


def resolve_build_target(
    root: ET.Element,
    cwd: Path,
    build_target_override: Optional[str] = None,
) -> str:
    """Pick BUILD_TARGET from override/env, else from a resolved variant."""
    override = (build_target_override or os.environ.get("BUILD_TARGET", "")).strip()
    variant = resolve_variant(root, cwd, build_target_override)
    if variant is None:
        if not override:
            _die(
                "No <variant> entries and BUILD_TARGET unset "
                f"(Description under {cwd})"
            )
        return override
    return override or variant_targets(variant)[0]


def emit_resolve(variant: ET.Element, shell: str) -> None:
    name = text_of(variant, "name")
    vid = text_of(variant, "id")
    env = text_of(variant, "env")
    env_version = text_of(variant, "env_version")
    build_path = text_of(variant, "build_path")
    targets = variant_targets(variant)
    build_target = targets[0] if targets else ""

    pairs = [
        ("BUILD_TARGET", build_target),
        ("VARIANT_NAME", name),
        ("VARIANT_ID", vid),
        ("VARIANT_ENV", env),
        ("ENV_VERSION", env_version),
        ("VARIANT_BUILD_PATH", build_path),
    ]
    if shell == "bat":
        for key, value in pairs:
            # Escape for cmd SET
            safe = value.replace("%", "%%")
            print(f"SET {key}={safe}")
    else:
        for key, value in pairs:
            safe = value.replace("'", "'\"'\"'")
            print(f"export {key}='{safe}'")


def iter_components(root: ET.Element) -> list[ET.Element]:
    comps = root.find("components")
    if comps is None:
        return []
    return list(comps.findall("component"))


def component_targets(comp: ET.Element) -> list[str]:
    return [t.text.strip() for t in comp.findall("build_target") if t.text]


def component_applies(comp: ET.Element, build_target: str) -> bool:
    targets = component_targets(comp)
    return not targets or build_target in targets


def component_sources(comp: ET.Element) -> list[tuple[str, str, str]]:
    """Return list of (type, uri, artifact_attr) — artifact_attr may be empty."""
    out: list[tuple[str, str, str]] = []
    for src in comp.findall("source"):
        stype = (src.get("type") or "").strip()
        uri = (src.text or "").strip()
        artifact = (src.get("artifact") or "").strip()
        if stype:
            out.append((stype, uri, artifact))
    return out


ARCHIVE_EXTENSIONS = (".7z", ".zip", ".tar", ".tar.gz", ".tgz")


def resolve_artifact(stype: str, uri: str, explicit: str = "") -> str:
    """
    Decide artifact form: archive | tree | file.
    Explicit source/@artifact wins; otherwise infer from type / URL extension.
    """
    if explicit in {"archive", "tree", "file"}:
        return explicit
    if stype == "https":
        name = Path(urlparse(uri).path).name.lower()
        for ext in ARCHIVE_EXTENSIONS:
            if name.endswith(ext):
                return "archive"
        return "file"
    if stype in {"git", "git-module", "git-submodules", "fs"}:
        return "tree"
    return "file"


def is_optional(comp: ET.Element) -> bool:
    """True if the component is not required for a successful build."""
    val = (comp.get("optional") or "").strip().lower()
    return val in {"1", "true", "yes"}


def needs_compile(comp: ET.Element) -> bool:
    """True if the component has a <build> element (sources to compile)."""
    return comp.find("build") is not None


def is_precompiled(comp: ET.Element) -> bool:
    """No <build> element: binary/prebuilt package (extract + select only)."""
    return not needs_compile(comp)


def list_components(root: ET.Element, build_target: str) -> None:
    for comp in iter_components(root):
        if not component_applies(comp, build_target):
            continue
        name = text_of(comp, "name")
        version = text_of(comp, "version")
        path = text_of(comp, "path")
        sources = ",".join(
            f"{t}:{resolve_artifact(t, u, a)}:{u}" if u else f"{t}:{resolve_artifact(t, u, a)}"
            for t, u, a in component_sources(comp)
        )
        flags = []
        if is_optional(comp):
            flags.append("optional")
        flags.append("build" if needs_compile(comp) else "precompiled")
        suffix = f"\t{','.join(flags)}" if flags else ""
        print(f"{name}\t{version}\t{path}\t{sources}{suffix}")


def project_root_from_description(description: Path) -> Path:
    return description.resolve().parent


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def which(cmd: str) -> Optional[str]:
    return shutil.which(cmd)


def download_file(url: str, dest: Path) -> None:
    ensure_dir(dest.parent)
    _info(f"Downloading {url} -> {dest}")
    try:
        req = urllib.request.Request(
            url, headers={"User-Agent": "Foundation-Dependencies/1.0"}
        )
        proxy = os.environ.get("HTTPS_PROXY") or os.environ.get("HTTP_PROXY")
        handlers = []
        if proxy:
            handlers.append(
                urllib.request.ProxyHandler({"http": proxy, "https": proxy})
            )
        opener = urllib.request.build_opener(*handlers)
        with opener.open(req) as resp, open(dest, "wb") as out:
            shutil.copyfileobj(resp, out)
        return
    except Exception as exc:  # noqa: BLE001
        if os.name != "nt":
            raise
        _warn(f"urllib download failed ({exc}); retrying via PowerShell")

    # Corporate SSL/proxy
    ps = (
        "$ProgressPreference='SilentlyContinue'; "
        "$p=@{}; "
        "if ($env:HTTPS_PROXY) { $p['Proxy']=$env:HTTPS_PROXY } "
        "elseif ($env:HTTP_PROXY) { $p['Proxy']=$env:HTTP_PROXY }; "
        f"Invoke-WebRequest -Uri '{url}' -OutFile '{dest}' @p -UseBasicParsing"
    )
    subprocess.run(
        [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-Command",
            ps,
        ],
        check=True,
    )


def extract_archive(archive: Path, dest: Path) -> None:
    ensure_dir(dest)
    suffix = "".join(archive.suffixes).lower()
    if suffix.endswith(".7z"):
        sevenz = which("7z") or which("7z.exe")
        if not sevenz:
            for candidate in (
                Path(os.environ.get("ProgramFiles", "")) / "7-Zip" / "7z.exe",
                Path(os.environ.get("ProgramFiles(x86)", "")) / "7-Zip" / "7z.exe",
            ):
                if candidate.is_file():
                    sevenz = str(candidate)
                    break
        if not sevenz:
            _die("7-Zip (7z) required to extract .7z archives")
        subprocess.run([sevenz, "x", str(archive), f"-o{dest}", "-y"], check=True)
        return
    if suffix.endswith(".zip") or archive.suffix == ".zip":
        shutil.unpack_archive(str(archive), str(dest))
        return
    if suffix.endswith(".tar.gz") or suffix.endswith(".tgz") or suffix.endswith(".tar"):
        shutil.unpack_archive(str(archive), str(dest))
        return
    _die(f"Unsupported archive format: {archive.name}")


def path_nonempty(dest: Path) -> bool:
    return dest.is_dir() and any(dest.iterdir())


def fetch_https(uri: str, dest: Path, artifact: str) -> None:
    if artifact == "tree":
        _die(f"https source cannot use artifact=tree ({uri})")
    if artifact not in {"archive", "file"}:
        _die(f"Unsupported https artifact={artifact!r} for {uri}")

    if artifact == "archive":
        ensure_dir(dest.parent)
        if dest.exists():
            _remove_fetched_dest(dest)
        parsed = urlparse(uri)
        name = Path(parsed.path).name or "download.bin"
        with tempfile.TemporaryDirectory(prefix="foundation-fetch-") as tmp:
            archive = Path(tmp) / name
            download_file(uri, archive)
            extract_archive(archive, dest)
        if not path_nonempty(dest):
            _die(f"Fetch incomplete: empty extract at {dest}")
        return

    # artifact == file: download without extraction
    ensure_dir(dest.parent)
    if dest.exists():
        _remove_fetched_dest(dest)
    download_file(uri, dest)
    if not dest.is_file():
        _die(f"Fetch incomplete: expected file at {dest}")


def fetch_git(uri: str, dest: Path, version: str) -> None:
    if dest.exists():
        _remove_fetched_dest(dest)
    ensure_dir(dest.parent)

    refs: list[Optional[str]] = []
    if version:
        refs.append(version)
        # Semver in Description often omits the leading 'v' used by tags
        if version[0].isdigit():
            refs.append(f"v{version}")
    refs.append(None)  # default branch

    last_err: Optional[BaseException] = None
    for ref in refs:
        cmd = ["git", "clone", "--depth", "1"]
        if ref:
            cmd += ["--branch", ref]
        cmd += [uri, str(dest)]
        _info(" ".join(cmd))
        try:
            subprocess.run(cmd, check=True)
            return
        except Exception as exc:  # noqa: BLE001
            last_err = exc
            if dest.exists():
                shutil.rmtree(dest)
            _warn(f"git clone failed for ref={ref!r}: {exc}")
    _die(f"git clone failed for {uri}: {last_err}")


def fetch_git_submodules(dest: Path) -> None:
    if not dest.is_dir():
        _die(f"git-submodules path missing: {dest}")
    _info(f"git submodule update --init --recursive ({dest})")
    subprocess.run(
        ["git", "submodule", "update", "--init", "--recursive"],
        cwd=str(dest),
        check=True,
    )


_FS_COPY_IGNORE = {"build", "dist", ".git"}


def _is_link_or_junction(path: Path) -> bool:
    try:
        if path.is_symlink():
            return True
    except OSError:
        pass
    is_j = getattr(path, "is_junction", None)
    if callable(is_j):
        try:
            if is_j():
                return True
        except OSError:
            pass
    # Windows directory junctions: Python < 3.12 has no Path.is_junction;
    # Path.is_symlink() is often False. Detect the reparse point instead.
    if os.name == "nt":
        try:
            import ctypes

            get_attrs = ctypes.windll.kernel32.GetFileAttributesW
            get_attrs.argtypes = [ctypes.c_wchar_p]
            get_attrs.restype = ctypes.c_uint32
            invalid = 0xFFFFFFFF
            reparse = 0x400
            attrs = get_attrs(str(path))
            if attrs != invalid and (attrs & reparse):
                return True
        except OSError:
            pass
    return False


def _rmtree_onexc(func, path, exc_info):
    """Clear read-only bits (git pack files on Windows) then retry."""
    if not os.access(path, os.W_OK):
        os.chmod(path, stat.S_IWRITE)
        func(path)
        return
    raise exc_info[1]


def _remove_fetched_dest(dest: Path) -> None:
    """Remove dest. Never rmtree through a junction/symlink (would wipe the sibling)."""
    if _is_link_or_junction(dest):
        _info(f"Removing link/junction (not recursive): {dest}")
        try:
            dest.unlink()
        except OSError:
            dest.rmdir()
        return
    if dest.is_dir():
        shutil.rmtree(dest, onexc=_rmtree_onexc)
    elif dest.exists():
        dest.unlink()


def fetch_fs(uri: str, dest: Path, root: Path) -> None:
    src = Path(uri)
    if not src.is_absolute():
        src = (root / src).resolve()
    if not src.exists():
        _die(f"fs source not found: {src}")
    if normalize_path(src) == normalize_path(dest):
        _info(f"fs source already at destination: {dest}")
        return
    if dest.exists():
        _remove_fetched_dest(dest)
    ensure_dir(dest.parent)
    if src.is_dir():
        _info(f"Copy fs tree {src} -> {dest} (ignore {_FS_COPY_IGNORE})")
        shutil.copytree(
            src,
            dest,
            ignore=lambda _dir, names: [n for n in names if n in _FS_COPY_IGNORE],
        )
    else:
        shutil.copy2(src, dest)


def fetch_nuget(_uri: str, _dest: Path) -> None:
    _warn("source type 'nuget' is not implemented yet; skipping")


def fetch_component(
    comp: ET.Element, root: Path, recurse: bool, build_target: str
) -> None:
    name = text_of(comp, "name")
    if is_optional(comp):
        _info(f"Skip optional component: {name}")
        return

    path = text_of(comp, "path")
    if not path:
        _warn(f"Component {name} has no <path>; skip")
        return
    dest = (root / path).resolve()
    version = text_of(comp, "version")
    sources = component_sources(comp)
    if not sources:
        _warn(f"Component {name} has no <source>; skip")
        return

    last_err: Optional[BaseException] = None
    for stype, uri, art_attr in sources:
        if stype not in SOURCE_TYPES_FETCHABLE:
            _warn(f"Unknown source type '{stype}' for {name}")
            continue
        artifact = resolve_artifact(stype, uri, art_attr)
        try:
            if stype == "https":
                if not uri:
                    continue
                fetch_https(uri, dest, artifact)
            elif stype == "git":
                if not uri:
                    continue
                if artifact != "tree":
                    _die(f"git source requires artifact=tree (got {artifact})")
                fetch_git(uri, dest, version)
            elif stype in {"git-module", "git-submodules"}:
                if artifact != "tree":
                    _die(f"{stype} source requires artifact=tree (got {artifact})")
                fetch_git_submodules(dest if dest.is_dir() else root)
            elif stype == "fs":
                if artifact != "tree":
                    _die(f"fs source requires artifact=tree (got {artifact})")
                fetch_fs(uri or path, dest, root)
            elif stype == "nuget":
                fetch_nuget(uri, dest)
            else:
                continue
            last_err = None
            break
        except Exception as exc:  # noqa: BLE001 — try next source
            last_err = exc
            _warn(f"{name} via {stype} failed: {exc}")
    if last_err is not None:
        _die(f"Failed to fetch component {name}: {last_err}")

    nested = dest / "Description.xml"
    if recurse and nested.is_file():
        _info(f"Recurse fetch into {nested}")
        run_command(
            ["fetch"],
            description=nested,
            recurse=True,
            build_target=os.environ.get("BUILD_TARGET") or build_target,
        )


def clean_component(comp: ET.Element, root: Path) -> None:
    name = text_of(comp, "name")
    if is_optional(comp):
        return
    sources = component_sources(comp)
    if not any(t in SOURCE_TYPES_FETCHABLE for t, _u, _a in sources):
        _info(f"Skip clean for non-fetchable component: {name}")
        return
    path = text_of(comp, "path")
    if not path:
        return
    dest = (root / path).resolve()
    if dest.exists() or _is_link_or_junction(dest):
        _info(f"Removing {dest}")
        _remove_fetched_dest(dest)
    # Also remove sibling download artifacts (e.g. name.7z next to extract dir)
    parent = dest.parent
    if parent.is_dir():
        for archive in parent.glob(f"{dest.name}.*"):
            if archive.is_file():
                _info(f"Removing {archive}")
                archive.unlink()


def run_command(
    argv: list[str],
    description: Path,
    recurse: bool = True,
    build_target: Optional[str] = None,
    shell: str = "sh",
    schema: Optional[Path] = None,
) -> None:
    cmd = argv[0]
    tree = load_tree(description)
    root = tree.getroot()
    project = project_root_from_description(description)

    if cmd == "validate":
        validate(description, schema)
        return

    if cmd == "resolve":
        validate(description, schema)
        variant = resolve_variant(root, project, build_target)
        if variant is None:
            _die("resolve requires <variant> entries in Description.xml")
        emit_resolve(variant, shell)
        return

    if cmd == "list-components":
        validate(description, schema)
        target = resolve_build_target(root, project, build_target)
        list_components(root, target)
        return

    if cmd == "fetch":
        validate(description, schema)
        target = resolve_build_target(root, project, build_target)
        os.environ["BUILD_TARGET"] = target
        _info(f"Fetching components for build_target={target}")
        for comp in iter_components(root):
            if not component_applies(comp, target):
                continue
            fetch_component(
                comp, project, recurse=recurse, build_target=target
            )
        return

    if cmd == "check":
        validate(description, schema)
        target = resolve_build_target(root, project, build_target)
        os.environ["BUILD_TARGET"] = target
        if recurse:
            for comp in iter_components(root):
                if not component_applies(comp, target) or is_optional(comp):
                    continue
                path = text_of(comp, "path")
                if not path:
                    continue
                nested = (project / path / "Description.xml").resolve()
                if nested.is_file():
                    _info(f"Recurse check into {nested}")
                    run_command(
                        ["check"],
                        description=nested,
                        recurse=True,
                        build_target=target,
                    )
        _info("check OK")
        return

    if cmd == "clean":
        validate(description, schema)
        target = resolve_build_target(root, project, build_target)
        for comp in iter_components(root):
            if not component_applies(comp, target):
                continue
            # Recurse first while nested Description.xml still exists under <path>
            if recurse:
                path = text_of(comp, "path")
                if path:
                    nested = (project / path / "Description.xml").resolve()
                    if nested.is_file():
                        run_command(
                            ["clean"],
                            description=nested,
                            recurse=True,
                            build_target=target,
                        )
            clean_component(comp, project)
        return

    if cmd == "gen":
        validate(description, schema)
        variant = resolve_variant(root, project, build_target)
        emit_resolve(variant, shell)
        return

    _die(f"Unknown command: {cmd}")


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "-d",
        "--description",
        type=Path,
        default=Path("Description.xml"),
        help="Path to Description.xml",
    )
    p.add_argument(
        "--schema",
        type=Path,
        default=None,
        help="Path to Description.xsd (default: next to Description.xml)",
    )
    p.add_argument(
        "--shell",
        choices=("bat", "sh"),
        default="sh",
        help="Shell syntax for resolve/gen output",
    )
    p.add_argument(
        "--build-target",
        default=None,
        help="Override BUILD_TARGET for resolve/fetch/list",
    )
    # Internal only — not part of Build.bat UI; hidden from --help.
    p.add_argument(
        "--no-recurse",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    p.add_argument(
        "command",
        choices=(
            "validate",
            "resolve",
            "list-components",
            "fetch",
            "check",
            "clean",
            "gen",
        ),
    )
    return p


def main(argv: Optional[Iterable[str]] = None) -> None:
    args = build_arg_parser().parse_args(list(argv) if argv is not None else None)
    description = args.description.resolve()
    run_command(
        [args.command],
        description=description,
        recurse=not args.no_recurse,
        build_target=args.build_target,
        shell=args.shell,
        schema=args.schema.resolve() if args.schema else None,
    )


if __name__ == "__main__":
    main()
