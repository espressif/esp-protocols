pub trait Service: Send + Sync {
    fn init(cb: fn(&[u8])) -> Box<Self>
    where
        Self: Sized;

    fn send(&self, packet: Vec<u8>);
    fn action1(&self);
    fn action2(&self);
    fn action3(&self);

    fn deinit(self: Box<Self>);
}
