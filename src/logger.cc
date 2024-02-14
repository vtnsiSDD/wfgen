
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <ios>
#include <ctime>
#include <unistd.h>

#include "logger.hh"

constexpr const char* file_name(const char* path){
  const char *file = path;
  while(*path){
    if(*path++ == '/'){
      file = path;
    }
  }
  return file;
}

const char serial_key[] = "wfgen:";
logger::null_buff drop;
std::ostream null_stream(&drop);
logger::level_map_t logger::level_map = {
    {logger::OFF,       "OFF"},
    {logger::CRITICAL,  "CRITICAL"},
    {logger::ERROR,     "ERROR"},
    {logger::WARNING,   "WARNING"},
    {logger::INFO,      "INFO"},
    {logger::DEBUG,     "DEBUG"},
};

const char * logger::set_level(level_t l){
  auto search = level_map.find(l);
  if(search != level_map.end()) return search->second;
  search = level_map.find(logger::OFF);
  return search->second;
}


logger_client::logger_client()
  : z_frontend_addr("tcp://127.0.0.1:40000"),
    streamer(std::string()),
    server_timeout_count(0),
    z_running(false),
    z_context(),
    z_frontend_socket(z_context, ZMQ_REQ),
    z_connected(false),
    auto_connect(false),
    use_cout(false),
    enabled(1)
{
  std::string file = file_name(__FILE__);
  std::stringstream temp;
  temp << ::getpid();
  header_tag = file.substr(0,file.size()) + "(" + temp.str() + ")";
  _init_defaults();
  if(auto_connect){
    setup_network();
  }
  else{
    enabled=0;
  }
}
logger_client::logger_client(std::string tag, std::string frontend_addr, bool auto_connect, bool cout_fallback)
  : header_tag(tag),
    z_frontend_addr(frontend_addr),
    streamer(std::string()),
    server_timeout_count(0),
    z_running(false),
    z_context(),
    z_frontend_socket(z_context, ZMQ_REQ),
    z_connected(false),
    auto_connect(auto_connect),
    use_cout(false),
    enabled(1)
{
  if (header_tag.size() == 0){
    std::string file = file_name(__FILE__);
    std::stringstream temp;
    temp << ::getpid();
    header_tag = file.substr(0,file.size()) + "(" + temp.str() + ")";
  }
  if(z_frontend_addr.size() == 0){
    z_frontend_addr = "tcp://127.0.0.1:40000";
  }
  _init_defaults();
  if(auto_connect){
    setup_network();
  }
  else{
    use_cout=cout_fallback;
    enabled=0;
  }
}
logger_client::~logger_client()
{
  *this << set_level(logger::INFO) << "Disconnecting\n";
  commit();
  shutdown_network();
}

