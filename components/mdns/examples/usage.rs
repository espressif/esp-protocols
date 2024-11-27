// examples/basic_usage.rs

use mdns::*;
use std::thread;
use std::time::Duration;
use std::net::{UdpSocket, Ipv4Addr};
use socket2::{Socket, Domain, Type, Protocol};



fn test_mdns() {
    let ip4 = EspIpAddr {
        u_addr: EspIpUnion {
            ip4: EspIp4Addr {
                addr: u32::from_le_bytes([224, 0, 0, 251]),
            },
        },
        addr_type: ESP_IPADDR_TYPE_V4,
    };

//     let query_packet: [u8; 34] = [
//         0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x64, 0x61,
//         0x76, 0x69, 0x64, 0x2d, 0x77, 0x6f, 0x72, 0x6b, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00,
//         0x00, 0x01, 0x00, 0x01,
//     ];

    if let Err(err) = mdns_pcb_init_rust(MdnsIf::Netif0, MdnsIpProtocol::Ip4) {
        eprintln!("Failed to initialize mDNS PCB: {}", err);
        return;
    }

    // let query_packet = create_mdns_query();
    // println!("{:?}", query_packet);



    // let len = mdns_udp_pcb_write_rust(MdnsIf::Netif0, MdnsIpProtocol::Ip4, ip4, 5353, &query_packet);
    // println!("Bytes sent: {}", len);

    thread::sleep(Duration::from_millis(500));

    if let Err(err) = mdns_pcb_deinit_rust(MdnsIf::Netif0, MdnsIpProtocol::Ip4) {
        eprintln!("Failed to deinitialize mDNS PCB: {}", err);
    }
}


fn main() {
    // Initialize mDNS
    mdns::mdns_init();

//     // Query for a specific host
//     mdns::mdns_query_host_rust("example.local");
    mdns::mdns_query("david-work.local");
    thread::sleep(Duration::from_millis(500));
    // Deinitialize mDNS
    mdns::mdns_deinit();


    // test_mdns();

//     let result = mdns::mdns_pcb_init_rust(mdns::MdnsIf::Netif0, mdns::MdnsIpProtocol::Ip4);
//     match result {
//         Ok(_) => println!("mdns_pcb_init succeeded"),
//         Err(err) => eprintln!("mdns_pcb_init failed with error code: {}", err),
//     }
}
