use dns_parser::{Builder, QueryClass, QueryType, Packet};
use std::sync::{Arc, Condvar, Mutex};
use std::collections::HashMap;
use std::time::{Duration, Instant};

#[derive(Debug)]
pub struct Query {
    pub name: String,
    pub service: String,
    pub proto: String,
    pub query_type: QueryType,
    pub unicast: bool,
    pub timeout: Duration,
    pub added_at: Instant,
    pub packet: Vec<u8>,
    pub id: usize,
}

pub struct Querier {
    queries: Vec<Query>,
    pub(crate) completed_queries: Arc<(Mutex<HashMap<usize, bool>>, Condvar)>, // Shared state for query completion
}

impl Querier {
    pub fn new() -> Self {
        Self {
            queries: Vec::new(),
            completed_queries: Arc::new((Mutex::new(HashMap::new()), Condvar::new())),
        }
    }

    pub fn add(
        &mut self,
        name: String,
        service: String,
        proto: String,
        query_type: QueryType,
        unicast: bool,
        timeout: Duration,
    ) -> usize {
        let id = self.queries.len();
        let query = Query {
            name: name.clone(),
            service,
            proto,
            query_type,
            unicast,
            timeout,
            added_at: Instant::now(),
            packet: create_a_query(&name),
            id: id.clone()
        };
        self.queries.push(query);
        self.completed_queries.0.lock().unwrap().insert(id, false); // Mark as incomplete
        id
    }
    pub fn parse(&mut self, data: &[u8]) -> Result<(), String> {
        println!("Parsing DNS response with length 2 : {}", data.len());
        let packet = Packet::parse(data).unwrap();
        for answer in packet.answers {
            println!("ANSWER:");
            println!("{:?}", answer);
            let name = answer.name.to_string();
            let mut completed_queries = vec![];
            self.queries.retain(|query| {
                if query.name == name {
                    println!("ANSWER: {:?}", answer.data);
                    completed_queries.push(query.id); // Track for completion
                    false
                }
                else { true }
            });
            let (lock, cvar) = &*self.completed_queries;
            let mut completed = lock.lock().unwrap();
            for query_id in completed_queries {
                if let Some(entry) = completed.get_mut(&query_id) {
                    *entry = true;
                }
            }
            cvar.notify_all();

        }
        for question in packet.questions {
            println!("{:?}", question);
        }
        Ok(())

    }

    pub fn process(&mut self) -> Option<Vec<u8>> {
        let now = Instant::now();
        let mut packet_to_send: Option<Vec<u8>> = None;

        // Collect IDs of timed-out queries to mark them as complete
        let mut timed_out_queries = vec![];
        self.queries.retain(|query| {
            let elapsed = now.duration_since(query.added_at);
            if elapsed > query.timeout {
                timed_out_queries.push(query.id); // Track for completion
                false // Remove the query
            } else {
                packet_to_send = Some(query.packet.clone());
                true // Keep the query
            }
        });

        // Mark timed-out queries as complete and notify waiting threads
        let (lock, cvar) = &*self.completed_queries;
        let mut completed = lock.lock().unwrap();
        for query_id in timed_out_queries {
            if let Some(entry) = completed.get_mut(&query_id) {
                *entry = true;
            }
        }
        cvar.notify_all();
        println!("Processing... query");

        packet_to_send
    }
    pub fn wait(&self, id: usize) -> Result<(), &'static str> {
        let (lock, cvar) = &*self.completed_queries;

        // Wait until the query is marked as complete or timeout expires
        let mut completed = lock.lock().unwrap();
        while !completed.get(&id).copied().unwrap_or(false) {
            completed = cvar.wait(completed).unwrap();
        }
        Ok(())
    }

    fn mark_query_as_complete(&self, query: &Query) {
        let (lock, cvar) = &*self.completed_queries;
        let mut completed = lock.lock().unwrap();
        if let Some(entry) = completed.get_mut(&(self.queries.len() - 1)) {
            *entry = true;
        }
        cvar.notify_all();
    }
}

fn create_a_query(name: &str) -> Vec<u8> {
    let query_type = QueryType::A; // Type A query for IPv4 address
    let query_class = QueryClass::IN; // Class IN (Internet)

    let mut builder = Builder::new_query(0, true);
    builder.add_question(name, false, query_type, query_class);
    builder.build().unwrap_or_else(|x| x)
}