void logger_client::_init_defaults(){
  specials = {
    "{color_reset}",
    "{header_tag_color}",
    "{header_tag}",
    "{date_color}",
    "{date}",
    "{timestamp_color}",
    "{timestamp}",
    "{level_color}",
    "{level}",
  };
  format = {"[", "{header_tag}", " :: ", "{timestamp}", " ] ", "{level}", " - "};
}
void logger_client::add_header(){
  auto now = std::chrono::high_resolution_clock::now();
  auto micro = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
  uint frac = micro.count() % 1000000;
  last_timestamp = micro.count()/1e6;
  auto timer = std::chrono::high_resolution_clock::to_time_t(now);
  std::tm lt = *std::localtime(&timer);

  std::ostream& use_stream = (use_cout) ? std::cout : streamer;
  for(auto idx = format.begin(); idx != format.end(); idx++ ){
    if( std::find(specials.begin(), specials.end(), *idx) != specials.end() ){
      if(*idx == "{color_reset}") use_stream << "\033[0m";
      else if(*idx == "{header_tag_color}") use_stream << "\033[0m";
      else if(*idx == "{timestamp_color}") use_stream << "\033[0m";
      else if(*idx == "{level_color}") use_stream << "\033[0m";
      else if(*idx == "{header_tag}") use_stream << header_tag;
      else if(*idx == "{date}"){
        auto old_flags = use_stream.flags();
        use_stream << std::put_time(&lt, "%Y%m%d");
        use_stream.flags(old_flags);
      }
      else if(*idx == "{timestamp}"){
        auto old_flags = use_stream.flags();
        use_stream << std::put_time(&lt, "%H:%M:%S")
                 << "." << std::setfill('0') << std::setw(6)
                 << frac << std::setfill(' ');
        use_stream.flags(old_flags);
      }
      else if(*idx == "{level}"){
        auto old_flags = use_stream.flags();
        use_stream << std::setw(8);
        auto search = logger::level_map.find(level);
        if(search != logger::level_map.end())
          use_stream << search->second;
        else{
          search = logger::level_map.find(logger::OFF);
          use_stream << search->second;
        }
        use_stream.flags(old_flags);
      }
    }
    else{
      use_stream << *idx;
    }
  }
}
std::ostream& operator<<(logger_client &log, char * const &t){
  if(!log.enabled && !log.use_cout) return null_stream;
  uint8_t level_found = 0;
  log.level = logger::OFF;
  for(int idx = logger::CRITICAL; idx != logger::INVALID; idx++){
    if( strcmp( t, logger::level_map[static_cast<logger::level_t>(idx)] ) == 0 ){
      log.level = static_cast<logger::level_t>(idx);
      level_found = 1;
    }
  }
  if(level_found){
    log.add_header();
    if(log.use_cout) return std::cout;
    return log.streamer;
  }
  else{
    return null_stream;
  }
}
std::ostream& operator<<(logger_client &log, const char * const &t){
  if(!log.enabled && !log.use_cout) return null_stream;
  uint8_t level_found = 0;
  log.level = logger::OFF;
  for(int idx = logger::CRITICAL; idx != logger::INVALID; idx++){
    if( strcmp( t, logger::level_map[static_cast<logger::level_t>(idx)] ) == 0 ){
      log.level = static_cast<logger::level_t>(idx);
      level_found = 1;
    }
  }
  if(level_found){
    log.add_header();
    if(log.use_cout) return std::cout;
    return log.streamer;
  }
  else{
    return null_stream;
  }
}
std::ostream& operator<<(logger_client &log, const logger::level_t &t){
  if(!log.enabled && !log.use_cout) return null_stream;
  log.level = t;
  log.add_header();
  if(log.use_cout) return std::cout;
  return log.streamer;
}
void logger_client::connect(){
  if(!enabled){
    setup_network();
    use_cout=false;
    enabled=true;
  }
}
void logger_client::setup_network(){
  if(server_timeout_count >= 100U || z_connected) return;
  void* alive = (void*)z_context;
  if( alive == nullptr ){
    // context has been closed
    z_context = zmq::context_t();
  }
  alive = (void*)z_frontend_socket;
  if( alive == nullptr ){
    // socket has been closed
    z_frontend_socket = zmq::socket_t(z_context,ZMQ_REQ);
  }
  z_frontend_socket.connect(z_frontend_addr);
  zmq::pollitem_t items[] = {{z_frontend_socket,0,ZMQ_POLLOUT,0}};

  try{
    zmq::poll(items, 1, 0);//should be in this state immediately
  }
  catch (zmq::error_t &ex){
    if(ex.num() != EINTR) throw;//if interrupted oh well
  }
  if(items[0].revents & ZMQ_POLLOUT){
    *this << set_level(logger::INFO) << "Connecting\n";
    z_connected = commit();
    return;
  }
  shutdown_network();
}
void logger_client::shutdown_network(){
  void *context_alive = (void *)z_context;
  void *socket_alive = (void *)z_frontend_socket;
  if( context_alive != nullptr ){
    z_context.shutdown();
  }
  if( socket_alive != nullptr ){
    if(z_frontend_socket.connected()){
      try{
        z_frontend_socket.disconnect(z_frontend_addr);
      }
      catch (const zmq::error_t &z_e){
        // some reason need to do this even though it makes no sense
        // std::cout << "frontend.disconnect : " << z_e.what() << std::endl;
      }
    }
    z_frontend_socket.close();
  }
  if( context_alive != nullptr ){
    z_context.close();
  }
}
void logger_client::disable(){ enabled = 0; use_cout = 0; shutdown_network(); }
void logger_client::flush(){
  // Take the payload structs out of the holder and push the string
  // serialization over the frontend_socket
  std::cout << streamer.str() << std::flush;
  streamer.str(std::string());
  streamer.clear();
}
std::string logger_client::debug_me(double &ts){
  ts = last_timestamp;
  return streamer.str();
}
void logger_server::debug_me(std::string line,double &ts){
  payload p = {ts,CHAR,line.size(),(void*)line.c_str()};
  size_t len;
  void *s = nullptr;
  uint rc;
  if((rc = serialize_payload(&s, &len, p))){
    // return rc;
  }
  std::string buf((char*)s,len);
  holder.push(buf);
}
uint8_t logger_client::commit(payload_t t){
  if(!enabled || use_cout) return 0;
  std::string line = streamer.str();
  payload p = {last_timestamp,t,line.size(),(void*)line.c_str()};
  size_t len;
  void *s = nullptr;
  uint rc;
  if((rc = serialize_payload(&s, &len, p))){
    return rc;
  }
  // std::string buf((char*)s,len);
  ///// SEND TO SERVER HERE
  zmq::message_t z_msg(s,len);
  zmq::message_t r_msg;

  zmq::pollitem_t z_items[] = {{z_frontend_socket,0,ZMQ_POLLIN | ZMQ_POLLOUT,0}};
  zmq::poll(z_items,1,100);
  if(z_items[0].revents & ZMQ_POLLIN){
    //// hmm in an odd state, but ok
    z_frontend_socket.recv(&r_msg);
    r_msg.rebuild();
    zmq::poll(z_items,1,100);
  }
  if(!(z_items[0].revents & ZMQ_POLLOUT)){
    // not in send mode? reconnect
    z_context.shutdown();
    if(z_frontend_socket.connected()){
      try{
        z_frontend_socket.disconnect(z_frontend_addr);
      }
      catch (const zmq::error_t &z_e){
        // some reason need to do this even though it makes no sense
        // std::cout << "frontend.disconnect : " << z_e.what() << std::endl;
      }
    }
    z_frontend_socket.close();
    z_context.close();
    server_timeout_count++;
    z_connected = false;
    setup_network();
    if(!z_connected){//failed to reconnect
      streamer.str(std::string());
      streamer.clear();
      return 1;
    }
  }
  try{
    z_frontend_socket.send(z_msg);
  }
  catch(const zmq::error_t &ex){
    streamer.str(std::string());
    streamer.clear();
    return 1;
  }
  try{
    zmq::poll(z_items,1,100);
    if(z_items[0].revents & ZMQ_POLLIN)
      z_frontend_socket.recv(&r_msg);//should probably confirm good msg, but eh.
  }
  catch(const zmq::error_t &ex){
    streamer.str(std::string());
    streamer.clear();
    return 1;
  }
  /////
  reset_payload(p,0);
  streamer.str(std::string());
  streamer.clear();
  return 0;
}


