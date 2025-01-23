use libc::AT_NULL;

use super::service::Service;
use std::fmt;

pub struct CService
{
    callback: fn(&[u8])
}

impl CService {
    fn new(cb: fn(&[u8])) -> Self {
        CService {
            callback: cb
        }
    }
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

static mut CALLBACK: Option<fn(&[u8])> = None;

extern "C" fn rust_callback(data: *const u8, len: usize)
{
    println!("Received len: {}", len);
    unsafe {
        // Ensure that the data pointer is valid
        if !data.is_null() {
            // Create a Vec<u8> from the raw pointer and length
            let data_vec = std::slice::from_raw_parts(data, len).to_vec();
            if let Some(cb) = CALLBACK {
                cb(&data_vec);
            }
            // Now call the safe parser function with the Vec<u8>
            // parse_dns_response(&data_vec);
        }
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

impl Service for CService {


    fn init(cb:fn(&[u8])) -> Box<Self> {
        println!("FfiHousekeeper: Initializing.");
        unsafe {
            set_callback(rust_callback);
            CALLBACK = Some(cb);
        }
        let _ = unsafe { _mdns_pcb_init(MdnsIf::Netif0, MdnsIpProtocol::Ip4) };


        Box::new(CService::new(cb))
    }

    fn send(&self, packet: Vec<u8>) {
        let ip4 = EspIpAddr {
            u_addr: EspIpUnion {
                ip4: EspIp4Addr {
                    addr: u32::from_le_bytes([224, 0, 0, 251]),
                },
            },
            addr_type: ESP_IPADDR_TYPE_V4,
        };
        println!("FfiHousekeeper: Sending");
        let len = mdns_udp_pcb_write_rust(MdnsIf::Netif0, MdnsIpProtocol::Ip4, ip4, 5353, &packet);
        println!("Bytes sent: {}", len);
    }

    fn action1(&self) {
        println!("FfiHousekeeper: Performing Action1.");
    }

    fn action2(&self) {
        println!("FfiHousekeeper: Performing Action2.");
    }

    fn action3(&self) {
        println!("FfiHousekeeper: Performing Action3.");
    }

    fn deinit(self: Box<Self>) {
        let _ = unsafe { _mdns_pcb_deinit(MdnsIf::Netif0, MdnsIpProtocol::Ip4) };
        println!("FfiHousekeeper: Deinitializing.");
    }

}
