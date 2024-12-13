// examples/basic_usage.rs

// use std::process::Termination;
use mdns::*;
use std::thread;
use std::time::Duration;
// use libc::__c_anonymous_xsk_tx_metadata_union;

fn main() {
    // Initialize mDNS
    mdns_init();

    mdns_query("david-work.local");
    thread::sleep(Duration::from_millis(1500));
    // Deinitialize mDNS
    mdns_deinit();
}
