#include <iostream>
#include "asio.hpp"
#include "boost/bind.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

class printer
{
public:
  printer(asio::demuxer& d)
    : dispatcher_(d),
      timer1_(d, boost::posix_time::seconds(1)),
      timer2_(d, boost::posix_time::seconds(1)),
      count_(0)
  {
    timer1_.async_wait(dispatcher_.wrap(boost::bind(&printer::print1, this)));
    timer2_.async_wait(dispatcher_.wrap(boost::bind(&printer::print2, this)));
  }

  ~printer()
  {
    std::cout << "Final count is " << count_ << "\n";
  }

  void print1()
  {
    if (count_ < 10)
    {
      std::cout << "Timer 1: " << count_ << "\n";
      ++count_;

      timer1_.expires_at(timer1_.expires_at() + boost::posix_time::seconds(1));
      timer1_.async_wait(dispatcher_.wrap(boost::bind(&printer::print1, this)));
    }
  }

  void print2()
  {
    if (count_ < 10)
    {
      std::cout << "Timer 2: " << count_ << "\n";
      ++count_;

      timer2_.expires_at(timer2_.expires_at() + boost::posix_time::seconds(1));
      timer2_.async_wait(dispatcher_.wrap(boost::bind(&printer::print2, this)));
    }
  }

private:
  asio::locking_dispatcher dispatcher_;
  asio::deadline_timer timer1_;
  asio::deadline_timer timer2_;
  int count_;
};

int main()
{
  asio::demuxer d;
  printer p(d);
  asio::thread t(boost::bind(&asio::demuxer::run, &d));
  d.run();
  t.join();

  return 0;
}
