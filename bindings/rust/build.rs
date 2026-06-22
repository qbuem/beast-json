fn main() {
    let mut build = cxx_build::bridge("src/lib.rs");

    build.file("src/qbuem_rust_shim.cpp")
         .include("src")            // Path to shim headers
         .include("../../include") // Path to qbuem_json headers
         .flag_if_supported("-std=c++20")
         // We emit the link directive ourselves (whole-archive) below, so silence
         // cc's own `cargo:rustc-link-lib=static=` to avoid a duplicate include.
         .cargo_metadata(false)
         .compile("qbuem_rust_shim");

    // Force the ENTIRE shim archive into the link. The cxx bridge trampolines
    // (`qbuem$rust$cxxbridge1$NNN$*`) and the UniquePtr glue
    // (`cxxbridge1$unique_ptr$...`) are C++ definitions only *referenced* from the
    // Rust rlib. A single-pass linker (rust-lld on Linux) with --gc-sections pulls
    // the archive in by command-line order and then drops those objects, producing
    // "undefined symbol" link failures for the bench. whole-archive keeps every
    // object regardless of order or section GC. (`cc::Build::compile` emits a plain
    // `static=` lib, which links on local macOS ld64 but not under the stricter CI
    // linkers — hence the explicit modifier here.)
    let out_dir = std::env::var("OUT_DIR").expect("OUT_DIR not set by cargo");
    println!("cargo:rustc-link-search=native={out_dir}");
    println!("cargo:rustc-link-lib=static:+whole-archive=qbuem_rust_shim");

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/qbuem_rust_shim.cpp");
    println!("cargo:rerun-if-changed=src/qbuem_rust_shim.hpp");
}
