#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build.ninja"

LOG_RESET = "\033[0m"
LOG_COLORS = {
    "ASM": "\033[1;35m",
    "BOOT": "\033[1;35m",
    "CC": "\033[1;32m",
    "CHECK": "\033[1;34m",
    "CLEAN": "\033[1;31m",
    "CP": "\033[1;34m",
    "CXX": "\033[1;36m",
    "FETCH": "\033[1;34m",
    "FIND": "\033[1;34m",
    "FMT": "\033[1;34m",
    "GEN": "\033[1;35m",
    "GRAPH": "\033[1;34m",
    "HOSTLD": "\033[1;33m",
    "ISO": "\033[1;35m",
    "LD": "\033[1;33m",
    "OBJCOPY": "\033[1;35m",
    "PREP": "\033[1;34m",
    "RUN": "\033[1;32m",
    "RUST": "\033[1;32m",
    "SIZE": "\033[1;34m",
    "STAGE": "\033[1;34m",
    "VDISK": "\033[1;35m",
    "VMDK": "\033[1;35m",
}


def log_label(tag: str) -> str:
    return f"{LOG_COLORS[tag]}[{tag}]{LOG_RESET}"


def log_desc(tag: str, text: str = "") -> str:
    return log_label(tag) if not text else f"{log_label(tag)} {text}"



def rel(path: Path | str) -> str:
    p = Path(path)
    if p.is_absolute():
        try:
            p = p.relative_to(ROOT)
        except ValueError:
            return p.as_posix()
    return p.as_posix()


def esc(path: str) -> str:
    return path.replace("$", "$$").replace(" ", "$ ")


def paths(items: list[Path | str]) -> str:
    return " ".join(esc(rel(item)) for item in items)


def cmd_output(command: list[str]) -> str:
    try:
        return subprocess.check_output(command, cwd=str(ROOT), text=True, stderr=subprocess.DEVNULL).strip()
    except (FileNotFoundError, subprocess.CalledProcessError):
        return ""


def find_executable(names: list[str], extra_paths: list[Path] | None = None) -> str:
    extra_paths = extra_paths or []
    for name in names:
        found = shutil.which(name)
        if found:
            return found
        for base in extra_paths:
            candidate = base / name
            if candidate.exists() and os.access(candidate, os.X_OK):
                return str(candidate)
    return names[0] if names else ""


def find_rustc() -> str:
    override = os.environ.get("RUSTC")
    if override:
        return override
    return find_executable(
        ["rustc"],
        [
            Path.home() / ".cargo/bin",
            Path("/snap/bin"),
            Path("/home/leon/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin"),
        ],
    )


def rust_target_libs(rustc: str, target: str) -> list[Path]:
    if not rustc:
        raise SystemExit("缺少 Rust 编译器：请安装 rustup/rustc，或设置 RUSTC=/path/to/rustc")
    libdir = cmd_output([rustc, "--print", "target-libdir", "--target", target])
    if not libdir:
        raise SystemExit(
            f"无法查询 Rust 目标库目录：{target}\n"
            f"请先运行：rustup target add {target}\n"
            "如果已经安装，请确认 RUSTC 指向同一个 rustup 工具链，然后运行：ninja reconfigure"
        )
    libs: list[Path] = []
    missing: list[str] = []
    for pattern in ("libcore-*.rlib", "libcompiler_builtins-*.rlib"):
        matches = sorted(Path(libdir).glob(pattern))
        if matches:
            libs.append(matches[-1])
        else:
            missing.append(pattern)
    if missing:
        raise SystemExit(
            f"Rust 目标库不完整：{target}\n"
            f"缺少：{', '.join(missing)}\n"
            f"目标库目录：{libdir}\n"
            f"请运行：rustup target add {target}\n"
            "然后运行：ninja reconfigure"
        )
    return libs


def opt_flag() -> str:
    return "-O" + os.environ.get("OPT_LEVEL", "2")


def rust_opt_flag() -> str:
    return "-C opt-level=" + os.environ.get("OPT_LEVEL", "2")


def find_files(base: str, suffixes: tuple[str, ...], *, max_depth: int | None = None) -> list[Path]:
    root = ROOT / base
    if not root.exists():
        return []
    out: list[Path] = []
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix not in suffixes:
            continue
        if max_depth is not None:
            try:
                depth = len(path.relative_to(root).parts)
            except ValueError:
                continue
            if depth > max_depth:
                continue
        out.append(path)
    return sorted(out)


def headers(*bases: str) -> list[Path]:
    out: list[Path] = []
    for base in bases:
        out.extend(find_files(base, (".h", ".hpp")))
    return sorted(out)


class Ninja:
    def __init__(self) -> None:
        self.lines: list[str] = []

    def line(self, text: str = "") -> None:
        self.lines.append(text)

    def comment(self, text: str) -> None:
        self.line(f"# {text}")

    def var(self, name: str, value: str) -> None:
        self.line(f"{name} = {value}")

    def rule(self, name: str, command: str, description: str | None = None, *, depfile: str | None = None) -> None:
        self.line(f"rule {name}")
        self.line(f"  command = {command}")
        if description:
            self.line(f"  description = {description}")
        if depfile:
            self.line(f"  depfile = {depfile}")
            self.line("  deps = gcc")
        self.line()

    def build(
        self,
        outputs: list[Path | str] | Path | str,
        rule: str,
        inputs: list[Path | str] | Path | str | None = None,
        *,
        implicit: list[Path | str] | None = None,
        order_only: list[Path | str] | None = None,
        variables: dict[str, str] | None = None,
    ) -> None:
        outs = [outputs] if isinstance(outputs, (str, Path)) else outputs
        ins: list[Path | str] = []
        if inputs is not None:
            ins = [inputs] if isinstance(inputs, (str, Path)) else inputs
        line = f"build {paths(list(outs))}: {rule}"
        if ins:
            line += f" {paths(ins)}"
        if implicit:
            line += f" | {paths(implicit)}"
        if order_only:
            line += f" || {paths(order_only)}"
        self.line(line)
        if variables:
            for key, value in variables.items():
                self.line(f"  {key} = {value}")
        self.line()


