mod service; // Import the trait definition
mod native; // Thread-based implementation
mod ffi; // FFI-based implementation

pub use service::Service; // Expose the trait
pub use native::NativeService;
pub use ffi::CService;
// pub use thread::ThreadHousekeeper; // Expose the thread-based implementation
// pub use ffi::FfiHousekeeper; // Expose the FFI-based implementation
