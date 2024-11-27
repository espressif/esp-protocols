// src/lib.rs
mod service;
use service::{Service, NativeService, CService};

extern crate libc;
// extern crate trust_dns;
// use crate trust_dns;
use std::fmt;
use std::os::raw::c_char;
use std::ffi::CStr;
use std::fmt::Write;  // For formatting strings
// use trust_dns_client::op::{Message, Query};
// use trust_dns_client::rr::{RecordType, Name};
// use trust_dns_client::serialize::binary::{BinDecodable, BinEncoder};
use std::net::Ipv4Addr;
use dns_parser::{Builder, QueryClass, QueryType, Packet};



#[cfg(not(feature = "ffi"))]
fn build_info() {
    println!("Default build");
}

#[cfg(not(feature = "ffi"))]
fn create_service(cb: fn(&[u8])) -> Box<dyn Service> {
    NativeService::init(cb)
}


#[cfg(feature = "ffi")]
fn build_info() {
    println!("FFI build");
}

#[cfg(feature = "ffi")]
fn create_service(cb: fn(&[u8])) -> Box<dyn Service> {
    CService::init(cb)
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EspIp4Addr {
    pub addr: u32, // IPv4 address as a 32-bit integer
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EspIp6Addr {
    pub addr: [u32; 4], // IPv6 address as an array of 4 32-bit integers
    pub zone: u8,       // Zone ID
}

#[repr(C)]
pub union EspIpUnion {
    pub ip4: EspIp4Addr,
    pub ip6: EspIp6Addr,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EspIpAddr {
    pub u_addr: EspIpUnion, // Union containing IPv4 or IPv6 address
    pub addr_type: u8,
}

// Manual implementation of Debug for the union
impl fmt::Debug for EspIpUnion {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Safely access and format the union's members for debugging
        unsafe {
            write!(f, "EspIpUnion {{ ip4: {:?}, ip6: {:?} }}", self.ip4, self.ip6)
        }
    }
}

// Manual implementation of Clone for the union
impl Clone for EspIpUnion {
    fn clone(&self) -> Self {
        // Safety: Assuming the union contains valid data in either `ip4` or `ip6`
        unsafe {
            EspIpUnion {
                ip4: self.ip4.clone(),
            }
        }
    }
}

// Manual implementation of Copy for the union
impl Copy for EspIpUnion {}

// Address type definitions
pub const ESP_IPADDR_TYPE_V4: u8 = 0;
pub const ESP_IPADDR_TYPE_V6: u8 = 6;
pub const ESP_IPADDR_TYPE_ANY: u8 = 46;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub enum MdnsIf {
    Netif0 = 0,
    // Add more as needed based on the actual C enum definition
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub enum MdnsIpProtocol {
    Ip4 = 0,
    Ip6 = 1,
}

type EspErr = i32;

extern "C" {
    fn _mdns_pcb_init(tcpip_if: MdnsIf, ip_protocol: MdnsIpProtocol) -> EspErr;
    fn _mdns_udp_pcb_write(
        tcpip_if: MdnsIf,
        ip_protocol: MdnsIpProtocol,
        ip: *const EspIpAddr,
        port: u16,
        data: *const u8,
        len: usize,
    ) -> usize;
    fn _mdns_pcb_deinit(tcpip_if: MdnsIf, ip_protocol: MdnsIpProtocol) -> EspErr;
    fn set_callback(callback: extern "C" fn(*const u8, usize));
}

extern "C" fn rust_callback(data: *const u8, len: usize)
{
    println!("Received len: {}", len);
    unsafe {
        // Ensure that the data pointer is valid
        if !data.is_null() {
            // Create a Vec<u8> from the raw pointer and length
            let data_vec = std::slice::from_raw_parts(data, len).to_vec();

            // Now call the safe parser function with the Vec<u8>
            parse_dns_response(&data_vec).unwrap();
        }
    }
}

fn parse_dns_response(data: &[u8]) -> Result<(), String> {
    println!("Parsing DNS response with length 2 : {}", data.len());
    let packet = Packet::parse(&data).unwrap();
    for answer in packet.answers {
        println!("{:?}", answer);
    }
    for question in packet.questions {
        println!("{:?}", question);
    }
    Ok(())
}

use std::ffi::CString;
use std::thread;
use std::time::Duration;
use lazy_static::lazy_static;
use std::sync::{Arc, Mutex};

lazy_static! {
    static ref SERVICE: Arc<Mutex<Option<Box<dyn Service>>>> = Arc::new(Mutex::new(None));
}

fn read_cb(vec: &[u8]) {
    println!("Received {:?}", vec);
    parse_dns_response(vec).unwrap();
}

pub fn mdns_init() {
    build_info();
    let mut service_guard = SERVICE.lock().unwrap();
    if service_guard.is_none() {
        // Initialize the service only if it hasn't been initialized
        *service_guard = Some(create_service(read_cb));
    }
    // let service: Box<dyn Service> = create_service(read_cb);
    // service.action1();
    // let packet: [u8; 34] = [
    //     0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x64, 0x61,
    //     0x76, 0x69, 0x64, 0x2d, 0x77, 0x6f, 0x72, 0x6b, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00,
    //     0x00, 0x01, 0x00, 0x01,
    // ];
    // service.send(packet.to_vec());
    // thread::sleep(Duration::from_millis(500));
    // service.deinit();
    println!("mdns_init called");
}

pub fn mdns_deinit() {
    let mut service_guard = SERVICE.lock().unwrap();
    if let Some(service) = service_guard.take() {
        service.deinit();
    }
    println!("mdns_deinit called");
}

fn create_a_query(name: &str) -> Vec<u8> {
    let query_type = QueryType::A;       // Type A query for IPv4 address
    let query_class = QueryClass::IN;    // Class IN (Internet)

    // Create a new query with ID and recursion setting
    let mut builder = Builder::new_query(0x5555, true);

    // Add the question for "david-work.local"
    builder.add_question(name, false, query_type, query_class);

    // Build and return the query packet
    builder.build().unwrap_or_else(|x| x)
}

pub fn mdns_query(name: &str) {
    let service_guard = SERVICE.lock().unwrap();
    if let Some(service) = &*service_guard {
        let packet = create_a_query(name);
        service.send(packet);
    } else {
        println!("Service not initialized");
    }
}

pub fn mdns_pcb_init_rust(tcpip_if: MdnsIf, ip_protocol: MdnsIpProtocol) -> Result<(), EspErr> {
    unsafe {
        set_callback(rust_callback);
//         set_callback2();
    }

    let err = unsafe { _mdns_pcb_init(tcpip_if, ip_protocol) };
    if err == 0 {
        Ok(())
    } else {
        Err(err)
    }
}



pub fn mdns_pcb_deinit_rust(tcpip_if: MdnsIf, ip_protocol: MdnsIpProtocol) -> Result<(), EspErr> {
    let err = unsafe { _mdns_pcb_deinit(tcpip_if, ip_protocol) };
    if err == 0 {
        Ok(())
    } else {
        Err(err)
    }
}