def root_objects(n: Ninja, builtin_xhci: bool) -> list[Path]:
    source_roots = ["kernel", "driver", "graphics", "font", "lib"]
    nasm_files: list[Path] = []
    asm_files: list[Path] = []
    c_files: list[Path] = []
    cpp_files: list[Path] = []
    for base in source_roots:
        nasm_files.extend(find_files(base, (".asm",)))
        asm_files.extend(find_files(base, (".S",)))
        c_files.extend(find_files(base, (".c",)))
        cpp_files.extend(find_files(base, (".cpp",)))
    if builtin_xhci:
        cpp_files.extend(find_files("kmod/xhci", (".cpp",)))

    config = Path("kernel/build_config.h")
    settings = Path("kernel/build_settings.h")
    objs: list[Path] = []
    for src in nasm_files:
        obj = Path("out") / src.relative_to(ROOT).with_suffix(".o")
        n.build(obj, "nasm", src, implicit=[settings, config])
        objs.append(obj)
    for src in asm_files:
        obj = Path("out") / src.relative_to(ROOT).with_suffix(".o")
        n.build(obj, "root_as", src, implicit=[settings, config])
        objs.append(obj)
    for src in c_files:
        obj = Path("out") / src.relative_to(ROOT).with_suffix(".o")
        n.build(obj, "root_cc", src, implicit=[settings, config])
        objs.append(obj)
    for src in cpp_files:
        obj = Path("out") / src.relative_to(ROOT).with_suffix(".o")
        n.build(obj, "root_cxx", src, implicit=[settings, config])
        objs.append(obj)
    return objs


def xapi(n: Ninja) -> tuple[list[Path], Path, Path, Path]:
    xapi_headers = headers("user/xapi/include")
    impl_includes = find_files("user/xapi", (".inc",))
    srcs = sorted(
        p
        for p in find_files("user/xapi", (".cpp", ".S"))
        if p.name not in ("start.cpp", "constart.cpp")
    )
    core_objs: list[Path] = []
    for src in srcs:
        rel_src = src.relative_to(ROOT / "user/xapi")
        obj = Path("out/xapi") / Path(str(rel_src) + ".o")
        rule = "xapi_as" if src.suffix == ".S" else "xapi_cxx"
        n.build(obj, rule, src, implicit=xapi_headers + impl_includes)
        core_objs.append(obj)
    start_obj = Path("out/xapi/start.cpp.o")
    constart_obj = Path("out/xapi/constart.cpp.o")
    n.build(start_obj, "xapi_head", "user/xapi/start.cpp", implicit=xapi_headers)
    n.build(constart_obj, "xapi_headcon", "user/xapi/constart.cpp", implicit=xapi_headers)
    liballoc = Path("out/xapi/liballoc-x86_64.a")
    n.build(liballoc, "copy", "liballoc-x86_64.a")
    return core_objs, start_obj, constart_obj, liballoc


def user_compile(n: Ninja, out: Path, src: str, flags: str, implicit: list[Path]) -> Path:
    n.build(out, "user_cxx_custom", src, implicit=implicit, variables={"cflags": flags})
    return out


def user_compile_c(n: Ninja, out: Path, src: str, flags: str, implicit: list[Path]) -> Path:
    n.build(out, "user_cc_custom", src, implicit=implicit, variables={"cflags": flags})
    return out


def user_simple_app(
    n: Ninja,
    name: str,
    src: str,
    core_objs: list[Path],
    liballoc: Path,
    implicit: list[Path],
    *,
    console: bool = False,
    c_source: bool = False,
) -> Path:
    target = Path(f"out/{name}")
    obj = Path(f"out/{name}.o")
    if c_source:
        cflags = f"{opt_flag()} -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c11 -fno-stack-protector -fno-strict-aliasing -fshort-wchar -nostdinc -I ./user/xapi/include -MMD -MP"
        user_compile_c(n, obj, src, cflags, implicit)
    else:
        user_compile(n, obj, src, "$user_cflags", implicit)
    if c_source:
        xobjs = core_objs
    elif console:
        xobjs = core_objs
    else:
        xobjs = core_objs + [Path("out/xapi/start.cpp.o")]
    n.build(target, "user_ld", xobjs + [liballoc, obj], variables={"message": log_desc("LD", target.as_posix())})
    return target


def grouped_cpp_app(
    n: Ninja,
    name: str,
    sources: list[str],
    obj_prefix: str,
    core_objs: list[Path],
    liballoc: Path,
    implicit: list[Path],
    *,
    extra_implicit: list[Path] | None = None,
) -> Path:
    objs: list[Path] = []
    for src in sources:
        src_path = Path(src)
        obj = Path("out") / obj_prefix / src_path.name.replace(".cpp", ".o")
        user_compile(n, obj, f"user/{src}", "$user_cflags", implicit + (extra_implicit or []))
        objs.append(obj)
    target = Path(f"out/{name}.elf")
    n.build(target, "user_ld", core_objs + [Path("out/xapi/start.cpp.o"), liballoc] + objs, variables={"message": log_desc("LD", target.as_posix())})
    return target


def rust_gui_app(
    n: Ninja,
    name: str,
    source: str,
    object_path: Path,
    core_objs: list[Path],
    start_obj: Path,
    liballoc: Path,
    rust_libs: list[Path],
    *,
    extra_inputs: list[Path] | None = None,
) -> Path:
    n.build(object_path, "rust_user", source)
    target = Path(f"out/{name}.elf")
    n.build(
        target,
        "user_ld",
        core_objs + [start_obj, liballoc, object_path] + (extra_inputs or []) + rust_libs,
        variables={"message": log_desc("LD", target.as_posix())},
    )
    return target


