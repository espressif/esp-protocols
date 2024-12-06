// examples/basic_usage.rs

use mdns::*;
use std::thread;
use std::time::Duration;


fn main() {
    // Initialize mDNS
    mdns_init();

    mdns_query("david-work.local");
    thread::sleep(Duration::from_millis(500));
    // Deinitialize mDNS
    mdns_deinit();
}