logger_server::logger_server()
  : tag(std::string()),
    dumper(std::cout),
    z_frontend_addr("tcp://127.0.0.1:40000"),
    z_backend_addr("tcp://127.0.1.1:40000"),
    streamer(std::string()),
    z_running(false),
    z_context(),
    z_frontend(z_context, ZMQ_ROUTER),
    z_backend(z_context, ZMQ_DEALER),
    void_buff(),
    null_stream(&void_buff),
    log_c(nullptr)
{
  _init_defaults();
  setup_network();
  log_c = new logger_client("",z_frontend_addr);
}
logger_server::logger_server(std::string tag, std::ostream &s, std::string frontend_addr, std::string backend_addr)
  : tag(tag),dumper(s),
    z_frontend_addr(frontend_addr),
    z_backend_addr(backend_addr),
    streamer(std::string()),
    z_running(false),
    z_context(),
    z_frontend(z_context, ZMQ_ROUTER),
    z_backend(z_context, ZMQ_DEALER),
    void_buff(),
    null_stream(&void_buff),
    log_c(nullptr)
{
  if(z_frontend_addr.size() == 0){
    z_frontend_addr = "tcp://127.0.0.1:40000";
  }
  if(z_backend_addr.size() == 0){
    z_backend_addr = "tcp://127.0.1.1:40000";
  }
  _init_defaults();
  setup_network();
  log_c = new logger_client("",z_frontend_addr);
}
logger_server::~logger_server()
{
  if (log_c != nullptr){ delete log_c; log_c = nullptr; }
  *this << set_level(logger::INFO) << "Disconnecting\n";
  commit();
  stop();
  wait();
  if(z_frontend.connected()){
    try{
      z_frontend.disconnect(z_frontend_addr);
    }
    catch (const zmq::error_t &z_e){
      // some reason need to do this even though it makes no sense
      // std::cout << "frontend.disconnect : " << z_e.what() << std::endl;
    }
  }
  if(z_backend.connected()){
    try{
      z_backend.disconnect(z_backend_addr);
    }
    catch (const zmq::error_t &z_e){
      // some reason need to do this even though it makes no sense
      // std::cout << "backend.disconnect : " << z_e.what() << std::endl;
    }
  }
  z_frontend.close();
  z_backend.close();
  z_context.close();
}

