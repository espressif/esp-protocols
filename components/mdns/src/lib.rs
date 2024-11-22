// src/lib.rs

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

// // Other structs remain the same
// #[repr(C)]
// #[derive(Debug, Clone, Copy)]
// pub struct EspIp4Addr {
//     addr: u32,
// }
//
// #[repr(C)]
// #[derive(Debug, Clone, Copy)]
// pub struct EspIp6Addr {
//     addr: [u32; 4],
//     zone: u8,
// }
//
// #[repr(C)]
// #[derive(Debug, Clone, Copy)]
// pub struct EspIpAddr {
//     u_addr: EspIpUnion, // Union containing IPv4 or IPv6 address
//     addr_type: u8,
// }
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

//     fn set_callback2();
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
        parse_dns_response(&data_vec);        }
    }
}

fn parse_dns_response(data: &[u8]) {
    // Safe handling of the slice
    println!("Parsing DNS response with length: {}", data.len());

    parse_dns_response2(data);
    // Process the data (this will be safe, as `data` is a slice)
    // Example: You could convert the slice to a string, inspect it, or pass it to a DNS library
}

fn parse_dns_response2(data: &[u8]) -> Result<(), String> {
    println!("Parsing DNS response with length 2 : {}", data.len());
//     use dns_parser::Packet;
    let packet = Packet::parse(&data).unwrap();
    for answer in packet.answers {
        println!("{:?}", answer);
    }
//     match Message::from_vec(data) {
//         Ok(msg) => {
//             // Successful parsing
//             println!("Parsed DNS message successfully.");
//         }
//         Err(e) => {
//             // Detailed error message
//             eprintln!("Error parsing DNS message: {}", e);
//         }
//     }
    // Parse the response message
//     let msg = Message::from_vec(data).map_err(|e| e.to_string())?;
//     println!("Type: {}", msg.op_code().to_string());
//     // Check if the message is a response (opcode is Response)
//     if msg.op_code() != trust_dns_client::op::OpCode::Status {
//         return Err("Not a response message".to_string());
//     }
//
//     // Display the answer section (which should contain A record)
//     for answer in msg.answers() {
//         println!("Non-IP answer: {:?}", answer);
//         if let Some(ipv4_addr) = answer.rdata().to_ip_addr() {
//             println!("Resolved IP address: {}", ipv4_addr);
//         } else {
//             println!("Non-IP answer: {:?}", answer);
//         }
//     }

    Ok(())
}

use std::ffi::CString;


pub fn mdns_init() {
    println!("mdns_init called");
}

pub fn mdns_deinit() {
    println!("mdns_deinit called");
}

pub fn create_mdns_query() -> Vec<u8> {
    let query_name = "david-work.local"; // The domain you want to query
    let query_type = QueryType::A;       // Type A query for IPv4 address
    let query_class = QueryClass::IN;    // Class IN (Internet)

    // Create a new query with ID and recursion setting
    let mut builder = Builder::new_query(12345, true);

    // Add the question for "david-work.local"
    builder.add_question(query_name, false, query_type, query_class);

    // Build and return the query packet
    builder.build().unwrap_or_else(|x| x)
}

pub fn mdns_query_host_rust(name: &str) {
    let c_name = CString::new(name).expect("Failed to create CString");
//     unsafe {
//         mdns_query_host(c_name.as_ptr());
//     }
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

pub fn mdns_udp_pcb_write_rust(
    tcpip_if: MdnsIf,
    ip_protocol: MdnsIpProtocol,
    ip: EspIpAddr,
    port: u16,
    data: &[u8],
) -> usize {
    unsafe {
        _mdns_udp_pcb_write(
            tcpip_if,
            ip_protocol,
            &ip as *const EspIpAddr,
            port,
            data.as_ptr(),
            data.len(),
        )
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
