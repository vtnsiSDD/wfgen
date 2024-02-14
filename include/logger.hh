#ifndef LOGGER_HH_INCLUDED
#define LOGGER_HH_INCLUDED

#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>
#include <zmq.hpp>
#include <vector>
#include <map>
#include <condition_variable>
#include <functional>

typedef enum _payload_type{
  CHAR,
  UCHAR,
  CHAR16,
  CHAR32,
  WCHAR
} payload_t;

typedef struct{
  double timestamp;
  payload_t type;
  uint64_t length;
  void *buffer;
} payload;

uint16_t get_payload_itemsize(payload_t &type);
size_t get_sizeof_payload(payload &p);
uint8_t serialize_payload(void **s, size_t *l, payload &p);
uint8_t deserialize_payload(payload *p, void **s, size_t &l);
void destroy_payload(payload **p);
void reset_payload(payload &p, uint8_t own_buffer=0);
void print_payload_stats(payload &p);

class logger_server;
class logger_client;


namespace logger{
  class null_buff : public std::streambuf{
   public:
    int overflow(int c) { return c; }
  };
  enum level_t{
    OFF,
    CRITICAL,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    INVALID
  };
  typedef std::map<level_t, const char*> level_map_t;
  extern level_map_t level_map;
  //     {OFF,       "OFF"},
  //     {CRITICAL,  "CRITICAL"},
  //     {ERROR,     "ERROR"},
  //     {WARNING,   "WARNING"},
  //     {INFO,      "INFO"},
  //     {DEBUG,     "DEBUG"},
  const char * set_level(level_t l);
}

std::ostream& operator<<(logger_server &log, char * const &t );
std::ostream& operator<<(logger_client &log, char * const &t );
std::ostream& operator<<(logger_server &log, const char * const &t );
std::ostream& operator<<(logger_client &log, const char * const &t );
std::ostream& operator<<(logger_server &log, const logger::level_t &t);
std::ostream& operator<<(logger_client &log, const logger::level_t &t);



class logger_client{//don't fork/share
 private:
  std::string header;
  std::string header_tag;
  std::string z_frontend_addr;
  std::stringstream streamer;
  logger::level_t level;
  double last_timestamp;

  uint8_t server_timeout_count;

  std::vector<std::string> format;
  std::vector<std::string> specials;

  uint8_t z_running;
  zmq::context_t z_context;
  zmq::socket_t z_frontend_socket;
  uint8_t z_connected;

  uint8_t auto_connect;
  uint8_t use_cout;

  uint8_t enabled;

  void _init_defaults();
  void add_header();

  void setup_network();
  void shutdown_network();

 public:
  logger_client();
  logger_client(std::string tag,std::string frontend_addr, bool auto_connect=true, bool cout_fallback=false);
  ~logger_client();

  void connect();
  void disable();

  uint8_t commit(payload_t t=CHAR);
  void flush();
  uint8_t is_connected(){ return !z_connected; }

  std::string debug_me(double &ts);

  friend std::ostream& operator<<( logger_client &log, char * const &t );
  friend std::ostream& operator<<( logger_client &log, const char * const &t );
  friend std::ostream& operator<<( logger_client &log, const logger::level_t &t );

};

class logger_server{//don't fork/share
 private:
  std::string tag;
  std::ostream &dumper;
  std::string z_frontend_addr;
  std::string z_backend_addr;
  std::stringstream streamer;
  std::mutex q_mutex;
  std::mutex qe_mutex;
  std::condition_variable q_event;
  std::queue<std::string> holder;
  double last_timestamp;

  uint8_t z_running;
  uint8_t z_workers;
  zmq::context_t z_context;
  zmq::socket_t z_frontend;
  zmq::socket_t z_backend;
  std::vector<zmq::socket_t> z_sockets;
  std::vector<std::thread> z_threads;
  std::thread z_broker_thread;

  logger::null_buff void_buff;
  std::ostream null_stream;
  logger_client *log_c;

  void _init_defaults();
  void sort_holder();

  void setup_network();
  void broker();
  void enqueue_worker(uint8_t rank);

 public:
  logger_server();
  logger_server(std::string tag,std::ostream &s,
                std::string frontend_addr,
                std::string backend_addr);
  ~logger_server();

  uint8_t commit(payload_t t=CHAR);
  void flush();

  void stop();
  void wait();
  bool empty(){ return holder.empty(); }
  uint8_t hold_for(double timeout);
  uint8_t hold_for(double timeout, bool(pred)());
  uint8_t hold_for(double timeout, std::function<bool()> pred);

  void debug_me(std::string s, double &ts);

  friend std::ostream& operator<<( logger_server &log, char * const &t );
  friend std::ostream& operator<<( logger_server &log, const char * const &t );
  friend std::ostream& operator<<( logger_server &log, const logger::level_t &t);

};

#endif // LOGGER_HH_INCLUDED