def user_apps(n: Ninja, core_objs: list[Path], start_obj: Path, constart_obj: Path, liballoc: Path, rust_libs: list[Path]) -> list[Path]:
    xapi_headers = headers("user/xapi/include")
    user_headers = headers("user/include")
    browser_headers = headers("user/browser")
    common_implicit = xapi_headers + user_headers

    targets: list[Path] = []
    gui_core = core_objs + [start_obj]
    console_core = core_objs + [constart_obj]
    browser_core = core_objs

    for name, src in (
        ("init.elf", "user/init.cpp"),
        ("sigtest.elf", "user/sigtest.cpp"),
        ("posixdemo.elf", "user/posixdemo.cpp"),
        ("libctest.elf", "user/libctest.cpp"),
        ("fbtest.elf", "user/fbtest.cpp"),
        ("msgtest.elf", "user/msgtest.cpp"),
        ("cp537.elf", "user/cp537.cpp"),
        ("picturer.elf", "user/picturer/picturer.cpp"),
        ("chat.elf", "user/chat.cpp"),
        ("netcfg.elf", "user/netcfg.cpp"),
    ):
        targets.append(user_simple_app(n, name, src, core_objs, liballoc, common_implicit))

    targets.append(user_simple_app(n, "ccompat-demo.elf", "user/ccompat_demo.c", core_objs + [constart_obj], liballoc, xapi_headers, console=True, c_source=True))
    n.build("out/dyn-hello", "host_cc", "user/dyn_hello.c")
    targets.append(Path("out/dyn-hello"))

    shell_sources = [
        "shell/shell_main.cpp",
        "shell/shell_ui.cpp",
        "shell/shell_cmd.cpp",
        "shell/shell_fs.cpp",
        "shell/shell_editor.cpp",
        "shell/shell_tam.cpp",
        "shell/sxah.cpp",
    ]
    fmanager_sources = [
        "fmanager/fmanager.cpp",
        "fmanager/fm_paint.cpp",
        "fmanager/fm_pathp.cpp",
        "fmanager/fm_processp.cpp",
    ]
    login_sources = ["login/login.cpp"]
    dock_sources = ["dock/dock.cpp"]
    desktop_sources = ["desktop/desktop.cpp"]
    ctrlmenu_sources = ["ctrlmenu/ctrlmenu.cpp", "ctrlmenu/cm_graph.cpp", "ctrlmenu/cm_processp.cpp"]
    installer_sources = ["installer/installer.cpp", "installer/installer_hidden.cpp"]

    targets.append(grouped_cpp_app(n, "shell", shell_sources, "shell", core_objs, liballoc, common_implicit))
    targets.append(grouped_cpp_app(n, "login", login_sources, "login", core_objs, liballoc, common_implicit))
    targets.append(grouped_cpp_app(n, "dock", dock_sources, "dock", core_objs, liballoc, common_implicit))
    targets.append(grouped_cpp_app(n, "desktop", desktop_sources, "desktop", core_objs, liballoc, common_implicit))
    targets.append(grouped_cpp_app(n, "fmanager", fmanager_sources, "fmanager", core_objs, liballoc, common_implicit))
    targets.append(grouped_cpp_app(n, "ctrlmenu", ctrlmenu_sources, "ctrlmenu", core_objs, liballoc, common_implicit))
    targets.append(rust_gui_app(n, "xjver", "user/xjver/xjver.rs", Path("out/xjver/xjver.rs.o"), core_objs, start_obj, liballoc, rust_libs))
    targets.append(grouped_cpp_app(n, "elfrun", ["elfrun/elfrun.cpp"], "elfrun", core_objs, liballoc, common_implicit))
    targets.append(rust_gui_app(n, "launcher", "user/launcher/launcher.rs", Path("out/launcher/launcher.rs.o"), core_objs, start_obj, liballoc, rust_libs))
    targets.append(rust_gui_app(n, "calc", "user/calc/calc.rs", Path("out/calc/calc.rs.o"), core_objs, start_obj, liballoc, rust_libs))
    targets.append(grouped_cpp_app(n, "texter", ["texter/texter.cpp"], "texter", core_objs, liballoc, common_implicit))
    targets.append(grouped_cpp_app(n, "taskmgr", ["taskmgr/taskmgr.cpp"], "taskmgr", core_objs, liballoc, common_implicit))
    targets.append(grouped_cpp_app(n, "filedlg", ["filedlg/filedlg.cpp"], "filedlg", core_objs, liballoc, common_implicit))
    targets.append(
        grouped_cpp_app(
            n,
            "installer",
            installer_sources,
            "installer",
            core_objs,
            liballoc,
            common_implicit,
            extra_implicit=[ROOT / "include/installer_protocol.h", ROOT / "user/installer/installer_hidden.h"],
        )
    )

    bcms_obj = user_compile(n, Path("out/bcms-sp.elf.o"), "user/bcms-sp.cpp", "$user_cflags", common_implicit)
    n.build("out/bcms-sp.elf", "user_ld", browser_core + [constart_obj, liballoc, bcms_obj], variables={"message": log_desc("LD", "out/bcms-sp.elf")})
    targets.append(Path("out/bcms-sp.elf"))

    libvterm_sources = find_files("third_party/libvterm/src", (".c",), max_depth=1)
    libvterm_objs: list[Path] = []
    for src in libvterm_sources:
        obj = Path("out/libvterm") / src.name.replace(".c", ".o")
        n.build(obj, "libvterm_cc", src)
        libvterm_objs.append(obj)
    targets.append(
        rust_gui_app(
            n,
            "busyterm",
            "user/busyterm.rs",
            Path("out/busyterm.rs.o"),
            core_objs,
            start_obj,
            liballoc,
            rust_libs,
            extra_inputs=libvterm_objs,
        )
    )

    mbedtls_sources = [
        "aes.c",
        "aria.c",
        "asn1parse.c",
        "asn1write.c",
        "base64.c",
        "bignum.c",
        "bignum_core.c",
        "bignum_mod.c",
        "bignum_mod_raw.c",
        "block_cipher.c",
        "camellia.c",
        "ccm.c",
        "chacha20.c",
        "chachapoly.c",
        "cipher.c",
        "cipher_wrap.c",
        "cmac.c",
        "constant_time.c",
        "ctr_drbg.c",
        "debug.c",
        "des.c",
        "dhm.c",
        "ecdh.c",
        "ecdsa.c",
        "ecjpake.c",
        "ecp.c",
        "ecp_curves.c",
        "ecp_curves_new.c",
        "error.c",
        "gcm.c",
        "hkdf.c",
        "hmac_drbg.c",
        "md.c",
        "md5.c",
        "mps_reader.c",
        "mps_trace.c",
        "nist_kw.c",
        "oid.c",
        "pem.c",
        "pk.c",
        "pk_ecc.c",
        "pk_wrap.c",
        "pkcs12.c",
        "pkcs5.c",
        "pkparse.c",
        "pkwrite.c",
        "platform.c",
        "platform_util.c",
        "poly1305.c",
        "psa_crypto.c",
        "psa_crypto_aead.c",
        "psa_crypto_cipher.c",
        "psa_crypto_driver_wrappers_no_static.c",
        "psa_crypto_ecp.c",
        "psa_crypto_ffdh.c",
        "psa_crypto_hash.c",
        "psa_crypto_mac.c",
        "psa_crypto_pake.c",
        "psa_crypto_rsa.c",
        "psa_crypto_slot_management.c",
        "psa_util.c",
        "ripemd160.c",
        "rsa.c",
        "rsa_alt_helpers.c",
        "sha1.c",
        "sha256.c",
        "sha3.c",
        "sha512.c",
        "ssl_ciphersuites.c",
        "ssl_client.c",
        "ssl_debug_helpers_generated.c",
        "ssl_msg.c",
        "ssl_tls.c",
        "ssl_tls12_client.c",
        "ssl_tls13_client.c",
        "ssl_tls13_generic.c",
        "ssl_tls13_keys.c",
        "version.c",
        "version_features.c",
        "x509.c",
        "x509_create.c",
        "x509_crl.c",
        "x509_crt.c",
        "x509_csr.c",
        "x509write.c",
        "x509write_crt.c",
        "x509write_csr.c",
    ]
    mbedtls_objs: list[Path] = []
    for name in mbedtls_sources:
        src = Path("third_party/mbedtls-src/library") / name
        obj = Path("out/httpget/mbedtls") / name.replace(".c", ".o")
        n.build(obj, "mbedtls_cc", src)
        mbedtls_objs.append(obj)

    httpget_objs: list[Path] = []
    for name in ("httpget.cpp", "https_client.cpp", "http_tls_compat.cpp"):
        obj = Path("out/httpget") / name.replace(".cpp", ".o")
        user_compile(n, obj, f"user/{name}", "$mbedtls_cflags $user_cflags", common_implicit)
        httpget_objs.append(obj)
    browser_builtins = Path("out/browser-builtins.path")
    n.build(browser_builtins, "browser_builtins")
    n.build(
        "out/httpget.elf",
        "link_with_builtins",
        gui_core + [liballoc] + httpget_objs + mbedtls_objs,
        implicit=[browser_builtins],
        variables={"ldflags": "-Ttext=0x200000", "message": log_desc("LD", "out/httpget.elf")},
    )
    targets.append(Path("out/httpget.elf"))

    nut_specs = [
        ("out/nut/main.o", "user/nut/main.cpp"),
        ("out/nut/https_client.o", "user/https_client.cpp"),
        ("out/nut/http_tls_compat.o", "user/http_tls_compat.cpp"),
    ]
    nut_objs: list[Path] = []
    for obj, src in nut_specs:
        user_compile(n, Path(obj), src, "$mbedtls_cflags $user_cflags", common_implicit)
        nut_objs.append(Path(obj))
    n.build(
        "out/nut.elf",
        "link_with_builtins",
        gui_core + [liballoc] + nut_objs + mbedtls_objs,
        implicit=[browser_builtins],
        variables={"ldflags": "-Ttext=0x200000", "message": log_desc("LD", "out/nut.elf")},
    )
    targets.append(Path("out/nut.elf"))

    litehtml_sources = find_files("third_party/litehtml/src", (".cpp",), max_depth=1)
    litehtml_objs: list[Path] = []
    for src in litehtml_sources:
        obj = Path("out/browser/litehtml") / src.name.replace(".cpp", ".o")
        rule = "litehtml_encodings_cxx" if src.name == "encodings.cpp" else "litehtml_cxx"
        n.build(obj, rule, src)
        litehtml_objs.append(obj)

    gumbo_objs: list[Path] = []
    for src in find_files("third_party/litehtml/src/gumbo", (".c",), max_depth=1):
        obj = Path("out/browser/gumbo") / src.name.replace(".c", ".o")
        n.build(obj, "browser_hosted_cc", src)
        gumbo_objs.append(obj)

    webp_sources = [
        "dec/alpha_dec.c",
        "dec/buffer_dec.c",
        "dec/frame_dec.c",
        "dec/idec_dec.c",
        "dec/io_dec.c",
        "dec/quant_dec.c",
        "dec/tree_dec.c",
        "dec/vp8_dec.c",
        "dec/vp8l_dec.c",
        "dec/webp_dec.c",
        "dsp/alpha_processing.c",
        "dsp/cpu.c",
        "dsp/dec.c",
        "dsp/dec_clip_tables.c",
        "dsp/filters.c",
        "dsp/lossless.c",
        "dsp/rescaler.c",
        "dsp/upsampling.c",
        "dsp/yuv.c",
        "utils/bit_reader_utils.c",
        "utils/color_cache_utils.c",
        "utils/filters_utils.c",
        "utils/huffman_utils.c",
        "utils/palette.c",
        "utils/quant_levels_dec_utils.c",
        "utils/random_utils.c",
        "utils/rescaler_utils.c",
        "utils/thread_utils.c",
        "utils/utils.c",
    ]
    webp_objs: list[Path] = []
    for item in webp_sources:
        src = Path("user/browser/third_party/libwebp/src") / item
        obj = Path("out/browser/libwebp") / item.replace(".c", ".o")
        n.build(obj, "browser_hosted_cc", src)
        webp_objs.append(obj)

    browser_objs: list[Path] = []
    for name in ("browser_app.cpp", "browser_fetch.cpp", "browser_alloc_guard.cpp", "browser_image_decode.cpp"):
        src = Path("user/browser") / name
        obj = Path("out/browser") / name.replace(".cpp", ".o")
        n.build(obj, "browser_hosted_cxx", src, implicit=xapi_headers + user_headers + browser_headers)
        browser_objs.append(obj)
    browser_start = Path("out/browser/browser_start.o")
    user_compile(n, browser_start, "user/browser/browser_start.cpp", "$user_cflags", common_implicit + browser_headers)
    n.build(
        "out/browser.elf",
        "link_browser",
        browser_core + [liballoc, browser_start] + browser_objs + litehtml_objs + gumbo_objs + webp_objs + [Path("out/httpget/https_client.o"), Path("out/httpget/http_tls_compat.o")] + mbedtls_objs,
        implicit=[browser_builtins],
    )
    targets.append(Path("out/browser.elf"))

    return targets


