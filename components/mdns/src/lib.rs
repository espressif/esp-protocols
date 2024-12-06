// src/lib.rs
mod service;
use service::{Service, NativeService, CService};
use lazy_static::lazy_static;
use std::sync::{Arc, Mutex};
use dns_parser::{Builder, QueryClass, QueryType, Packet};
use std::time::{Duration, Instant};


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


// pub trait Querier: Send + Sync {
//     fn init() -> Box<Self>
//     where
//         Self: Sized;

//     fn deinit(self: Box<Self>);
// }


#[derive(Debug)]
pub struct Query {
    pub name: String,
    pub service: String,
    pub proto: String,
    pub query_type: QueryType,
    pub unicast: bool,
    pub timeout: Duration,
    pub added_at: Instant, // To track when the query was added
}

pub struct Querier {
    queries: Vec<Query>,
}

fn create_querier() -> Box<dyn Querier> {
    NativeService::init(cb)
}

impl Querier {
    pub fn new() -> Self {
        Self {
            queries: Vec::new(),
        }
    }

    pub fn init(&mut self) {
        println!("Querier initialized");
    }

    pub fn deinit(&mut self) {
        self.queries.clear();
        println!("Querier deinitialized");
    }

    pub fn add(
        &mut self,
        name: String,
        service: String,
        proto: String,
        query_type: QueryType,
        unicast: bool,
        timeout: Duration,
        // semaphore: Option<tokio::sync::Semaphore>,
    ) -> usize {
        let query = Query {
            name,
            service,
            proto,
            query_type,
            unicast,
            timeout,
            // semaphore,
            added_at: Instant::now(),
        };
        self.queries.push(query);
        self.queries.len() - 1 // Return the ID (index of the query)
    }

    pub fn process(&mut self) {
        let now = Instant::now();
        self.queries.retain(|query| {
            let elapsed = now.duration_since(query.added_at);
            if elapsed > query.timeout {
                println!("Query timed out: {:?}", query);
                // if let Some(semaphore) = &query.semaphore {
                //     semaphore.add_permits(1); // Release semaphore if waiting
                // }
                false // Remove the query
            } else {
                println!("Processing query: {:?}", query);
                // Implement retry logic here if needed
                true // Keep the query
            }
        });
    }

    pub async fn wait(&mut self, id: usize) -> Result<(), &'static str> {
        if let Some(query) = self.queries.get_mut(id) {
            Ok(())
            // if let Some(semaphore) = &query.semaphore {
            //     semaphore.acquire().await.unwrap(); // Block until the semaphore is released
            //     Ok(())
            // } else {
            //     Err("No semaphore set for this query")
            // }
        } else {
            Err("Invalid query ID")
        }
    }
}

struct Objects {
    service: Option<Box<dyn Service>>,
    // responder: Option<Box<dyn Responder>>,
    querier: Option<Querier>,
}

// lazy_static! {
//     static ref SERVICE: Arc<Mutex<Option<Box<dyn Service>>>> = Arc::new(Mutex::new(None));
// }

lazy_static! {
    static ref SERVER: Arc<Mutex<Objects>> = Arc::new(Mutex::new(Objects {
        service: None,
        querier: None,
    }));
}

fn read_cb(vec: &[u8]) {
    println!("Received {:?}", vec);
    parse_dns_response(vec).unwrap();
}

pub fn mdns_init() {
    build_info();
    let mut service_guard = SERVER.lock().unwrap();
    if service_guard.service.is_none() {
        // Initialize the service only if it hasn't been initialized
        service_guard.service = Some(create_service(read_cb));
    }
    if service_guard.querier.is_none() {
        // Initialize the service only if it hasn't been initialized
        service_guard.querier = Some(init());
    }

    println!("mdns_init called");
}

pub fn mdns_deinit() {
    let mut service_guard = SERVER.lock().unwrap();
    if let Some(service) = service_guard.service.take() {
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
    let service_guard = SERVER.lock().unwrap();
    if let Some(service) = &service_guard.service {
        let packet = create_a_query(name);
        service.send(packet);
    } else {
        println!("Service not initialized");
    }
}
