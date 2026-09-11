# Build and statically link the resvg C API from the pinned source submodule.
isEmpty(RESVG_SOURCE_ROOT): error(RESVG_SOURCE_ROOT must be set before including resvg.pri)
RESVG_C_API_DIR = $$RESVG_SOURCE_ROOT/crates/c-api
RESVG_MANIFEST = $$RESVG_C_API_DIR/Cargo.toml
RESVG_TARGET_DIR = $$clean_path($$shadowed($$RESVG_SOURCE_ROOT/..)/cargo-target)

CONFIG(debug, debug|release) {
    RESVG_PROFILE = debug
    RESVG_CARGO_FLAGS =
} else {
    RESVG_PROFILE = release
    RESVG_CARGO_FLAGS = --release
}

win32: RESVG_LIBRARY = $$RESVG_TARGET_DIR/$$RESVG_PROFILE/resvg.lib
unix: RESVG_LIBRARY = $$RESVG_TARGET_DIR/$$RESVG_PROFILE/libresvg.a

RESVG_CARGO = cargo
win32 {
    RESVG_RUSTUP_CARGO = $$(USERPROFILE)/.cargo/bin/cargo.exe
    exists($$RESVG_RUSTUP_CARGO): RESVG_CARGO = $$RESVG_RUSTUP_CARGO
}

resvg_c_api.target = $$RESVG_LIBRARY
resvg_c_api.commands = $$shell_quote($$shell_path($$RESVG_CARGO)) build --locked $$RESVG_CARGO_FLAGS --manifest-path $$shell_quote($$shell_path($$RESVG_MANIFEST)) --target-dir $$shell_quote($$shell_path($$RESVG_TARGET_DIR))
QMAKE_EXTRA_TARGETS += resvg_c_api
PRE_TARGETDEPS += $$RESVG_LIBRARY

INCLUDEPATH += $$RESVG_C_API_DIR
LIBS += -L$$shell_quote($$shell_path($$RESVG_TARGET_DIR/$$RESVG_PROFILE)) -lresvg

win32: LIBS += -lWs2_32 -lBcrypt -lUserenv -lNtdll
