use super::service::Service;
use std::thread;

use std::net::{UdpSocket, Ipv4Addr};
use socket2::{Socket, Domain, Type, Protocol};
use nix::unistd::{pipe, read, write, close};
use nix::sys::select::{select, FdSet};
use nix::sys::time::TimeVal;
use std::os::fd::AsRawFd;

enum Action {
    Action1,
    Action2,
    Action3,
}

pub struct NativeService
{
    handle: Option<thread::JoinHandle<()>>,
    socket: UdpSocket,
    write_fd: i32,
    callback: fn(&[u8]),
}

fn create_multicast_socket() -> UdpSocket {
    let addr: std::net::SocketAddr = "0.0.0.0:5353".parse().unwrap();

    let socket = Socket::new(Domain::IPV4, Type::DGRAM, Some(Protocol::UDP)).unwrap();
    socket.set_reuse_address(true).unwrap();
    socket.bind(&addr.into()).unwrap();

    let multicast_addr = Ipv4Addr::new(224, 0, 0, 251);
    let interface = Ipv4Addr::new(0, 0, 0, 0);
    socket.join_multicast_v4(&multicast_addr, &interface).unwrap();

    socket.into()
}

impl NativeService
{
    fn new(cb: fn(&[u8])) -> Self {
        let socket = create_multicast_socket();
        let (read_fd, w_fd) = pipe().expect("Failed to create pipe");
        let local_cb = cb;
        let local_socket = socket.try_clone().unwrap();

        let handle = thread::spawn(move || {

            loop {
                let socket_fd = local_socket.as_raw_fd();
                let mut read_fds = FdSet::new();
                read_fds.insert(socket_fd);
                read_fds.insert(read_fd);


                let mut timeout = TimeVal::new(0, 100_000);

                match select(read_fd.max(socket_fd) + 1, Some(&mut read_fds), None, None, Some(&mut timeout)) {
                    Ok(0) => println!("ThreadHousekeeper: Performing housekeeping tasks"),
                    Ok(_) => {
                        if read_fds.contains(socket_fd) {
                            // let mut buf: [MaybeUninit<u8>; 1500] = unsafe { MaybeUninit::uninit().assume_init() };
                            // let mut buf: [u8; 1500];
                            let mut buf = vec![0u8; 1500]; // Create a buffer using a vector
                            // let sock =  unsafe { Socket::from_raw_fd(socket_fd) };
                            match local_socket.recv_from(&mut buf) {
                                Ok((size, addr)) => {
                                    // Convert the buffer from MaybeUninit to a regular slice of u8
                                    // let buf = unsafe {
                                    //     std::slice::from_raw_parts_mut(buf.as_mut_ptr() as *mut u8, size)
                                    // };
                                    // println!("Received {} bytes from {:?}: {:?}", size, addr, buf);
                                    local_cb(&buf[..size]);
                                }
                                Err(e) => println!("Error reading from socket: {:?}", e),
                            }
                        }

                        if read_fds.contains(read_fd) {
                            let mut buf = [0u8; 10];
                            match read(read_fd, &mut buf) {
                                Ok(size) => {
                                    println!("{}", size);
                                    if size == 0 {
                                        break;
                                    }
                                }
                                Err(e) => println!("Error reading from socket: {:?}", e),
                            }
                        }
                    }
                    Err(e) => {
                        println!("Error in select(): {:?}", e);
                        break;
                    }
                }
            }

            // close(read_fd).expect("Failed to close read end of pipe");
        });

        NativeService {
            handle: Some(handle),
            socket,
            write_fd: w_fd,
            callback: cb
        }
    }
}

impl Service for NativeService {

    fn init(cb: fn(&[u8])) -> Box<Self> {
        println!("Native: Initializing.");
        Box::new(NativeService::new(cb))
    }

    fn send(&self, packet: Vec<u8>) {
        let destination: std::net::SocketAddr = "224.0.0.251:5353".parse().unwrap();
        self.socket.send_to(&packet, &destination)
                    .expect("Failed to send mDNS query");
    }

    fn action1(&self) {
        let buf: [u8; 1] = [0x01];
        // self.socket.send(&buf).unwrap();
        write(self.write_fd, &buf).unwrap();
        // if let Some(tx) = &self.tx {
        //     tx.send(Action::Action1).unwrap();
        // }
    }

    fn action2(&self) {
        // if let Some(tx) = &self.tx {
        //     tx.send(Action::Action2).unwrap();
        // }
    }

    fn action3(&self) {
        // if let Some(tx) = &self.tx {
        //     tx.send(Action::Action3).unwrap();
        // }
    }

    fn deinit(self: Box<Self>) {
        println!("DEINIT called");
        // if let Some(tx) = self.tx {
        //     drop(tx);
        // }
        close(self.write_fd).unwrap();

        if let Some(handle) = self.handle {
            handle.join().unwrap();
            println!("NativeService: Deinitialized");
        }
        println!("DEINIT done...");

    }
}