def kmods(n: Ninja) -> list[Path]:
    targets: list[Path] = []
    e1000_objs: list[Path] = []
    for src in find_files("kmod/e1000", (".cpp",), max_depth=1):
        obj = Path("out/kmod/e1000") / src.name.replace(".cpp", ".o")
        n.build(obj, "kmod_e1000_cxx", src)
        e1000_objs.append(obj)
    n.build("out/e1000.sys", "kmod_link", e1000_objs, variables={"ldflags": "-nostdlib -Wl,-e,dlmain"})
    targets.append(Path("out/e1000.sys"))

    lwip_c = [
        "lwip/core/def.c",
        "lwip/core/dns.c",
        "lwip/core/init.c",
        "lwip/core/inet_chksum.c",
        "lwip/core/ip.c",
        "lwip/core/mem.c",
        "lwip/core/memp.c",
        "lwip/core/netif.c",
        "lwip/core/pbuf.c",
        "lwip/core/stats.c",
        "lwip/core/sys.c",
        "lwip/core/tcp.c",
        "lwip/core/tcp_in.c",
        "lwip/core/tcp_out.c",
        "lwip/core/udp.c",
        "lwip/core/timeouts.c",
        "lwip/core/ipv4/dhcp.c",
        "lwip/core/ipv4/etharp.c",
        "lwip/core/ipv4/icmp.c",
        "lwip/core/ipv4/ip4.c",
        "lwip/core/ipv4/ip4_addr.c",
        "lwip/netif/ethernet.c",
        "lwip/api/api_lib.c",
        "lwip/api/api_msg.c",
        "lwip/api/err.c",
        "lwip/api/netbuf.c",
        "lwip/api/netifapi.c",
        "lwip/api/tcpip.c",
    ]
    net_objs: list[Path] = []
    for item in lwip_c:
        src = Path("kmod/netserver") / item
        obj = Path("out/kmod/netserver") / item.replace(".c", ".o")
        n.build(obj, "netserver_cc", src)
        net_objs.append(obj)
    for item in ("netserver.cpp", "arch/sys_arch.cpp", "arch/utils.cpp"):
        src = Path("kmod/netserver") / item
        obj = Path("out/kmod/netserver") / item.replace(".cpp", ".o")
        n.build(obj, "netserver_cxx", src)
        net_objs.append(obj)
    n.build("out/netserver.sys", "kmod_link", net_objs, variables={"ldflags": "-nostdlib -Wl,-e,dlmain -Wl,-z,muldefs"})
    targets.append(Path("out/netserver.sys"))

    xhci_objs: list[Path] = []
    for name in ("usb_core.cpp", "usb_hub.cpp", "usb_msc.cpp", "xhci.cpp"):
        src = Path("kmod/xhci") / name
        obj = Path("out/kmod_module/xhci") / name.replace(".cpp", ".o")
        n.build(obj, "kmod_xhci_cxx", src)
        xhci_objs.append(obj)
    n.build("out/xhci.sys", "kmod_link", xhci_objs, variables={"ldflags": "-nostdlib -Wl,-e,dlmain -Wl,-Bsymbolic -Wl,-z,muldefs"})
    targets.append(Path("out/xhci.sys"))
    return targets