void logger_server::_init_defaults(){
  z_workers = 20;
  z_sockets = std::vector<zmq::socket_t>(z_workers);
}

std::ostream& operator<<(logger_server &log, char * const &t){
  if(log.log_c == nullptr) return log.null_stream;
  return (*(log.log_c) << t);
}
std::ostream& operator<<(logger_server &log, const char * const &t){
  if(log.log_c == nullptr) return log.null_stream;
  return (*(log.log_c) << t);
}
std::ostream& operator<<(logger_server &log, const logger::level_t &t){
  if(log.log_c == nullptr) return log.null_stream;
  return (*(log.log_c) << t);
}
void logger_server::sort_holder(){// not thread safe
  // if (holder.size() <= 1) return;
  std::vector<payload> sortee(holder.size());
  size_t index = 0;
  size_t l;
  while(holder.empty() == false){
    const std::string str = holder.front();
    const char *s = str.c_str();
    l = str.size();
    deserialize_payload(&sortee[index++], (void**)&s, l);
    holder.pop();
  }

  std::sort(sortee.begin(), sortee.end(),
          [](payload a, payload b){ return a.timestamp < b.timestamp; });
  
  
  void *c = nullptr;
  l = 0;
  for(index = 0; index < sortee.size(); index++){
    serialize_payload(&c, &l, sortee[index]);
    std::string s((char*)c,l);
    holder.push(s);
    free(c);
    c = nullptr;
    l = 0;
  }
}
void logger_server::setup_network(){
  int timeout = 100;
  z_frontend.setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
  z_backend.setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
  z_frontend.bind(z_frontend_addr);//throws if fail
  z_backend.bind(z_backend_addr);//throws if fail

  // zmq::proxy(z_frontend, z_backend, nullptr);//blocking
  // zmq_proxy((void*)&z_frontend, (void*)&z_backend, nullptr);// no clue

  z_broker_thread = std::thread(&logger_server::broker, this);

  z_running = true;
  // start workers
  for(uint8_t idx = 0; idx < z_workers; idx++){
    z_threads.emplace_back(&logger_server::enqueue_worker, this, idx);
  }

  *this << set_level(logger::INFO) << "Connected\n";
  commit();
  flush();
}
void logger_server::broker(){
  zmq::pollitem_t to_poll[] = {
    {z_frontend, 0, ZMQ_POLLIN, 0},
    {z_backend, 0, ZMQ_POLLIN, 0}
  };
  int more;
  size_t more_size = sizeof(more);
  zmq::message_t z_msg;
  while(z_running){
    try{
      zmq::poll(to_poll, 2, 100);
    }
    catch (zmq::error_t &ex){
      if(ex.num() == ETERM) continue;
      z_running = false;
      if(ex.num() != EINTR) throw;
      continue;
    }
    if(to_poll[0].revents & ZMQ_POLLIN){
      while(1){
        z_frontend.recv(&z_msg);
        z_frontend.getsockopt(ZMQ_RCVMORE, &more, &more_size);
        z_backend.send(z_msg, more ? ZMQ_SNDMORE : 0);
        // std::cout << "broker : f->b m(" << more << ")\n";
        if(!more) break;
      }
    }
    if(to_poll[1].revents & ZMQ_POLLIN){
      while(1){
        z_backend.recv(&z_msg);
        z_backend.getsockopt(ZMQ_RCVMORE, &more, &more_size);
        z_frontend.send(z_msg, more ? ZMQ_SNDMORE : 0);
        // std::cout << "broker : b->f m(" << more << ")\n";
        if(!more) break;
      }
    }
  }
}
void logger_server::enqueue_worker(uint8_t rank){
  // This is a thread that should constantly poll and push stuff into holder
  // might need a mutex when racing with flush()
  zmq::context_t local_context;
  int timeout = 100;
  z_sockets[rank] = zmq::socket_t(local_context, ZMQ_REP);
  z_sockets[rank].setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
  z_sockets[rank].connect(z_backend_addr);

  zmq::pollitem_t z_poll_items[] = {{z_sockets[rank], 0, ZMQ_POLLIN, 0}};
  int more;
  size_t more_size = sizeof(more);
  std::string buf;
  payload p;
  while(z_running){
    try{
      zmq::poll(z_poll_items, 1, 100);
    }
    catch (zmq::error_t &ex){
      if(ex.num() != EINTR) throw;
      if(z_sockets[rank].connected()){
        try{
          z_sockets[rank].disconnect(z_backend_addr);
        }
        catch (const zmq::error_t &z_e){}
      }
      z_sockets[rank].close();
      local_context.close();
      
      //might be a hickup, keep running on continue if still active
      local_context = zmq::context_t();
      int timeout = 100;
      z_sockets[rank] = zmq::socket_t(local_context, ZMQ_REP);
      z_sockets[rank].setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
      z_sockets[rank].connect(z_backend_addr);
      continue;
    }
    if(z_poll_items[0].revents & ZMQ_POLLIN){
      zmq::message_t z_message;// cleaned up when out of scope
      z_sockets[rank].recv(&z_message);
      z_sockets[rank].getsockopt(ZMQ_RCVMORE, &more, &more_size);
      // std::cout << "worker: b->w m(" << more <<")\n";
      buf += z_message.to_string();
      if(!more){
        // full message is done. So enqueue
        zmq::message_t z_msg;
        const char *c = buf.c_str();
        size_t len = buf.size();
        if(!deserialize_payload(&p,(void**)&c,len)){
          z_msg = zmq::message_t("good",5);
        }
        else{
          z_msg = zmq::message_t("bad",4);
        }
        z_sockets[rank].send(z_msg);
        // std::cout << "worker: w->b\n";
        std::unique_lock<std::mutex> p_lock(q_mutex);
        holder.push(buf);
        q_event.notify_all();
        p_lock.unlock();
        reset_payload(p,0);
        buf = std::string();
      }
    }
  }
  try{
    z_sockets[rank].disconnect(z_backend_addr);
  }
  catch (const zmq::error_t &z_e){}
  z_sockets[rank].close();
  local_context.close();
}
void logger_server::stop(){
  uint8_t shutdown = (z_running == true);
  z_running = false;
  if(shutdown) z_context.shutdown();
}
void logger_server::wait(){
  for(uint8_t idx = 0; idx < z_workers; idx++){
    if(z_threads[idx].joinable()) z_threads[idx].join();
  }
  if (z_broker_thread.joinable()) z_broker_thread.join();
}
void logger_server::flush(){
  // Take the payload structs out of the holder and push the string
  // to the dumper. Likely want a mutex with this and enque_worker()
  std::unique_lock<std::mutex> p_lock(q_mutex);
  sort_holder();
  size_t len;
  payload p;
  while(holder.empty() == false){
    const std::string line = holder.front();
    len = line.size();
    const char *s = line.c_str();
    deserialize_payload(&p, (void**)&s, len);
    const std::string log((char*)p.buffer,p.length);
    dumper << log << std::flush;
    holder.pop();
  }
  p_lock.unlock();
}
uint8_t logger_server::commit(payload_t t){
  if(log_c == nullptr) return 1;
  return log_c->commit(t);
}
uint8_t logger_server::hold_for(double timeout){
  std::unique_lock<std::mutex> local_lock(qe_mutex);
  auto now = std::chrono::high_resolution_clock::now();
  if(q_event.wait_until(local_lock,now+std::chrono::duration<double>(timeout)) == std::cv_status::timeout){
    return 0;
  }
  return 1;
}
uint8_t logger_server::hold_for(double timeout, bool(pred)()){
  std::unique_lock<std::mutex> local_lock(qe_mutex);
  auto now = std::chrono::high_resolution_clock::now();
  if(q_event.wait_until(local_lock,
          now+std::chrono::duration<double>(timeout),
          pred)){
    return !empty();
  }
  return 0;
}
uint8_t logger_server::hold_for(double timeout, std::function<bool()> pred){
  std::unique_lock<std::mutex> local_lock(qe_mutex);
  auto now = std::chrono::high_resolution_clock::now();
  if(q_event.wait_until(local_lock,
          now+std::chrono::duration<double>(timeout),
          pred)){
    return !empty();
  }
  return 0;
}



