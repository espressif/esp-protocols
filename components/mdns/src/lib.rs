mod service;
mod querier;

use service::{Service, NativeService, CService};
use querier::Querier;

use lazy_static::lazy_static;
use std::sync::{Arc, Mutex};
use dns_parser::{Builder, QueryClass, QueryType, Packet};
use std::time::Duration;

lazy_static! {
    static ref SERVER: Arc<Mutex<Objects>> = Arc::new(Mutex::new(Objects {
        service: None,
        querier: None,
    }));
}

struct Objects {
    service: Option<Box<dyn Service>>,
    querier: Option<Querier>,
}

fn build_info() {
    #[cfg(not(feature = "ffi"))]
    {
        println!("Default build");
    }
    #[cfg(feature = "ffi")]
    {
        println!("FFI build");
    }
}

fn create_service(cb: fn(&[u8])) -> Box<dyn Service> {
    #[cfg(not(feature = "ffi"))]
    {
        NativeService::init(cb)
    }
    #[cfg(feature = "ffi")]
    {
        CService::init(cb)
    }
}

fn read_cb(vec: &[u8]) {
    if vec.len() == 0 {
        let mut service_guard = SERVER.lock().unwrap();
        if let Some(querier) = &mut service_guard.querier {
            println!("querier process {:?}", vec);
            let packet = querier.process();
            if packet.is_some() {
                if let Some(service) = &service_guard.service {
                    service.send(packet.unwrap());
                }
            }
        }
    } else {
        println!("Received {:?}", vec);
        let mut service_guard = SERVER.lock().unwrap();
        if let Some(querier) = &mut service_guard.querier {
            querier.parse(&vec).expect("Failed to parse..");
        }
        // parse_dns_response(vec).unwrap();
    }
}

fn parse_dns_response(data: &[u8]) -> Result<(), String> {
    println!("Parsing DNS response with length 2 : {}", data.len());
    let packet = Packet::parse(data).unwrap();
    for answer in packet.answers {
        println!("ANSWER:");
        println!("{:?}", answer);
    }
    for question in packet.questions {
        println!("QUESTION:");
        println!("{:?}", question);
    }
    Ok(())
}

pub fn mdns_init() {
    build_info();
    let mut service_guard = SERVER.lock().unwrap();
    if service_guard.service.is_none() {
        service_guard.service = Some(create_service(read_cb));
    }
    if service_guard.querier.is_none() {
        service_guard.querier = Some(Querier::new());
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

/*
pub fn mdns_query(name: &str) {
    let mut service_guard = SERVER.lock().unwrap();

    if let Some(querier) = &mut service_guard.querier {
        let timeout = Duration::from_secs(5);
        let query_id = querier.add(
            name.to_string(),
            "".to_string(),
            "_http._tcp".to_string(),
            QueryType::A,
            false,
            timeout,
        );
        querier.wait(query_id).await.unwrap();
        println!("Query added with ID: {}", query_id);
    }

}

 */

pub fn mdns_query(name: &str) {
    // Lock the server to access the querier
    let query_id;
    let querier_cvar;

    {
        let mut service_guard = SERVER.lock().unwrap();
        if let Some(querier) = &mut service_guard.querier {
            let timeout = Duration::from_secs(1);
            query_id = querier.add(
                name.to_string(),
                "".to_string(),
                "_http._tcp".to_string(),
                QueryType::A,
                false,
                timeout,
            );
            querier_cvar = querier.completed_queries.clone(); // Clone the Arc<Mutex> pair
        } else {
            println!("No querier available");
            return;
        }
    } // Release the SERVER lock here

    // Wait for the query to complete
    let (lock, cvar) = &*querier_cvar;
    let mut completed = lock.lock().unwrap();
    while !completed.get(&query_id).copied().unwrap_or(false) {
        let result = cvar.wait_timeout(completed, Duration::from_secs(5)).unwrap();
        completed = result.0; // Update the lock guard
        if result.1.timed_out() {
            println!("Query timed out: ID {}", query_id);
            return;
        }
    }

    println!("Query completed!!! ID {}", query_id);
}