def stardust_example(
    n: Ninja,
    name: str,
    framework_sources: list[str],
    cxxflags: str,
    core_objs: list[Path],
    start_obj: Path,
    liballoc: Path,
) -> Path:
    base = Path(f"frameworks/StardustUI/examples/{name}")
    if not (ROOT / base).is_dir():
        return Path()
    obj_dir = Path(f"out/stardustui/{name}/obj")
    objs: list[Path] = []
    app_obj = obj_dir / f"{name}.o"
    n.build(
        app_obj,
        "stardust_cxx",
        base / f"{name}.cpp",
        variables={"cflags": cxxflags},
    )
    objs.append(app_obj)
    for item in framework_sources + ["src/platforms/xj380.cpp"]:
        src = Path("frameworks/StardustUI") / item
        obj = obj_dir / "framework" / item.removeprefix("src/").replace(".cpp", ".o")
        n.build(obj, "stardust_cxx", src, variables={"cflags": cxxflags})
        objs.append(obj)
    target = Path(f"frameworks/StardustUI/examples/{name}/build/xj380/{name}.elf")
    n.build(target, "stardust_ld", core_objs + [start_obj, liballoc] + objs, variables={"message": log_desc("LD", target.as_posix())})
    app_out = Path(f"out/{name}.elf")
    n.build(app_out, "copy", target)
    return app_out


def stardust(n: Ninja, core_objs: list[Path], start_obj: Path, liballoc: Path) -> list[Path]:
    hello_sources = [
        "src/file.cpp",
        "src/sytel.cpp",
        "src/text/font.cpp",
        "src/text/text_renderer.cpp",
        "src/text/rasterizer/rasterizer.cpp",
        "src/text/truetype/face.cpp",
        "src/window.cpp",
        "src/components/base.cpp",
        "src/components/lable.cpp",
        "src/components/button.cpp",
        "src/components/scrollbar.cpp",
        "src/components/textbox.cpp",
    ]
    full_sources = [
        "src/file.cpp",
        "src/network.cpp",
        "src/theme.cpp",
        "src/sytel.cpp",
        "src/text/font.cpp",
        "src/text/text_renderer.cpp",
        "src/text/rasterizer/rasterizer.cpp",
        "src/text/truetype/face.cpp",
        "src/window.cpp",
        "src/components/base.cpp",
        "src/components/lable.cpp",
        "src/components/button.cpp",
        "src/components/canvas.cpp",
        "src/components/scrollbar.cpp",
        "src/components/textbox.cpp",
        "src/components/flex.cpp",
    ]
    hello_flags = f"-Iframeworks/StardustUI -Iframeworks/StardustUI/includes -DXJ380 {opt_flag()} -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=gnu++17 -fno-stack-protector -fno-exceptions -fno-strict-aliasing -fno-rtti -fshort-wchar -nostdinc -Wno-write-strings -MMD -MP"
    opt_flags = f"-Iframeworks/StardustUI -Iframeworks/StardustUI/includes -DXJ380 {opt_flag()} -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=gnu++17 -fno-stack-protector -fno-exceptions -fno-strict-aliasing -fno-rtti -fno-use-cxa-atexit -fshort-wchar -nostdinc -Wno-write-strings -MMD -MP"
    targets = [
        stardust_example(n, "helloworld", hello_sources, hello_flags, core_objs, start_obj, liballoc),
        stardust_example(n, "layout", full_sources, opt_flags, core_objs, start_obj, liballoc),
    ]
    duck = stardust_example(n, "duckchat", full_sources, opt_flags, core_objs, start_obj, liballoc)
    if duck != Path():
        targets.append(duck)
    return [t for t in targets if t != Path()]


