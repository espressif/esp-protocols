use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use serde::Deserialize;

#[derive(Debug, Deserialize)]
struct CompileCommand {
    directory: String,
    command: String,
    file: String,
}

fn main() {

    println!("cargo:rerun-if-env-changed=COMPILE_COMMANDS_DIR");
    // Get the directory for compile_commands.json from an environment variable
    let compile_commands_dir = match env::var("COMPILE_COMMANDS_DIR") {
        Ok(dir) => dir,
        Err(_) => {
            // If the environment variable is not defined, return early
            println!("COMPILE_COMMANDS_DIR not set, skipping custom build.");
            // this is a native build
            // println!("cargo:rustc-cfg=native");
            return;
        }
    };

    // building with FFI of mdns_networking
    println!("COMPILE_COMMANDS_DIR set, enabling FFI feature.");
    println!("cargo:rustc-cfg=feature=\"ffi\"");
    // Construct the path to the compile_commands.json file
    let compile_commands_path = Path::new(&compile_commands_dir).join("compile_commands.json");

    // Parse compile_commands.json
    let compile_commands: Vec<CompileCommand> = {
        let data = fs::read_to_string(&compile_commands_path)
            .expect("Failed to read compile_commands.json");
        serde_json::from_str(&data)
            .expect("Failed to parse compile_commands.json")
    };

    // Directory of compile_commands.json, used to resolve relative paths
    let base_dir = compile_commands_path
        .parent()
        .expect("Failed to get base directory of compile_commands.json");

    // List of C files to compile (only base names)
    let files_to_compile = vec![
        "mdns_networking_socket.c",
        "log_write.c",
        "log_timestamp.c",
        "esp_netif_linux.c",
        "freertos_linux.c",
        "tag_log_level.c",
        "log_linked_list.c",
        "log_lock.c",
        "log_level.c",
        "log_binary_heap.c",
        "esp_system_linux2.c",
        "heap_caps_linux.c",
        "mdns_stub.c",
        "log_buffers.c",
        "util.c"
    ];

    // Initialize the build
    let mut build = cc::Build::new();

for file in &files_to_compile {
    // Extract the base name from `file` for comparison
    let target_base_name = Path::new(file)
        .file_name()
        .expect("Failed to extract base name from target file")
        .to_str()
        .expect("Target file name is not valid UTF-8");

    // Find the entry in compile_commands.json by matching the base name
    let cmd = compile_commands.iter()
        .find(|entry| {
            let full_path = Path::new(&entry.directory).join(&entry.file); // Resolve relative paths
            if let Some(base_name) = full_path.file_name().and_then(|name| name.to_str()) {
//                 println!("Checking file: {} against {}", base_name, target_base_name); // Debug information
                base_name == target_base_name
            } else {
                false
            }
        })
        .unwrap_or_else(|| panic!("{} not found in compile_commands.json", target_base_name));

    // Add the file to the build
    build.file(&cmd.file);

    // Parse flags and include paths from the command
    for part in cmd.command.split_whitespace() {
        if part.starts_with("-I") {
            // Handle include directories
            let include_path = &part[2..];
            let full_include_path = if Path::new(include_path).is_relative() {
                base_dir.join(include_path).canonicalize()
                    .expect("Failed to resolve relative include path")
            } else {
                PathBuf::from(include_path)
            };
            build.include(full_include_path);
        } else if part.starts_with("-D") || part.starts_with("-std") {
            // Add other compilation flags
            build.flag(part);
        }
    }
}




    // Compile with the gathered information
    build.compile("mdns");
}