uint16_t get_payload_itemsize(payload_t &type){
  uint16_t scale;
  switch (type){
    case CHAR:
    case UCHAR:
      scale = sizeof(char);
      break;
    case CHAR16:
      scale = sizeof(char16_t);
      break;
    case CHAR32:
      scale = sizeof(char32_t);
      break;
    case WCHAR:
      scale = sizeof(wchar_t);
      break;
    default:
      scale = 1;
      break;
  }
  return scale;
}

size_t get_sizeof_payload(payload &p){
  size_t size = sizeof(serial_key) + sizeof(double) + sizeof(payload_t)
            + sizeof(uint64_t);
  size_t scale = get_payload_itemsize(p.type);
  size += p.length*scale;
  return size;
}

uint8_t serialize_payload(void **s, size_t *l, payload &p){
  if(s == nullptr){
    return 3U;
  }
  if((*s) == nullptr){
    *l = get_sizeof_payload(p);
    *s = malloc(*l);
  }
  else if (*l < get_sizeof_payload(p)){
    return 1U; // not enough space for the payload to be copied
  }
  if( (*s) == NULL ){
    return 2U;
  }
  uint8_t *buf = (uint8_t*)(*s);
  size_t scale = get_payload_itemsize(p.type);
  memcpy(buf, serial_key, sizeof(serial_key));
  buf += sizeof(serial_key);
  memcpy(buf, &p.timestamp, sizeof(double));
  buf += sizeof(double);
  uint8_t pd = p.type;
  memcpy(buf, &pd, sizeof(uint8_t));
  buf += sizeof(uint8_t);
  memcpy(buf, &p.length, sizeof(uint64_t));
  buf += sizeof(uint64_t);
  memcpy(buf, p.buffer, scale*p.length);
  return 0U;
}
uint8_t deserialize_payload(payload *p, void **s, size_t &l){
  uint8_t good_start = 1;
  if(s == nullptr){
    good_start = 0;
  }
  else if(*s == nullptr){
    good_start = 0;
  }
  if (l < sizeof(serial_key)){
    good_start = 0;
  }
  if (!good_start) return 2U;
  char *buf = (char*)(*s);
  // std::cout << "--large scale checks seem good\n";
  for(size_t idx = 0; idx < sizeof(serial_key); idx++){
    if(serial_key[idx] != buf[idx]) good_start = 0;
  }
  if (!good_start) return 1U;
  // std::cout << "--the expected key is there\n";
  if(p == nullptr){
    p = (payload *)malloc(sizeof(payload));
  }
  else{
    reset_payload(*p);
  }
  buf += sizeof(serial_key);
  memcpy(&(p->timestamp), buf, sizeof(double));
  buf += sizeof(double);
  uint8_t pd;
  memcpy(&pd, buf, sizeof(uint8_t));
  p->type = static_cast<payload_t>(pd);
  buf += sizeof(uint8_t);
  memcpy(&(p->length), buf, sizeof(uint64_t));
  buf += sizeof(uint64_t);
  size_t scale = get_payload_itemsize(p->type);
  p->buffer = malloc(scale*p->length);
  memcpy((p->buffer), buf, scale*p->length);
  return 0U;
}

void destroy_payload(payload **p){
  if(p==nullptr) return;
  if(*p==nullptr) return;
  if((*p)->buffer != nullptr) free((*p)->buffer);
  free(p);
}
void reset_payload(payload &p, uint8_t own_buffer){
  if(own_buffer != 0 && p.buffer != nullptr) free(p.buffer);
  p.buffer = nullptr;
  p.length = 0;
  p.timestamp = 0;
}
void print_payload_stats(payload &p){
  std::cout << "timestamp: " << p.timestamp << std::endl;
  std::cout << "type: " << (int)p.type << std::endl;
  std::cout << "length: " << (size_t)p.length << std::endl;
  std::cout << "buffer: " << p.buffer << std::endl;
}