def phony(n: Ninja, name: str, deps: list[Path | str]) -> None:
    n.build(name, "phony", deps)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=str(OUT))
    args = parser.parse_args()

    builtin_xhci = os.environ.get("BUILTIN_XHCI", "1") == "1"
    cpp_extra = " -DXHCI_BUILTIN" if builtin_xhci else ""
    host_gcc = os.environ.get("HOST_GCC", "gcc")
    host_gcc_triple = cmd_output([host_gcc, "-dumpmachine"])
    rust_target = os.environ.get("RUST_TARGET", "x86_64-unknown-none")
    rustc = find_rustc()
    rust_libs = rust_target_libs(rustc, rust_target)

    n = Ninja()
    n.comment("Generated by tools/gen_ninja.py. Run `ninja reconfigure` after changing the build graph.")
    n.var("ninja_required_version", "1.10")
    n.var("cc", os.environ.get("CC", os.environ.get("COMPILER_PREFIX", "") + "clang"))
    n.var("cxx", os.environ.get("CPP", os.environ.get("COMPILER_PREFIX", "") + "clang++"))
    n.var("nasm", os.environ.get("NASM", os.environ.get("COMPILER_PREFIX", "") + "nasm"))
    n.var("ld", os.environ.get("LD", os.environ.get("COMPILER_PREFIX", "") + "ld"))
    n.var("objc", os.environ.get("OBJC", os.environ.get("COMPILER_PREFIX", "") + "objcopy"))
    n.var("host_cc", os.environ.get("HOST_CC", "cc"))
    n.var("user_cc", os.environ.get("CC", os.environ.get("COMPILER_PREFIX", "") + "clang"))
    n.var("user_cxx", os.environ.get("CXX", "g++"))
    n.var("user_ld", os.environ.get("LD", "ld"))
    n.var("rustc", rustc)
    n.var("rust_target", rust_target)
    n.var("opt", opt_flag())
    n.var("rust_opt", rust_opt_flag())
    n.var("nasm_flags", "-f elf64 -DX86_64_TARGET -DUEFI")
    n.var("asm_flags", "$opt -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-strict-aliasing -mno-80387 -MMD -MP")
    n.var("c_flags", "$opt -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-strict-aliasing -std=c11 -fshort-wchar -nostdinc -mno-80387 -I./include -MMD -MP")
    n.var(
        "cpp_flags",
        "$opt -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-strict-aliasing -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -I./include -Wno-writable-strings -Wno-c++11-narrowing -MMD -MP"
        + cpp_extra,
    )
    n.var("boot_c_flags", "-g -I ./boot/include -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar")
    n.var("user_cflags", "$opt -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c++11 -fno-stack-protector -fno-exceptions -fno-strict-aliasing -fshort-wchar -nostdinc -I ./user/xapi/include -I ./user/include -I ./include -Wno-write-strings -MMD -MP")
    n.var("rust_user_flags", "--edition=2024 --target $rust_target --emit=obj --crate-type lib -C panic=abort -C no-redzone=yes $rust_opt -C debuginfo=2 -C overflow-checks=off")
    n.var("mbedtls_cflags", "-I ./user/include/mbed_compat -I ./third_party/mbedtls-src/include -DMBEDTLS_USER_CONFIG_FILE='\"mbed_compat/xj380_mbedtls_user_config.h\"'")
    n.var("mbedtls_ccflags", "$opt -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c11 -fno-stack-protector -fno-strict-aliasing -fshort-wchar -nostdinc -I ./user/include/mbed_compat -I ./user/xapi/include -I ./user/include -I ./include -I ./third_party/mbedtls-src/include -DMBEDTLS_USER_CONFIG_FILE='\"mbed_compat/xj380_mbedtls_user_config.h\"' -DMBEDTLS_NO_UDBL_DIVISION -MMD -MP")
    litehtml_includes = "-I ./third_party/litehtml/include -I ./third_party/litehtml/include/litehtml -I ./third_party/litehtml/src -I ./third_party/litehtml/src/gumbo -I ./third_party/litehtml/src/gumbo/include -I ./third_party/litehtml/src/gumbo/include/gumbo"
    webp_includes = "-I ./user/browser/third_party/libwebp"
    n.var("browser_hosted_cxxflags", f"$opt -Wall -g -std=c++17 -m64 -mstackrealign -fno-stack-protector -fno-strict-aliasing -fno-threadsafe-statics -fno-use-cxa-atexit -ffunction-sections -fdata-sections -I ./user/browser {litehtml_includes} {webp_includes} -DLITEHTML_NO_THREADS -DLITEHTML_KEEP_GUMBO_OUTPUT -MMD -MP")
    n.var("browser_hosted_cflags", f"$opt -Wall -g -std=c11 -m64 -mstackrealign -fno-stack-protector -fno-strict-aliasing -ffunction-sections -fdata-sections {litehtml_includes} {webp_includes} -DNDEBUG -DHAVE_CONFIG_H -MMD -MP")
    n.var("libvterm_cflags", "$opt -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c99 -fno-stack-protector -fno-strict-aliasing -fshort-wchar -nostdinc -I ./user/xapi/include -I ./user/include -I ./third_party/libvterm/include -I ./third_party/libvterm/src -MMD -MP")
    n.var("kmod_cxxflags", "$opt -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-strict-aliasing -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -Wno-writable-strings -Wno-c++11-narrowing -fPIC -fvisibility=hidden -MMD -MP")
    n.var("netserver_cflags", "$opt -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-strict-aliasing -std=gnu11 -fshort-wchar -nostdinc -mno-80387 -fPIC -fvisibility=hidden -MMD -MP -I./include -I./kmod/netserver/lwip/include -I./kmod/netserver")
    n.var("netserver_cxxflags", "$kmod_cxxflags -MMD -MP -I./include -I./kmod/netserver/lwip/include -I./kmod/netserver")
    n.var("browser_stdcpp", cmd_output(["g++", "-print-file-name=libstdc++.a"]))
    n.var("browser_supcpp", cmd_output(["g++", "-print-file-name=libsupc++.a"]))
    n.var("browser_libgcc", cmd_output(["g++", "-print-libgcc-file-name"]))
    n.var("browser_libgcc_eh", cmd_output(["g++", "-print-file-name=libgcc_eh.a"]))
    n.var("host_gcc_triple", host_gcc_triple)
    n.line()

    n.rule("regen", "python3 tools/gen_ninja.py --out build.ninja", log_desc("GEN", "build.ninja"), depfile=None)
    n.rule("nasm", "mkdir -p $$(dirname $out) && $nasm $nasm_flags $in -o $out", log_desc("ASM", "$in -> $out"))
    n.rule("root_as", "mkdir -p $$(dirname $out) && $cc -mcmodel=large $asm_flags -MF $out.d -c $in -o $out", log_desc("ASM", "$in -> $out"), depfile="$out.d")
    n.rule("root_cc", "mkdir -p $$(dirname $out) && $cc -fno-pic -fno-pie -mcmodel=large $c_flags -MF $out.d -c $in -o $out", log_desc("CC", "$in -> $out"), depfile="$out.d")
    n.rule("root_cxx", "mkdir -p $$(dirname $out) && $cxx -fno-pic -fno-pie -mcmodel=large $cpp_flags -MF $out.d -c $in -o $out", log_desc("CXX", "$in -> $out"), depfile="$out.d")
    n.rule("gen_config", "tmp=$out.tmp; { echo '#pragma once'; echo '#define CONFIG_KERNEL_BUSYBOX_ALIASES '$${KERNEL_BUSYBOX_ALIASES:-1}; echo '#define CONFIG_KERNEL_BUILTIN_XHCI '$${BUILTIN_XHCI:-1}; } > $$tmp; if ! cmp -s $$tmp $out; then mv $$tmp $out; else rm -f $$tmp; fi", log_desc("GEN", "$out"))
    n.rule("boot", "mkdir -p $$(dirname $out) && x86_64-w64-mingw32-gcc $boot_c_flags -o $out boot/bootx64.c boot/bootlib.c", log_desc("BOOT", "$out"))
    n.rule("objcopy_bin", "mkdir -p $$(dirname $out) && $objc -I binary -O elf64-x86-64 $bin_input $out", log_desc("OBJCOPY", "$bin_input"))
    n.rule(
        "kernel_link",
        "$ld -z muldefs -T linker.ld --static --wrap=malloc --wrap=calloc --wrap=realloc --wrap=aligned_alloc "
        "-o $out $in",
        log_desc("LD", "$out"),
    )
    n.rule("download_liballoc", "[ -f $out ] || curl -Lo $out https://github.com/plos-clan/liballoc/releases/download/release/liballoc-x86_64.a", log_desc("FETCH", "$out"))
    n.rule("copy", "mkdir -p $$(dirname $out) && cp -a $in $out", log_desc("CP", "$in -> $out"))
    n.rule("user_cxx_custom", "mkdir -p $$(dirname $out) && $user_cxx $cflags -MF $out.d -c $in -o $out", log_desc("CXX", "$in -> $out"), depfile="$out.d")
    n.rule("user_cc_custom", "mkdir -p $$(dirname $out) && $user_cc $cflags -MF $out.d -c $in -o $out", log_desc("CC", "$in -> $out"), depfile="$out.d")
    n.rule("rust_user", "mkdir -p $$(dirname $out) && $rustc $rust_user_flags $in -o $out", log_desc("RUST", "$in -> $out"))
    n.rule("xapi_cxx", "mkdir -p $$(dirname $out) && $user_cxx $opt -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c++11 -fno-stack-protector -fno-strict-aliasing -fshort-wchar -nostdinc -I ./user/xapi/include -Wno-write-strings -MMD -MP -MF $out.d -c $in -o $out", log_desc("CXX", "$in"), depfile="$out.d")
    n.rule("xapi_as", "mkdir -p $$(dirname $out) && $user_cc $opt -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c++11 -fno-stack-protector -fno-strict-aliasing -fshort-wchar -nostdinc -I ./user/xapi/include -Wno-write-strings -MMD -MP -MF $out.d -c $in -o $out", log_desc("ASM", "$in"), depfile="$out.d")
    n.rule("xapi_head", "mkdir -p $$(dirname $out) && $user_cc $opt -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c++11 -fno-stack-protector -fno-strict-aliasing -fshort-wchar -nostdinc -I ./user/xapi/include -Wno-write-strings -MMD -MP -MF $out.d -c $in -o $out", log_desc("CXX", "start.cpp"), depfile="$out.d")
    n.rule("xapi_headcon", "mkdir -p $$(dirname $out) && $user_cc $opt -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c++11 -fno-stack-protector -fno-strict-aliasing -fshort-wchar -nostdinc -I ./user/xapi/include -Wno-write-strings -MMD -MP -MF $out.d -c $in -o $out", log_desc("CXX", "constart.cpp"), depfile="$out.d")
    n.rule("user_ld", "$user_ld -Ttext=0x200000 $in -o $out", log_desc("LD", "$out"))
    n.rule("host_cc", "mkdir -p $$(dirname $out) && $user_cc -O0 -g -Wall -Wextra $in -o $out", log_desc("HOSTLD", "$in"))
    n.rule("mbedtls_cc", "mkdir -p $$(dirname $out) && $user_cc $mbedtls_ccflags -MF $out.d -c $in -o $out", log_desc("CC", "$in -> $out"), depfile="$out.d")
    n.rule("browser_builtins", "mkdir -p $$(dirname $out) && python3 tools/ninja_build.py browser-builtins > $out", log_desc("FIND", "browser builtins"))
    n.rule("link_with_builtins", "$user_ld $ldflags $in $$(cat out/browser-builtins.path) -o $out", log_desc("LD", "$out"))
    n.rule("browser_hosted_cxx", "mkdir -p $$(dirname $out) && $user_cxx $browser_hosted_cxxflags -MF $out.d -c $in -o $out", log_desc("CXX", "$in -> $out"), depfile="$out.d")
    n.rule("litehtml_cxx", "mkdir -p $$(dirname $out) && $user_cxx $browser_hosted_cxxflags -fno-exceptions -MF $out.d -c $in -o $out", log_desc("CXX", "$in -> $out"), depfile="$out.d")
    n.rule("litehtml_encodings_cxx", "mkdir -p $$(dirname $out) && $user_cxx $browser_hosted_cxxflags -fexceptions -MF $out.d -c $in -o $out", log_desc("CXX", "$in -> $out"), depfile="$out.d")
    n.rule("browser_hosted_cc", "mkdir -p $$(dirname $out) && $user_cc $browser_hosted_cflags -MF $out.d -c $in -o $out", log_desc("CC", "$in -> $out"), depfile="$out.d")
    n.rule("link_browser", "$user_ld -Ttext=0x200000 --gc-sections --wrap=free $in --start-group $browser_stdcpp $browser_supcpp $browser_libgcc_eh $browser_libgcc $$(cat out/browser-builtins.path) --end-group -o $out", log_desc("LD", "$out"))
    n.rule("libvterm_cc", "mkdir -p $$(dirname $out) && $user_cc $libvterm_cflags -MF $out.d -c $in -o $out", log_desc("CC", "$in -> $out"), depfile="$out.d")
    n.rule("kmod_e1000_cxx", "mkdir -p $$(dirname $out) && $cxx $kmod_cxxflags -I./include -MF $out.d -c $in -o $out", log_desc("CXX", "$in"), depfile="$out.d")
    n.rule("kmod_xhci_cxx", "mkdir -p $$(dirname $out) && $cxx $kmod_cxxflags -I./include -I./kmod/xhci -MF $out.d -c $in -o $out", log_desc("CXX", "$in"), depfile="$out.d")
    n.rule("netserver_cc", "mkdir -p $$(dirname $out) && $cc $netserver_cflags -MF $out.d -c $in -o $out", log_desc("CC", "$in"), depfile="$out.d")
    n.rule("netserver_cxx", "mkdir -p $$(dirname $out) && $cxx $netserver_cxxflags -MF $out.d -c $in -o $out", log_desc("CXX", "$in"), depfile="$out.d")
    n.rule("kmod_link", "$cxx -shared $in $ldflags -o $out", log_desc("LD", "$out"))
    n.rule("stardust_cxx", "mkdir -p $$(dirname $out) && $user_cxx $cflags -MF $out.d -c $in -o $out", log_desc("CXX", "$in"), depfile="$out.d")
    n.rule("stardust_ld", "mkdir -p $$(dirname $out) && $user_ld -Ttext=0x200000 -e xapi_start $in -o $out", log_desc("LD", "$out"))
    n.rule("python_cmd", "python3 tools/ninja_build.py $cmd", "$desc")
    n.rule("shell_cmd", "$cmd", "$desc")

    n.build(
        "build.ninja",
        "regen",
        ["tools/gen_ninja.py", "tools/ninja_build.py"],
        implicit=[
            "Makefile",
            "user/Makefile",
            "user/xapi/Makefile",
            "user/calc/calc.rs",
            "user/busyterm.rs",
            "user/xjver/xjver.rs",
            "user/launcher/launcher.rs",
            "user/elfrun/elfrun.cpp",
            "user/msgtest.cpp",
        ],
    )
    n.build("kernel/build_config.h", "gen_config")
    n.build("liballoc-x86_64.a", "download_liballoc")

    kernel_objs = root_objects(n, builtin_xhci)
    n.build("out/BOOTX64.efi", "boot", ["boot/bootx64.c", "boot/bootlib.c"], implicit=find_files("boot/include", (".h",)))
    n.build("font/hankaku.o", "objcopy_bin", "font/hankaku.bin", variables={"bin_input": "./font/hankaku.bin"})
    n.build(
        "graphics/logo/xj380_2.o",
        "objcopy_bin",
        "graphics/logo/xj380_2.png",
        variables={"bin_input": "./graphics/logo/xj380_2.png"},
    )
    n.build("out/kernel.krl", "kernel_link", kernel_objs + [Path("font/hankaku.o"), Path("graphics/logo/xj380_2.o"), Path("liballoc-x86_64.a")], implicit=["linker.ld"])

    core_objs, start_obj, constart_obj, xapi_liballoc = xapi(n)
    user_targets = user_apps(n, core_objs, start_obj, constart_obj, xapi_liballoc, rust_libs)
    kmod_targets = kmods(n)
    stardust_targets = stardust(n, core_objs, start_obj, xapi_liballoc)

    all_deps = [Path("out/BOOTX64.efi"), Path("out/kernel.krl")] + user_targets + kmod_targets + stardust_targets
    phony(n, "all", all_deps)
    phony(n, "build_xapi", core_objs + [start_obj, constart_obj, xapi_liballoc])
    phony(n, "stardustui.examples.copy", stardust_targets)
    phony(n, "kmods", kmod_targets)

    n.build(
        ["vdisk", "XJ380.img"],
        "python_cmd",
        all_deps,
        variables={"cmd": "vdisk", "desc": log_desc("VDISK", "XJ380.img"), "pool": "console"},
    )
    n.build(
        "complete",
        "python_cmd",
        all_deps,
        variables={"cmd": "complete", "desc": log_desc("VDISK", "complete XJ380.img"), "pool": "console"},
    )
    n.build(
        ["installer.system.stage", "out/system-payload.pak"],
        "python_cmd",
        all_deps,
        variables={"cmd": "installer-system-stage", "desc": log_desc("STAGE", "installer system")},
    )
    n.build(
        ["installer.root.stage", "out/installer-root.pak"],
        "python_cmd",
        all_deps,
        variables={"cmd": "installer-root-stage", "desc": log_desc("STAGE", "installer root")},
    )
    n.build(
        ["installer.iso", "XJ380-installer.iso"],
        "python_cmd",
        ["installer.system.stage", "installer.root.stage"],
        variables={"cmd": "installer-iso", "desc": log_desc("ISO", "XJ380-installer.iso")},
    )
    n.build(["vmdk", "XJ380.vmdk"], "python_cmd", ["vdisk"], variables={"cmd": "vmdk", "desc": log_desc("VMDK", "XJ380.vmdk")})
    n.build("run", "python_cmd", ["vdisk"], variables={"cmd": "run", "desc": log_desc("RUN", "QEMU"), "pool": "console"})
    n.build("justrun", "python_cmd", variables={"cmd": "run", "desc": log_desc("RUN", "QEMU"), "pool": "console"})
    n.build("prepare", "python_cmd", variables={"cmd": "prepare", "desc": log_desc("PREP", "root")})
    n.build("installer.prepare", "python_cmd", variables={"cmd": "installer-prepare", "desc": log_desc("PREP", "installer root")})
    n.build("stage.selfhost-user", "python_cmd", variables={"cmd": "stage-selfhost-user", "desc": log_desc("STAGE", "selfhost user")})
    n.build("stage.linux-compat", "python_cmd", variables={"cmd": "stage-linux-compat", "desc": log_desc("STAGE", "linux compat")})
    n.build("format", "python_cmd", variables={"cmd": "format", "desc": log_desc("FMT", "sources")})
    n.build("check", "python_cmd", variables={"cmd": "check", "desc": log_desc("CHECK", "sources")})
    n.build("gen.clangd", "python_cmd", variables={"cmd": "gen-clangd", "desc": log_desc("GEN", "clangd")})
    n.build("size", "python_cmd", variables={"cmd": "size", "desc": log_desc("SIZE", "report")})
    n.build("check.tools", "python_cmd", variables={"cmd": "check-tools", "desc": log_desc("CHECK", "tools")})
    n.build("graph", "python_cmd", variables={"cmd": "graph", "desc": log_desc("GRAPH", "out/build-graph.dot")})
    n.build("clean.clangd", "shell_cmd", variables={"cmd": "rm -f .clangd boot/.clangd", "desc": log_desc("CLEAN", "clangd")})
    n.build("clean", "python_cmd", variables={"cmd": "clean", "desc": log_desc("CLEAN", "build outputs")})
    n.build("reconfigure", "regen", ["tools/gen_ninja.py", "tools/ninja_build.py"])
    n.line("default all")

    output = Path(args.out)
    output.write_text("\n".join(n.lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
