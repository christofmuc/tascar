#include "jackiowav.h"
#include "session.h"
#include <mutex>

#define OSC_VOID(x)                                                            \
  static int x(const char* path, const char* types, lo_arg** argv, int argc,   \
               lo_message msg, void* user_data)                                \
  {                                                                            \
    ((jackrec_t*)user_data)->x();                                              \
    return 0;                                                                  \
  };                                                                           \
  void x()

#define OSC_STRING(x)                                                          \
  static int x(const char* path, const char* types, lo_arg** argv, int argc,   \
               lo_message msg, void* user_data)                                \
  {                                                                            \
    ((jackrec_t*)user_data)->x(&(argv[0]->s));                                 \
    return 0;                                                                  \
  };                                                                           \
  void x(const std::string&)

class jackrec_t : public TASCAR::module_base_t {
public:
  jackrec_t(const TASCAR::module_cfg_t& cfg);
  ~jackrec_t();
  void add_variables(TASCAR::osc_server_t* srv);
  OSC_VOID(start);
  OSC_VOID(stop);
  OSC_VOID(clearports);
  OSC_STRING(addport);
  OSC_VOID(listports);

private:
  // configuration variables:
  std::string name;
  double buflen;
  // OSC variables:
  std::string ofname;
  std::vector<std::string> ports;
  // internal members:
  std::string prefix;
  jackrec_async_t* jr;
  std::mutex mtx;
  lo_address lo_addr;
  void service();
  std::thread srv;
  bool run_service;
};

jackrec_t::jackrec_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), name("jackrec"), buflen(10), jr(NULL), lo_addr(NULL),
      run_service(true)
{
  std::string url;
  // get configuration variables:
  GET_ATTRIBUTE(name);
  GET_ATTRIBUTE(buflen);
  GET_ATTRIBUTE(url);
  // register OSC variables:
  prefix = std::string("/") + name;
  add_variables(session);
  // optionally set OSC response target:
  if(!url.empty())
    lo_addr = lo_address_new_from_url(url.c_str());
  srv = std::thread(&jackrec_t::service, this);
  if(lo_addr)
    lo_send(lo_addr, (prefix + "/ready").c_str(), "");
}

jackrec_t::~jackrec_t()
{
  run_service = false;
  srv.join();
  std::lock_guard<std::mutex> lock(mtx);
  if(jr)
    delete jr;
  if(lo_addr)
    lo_address_free(lo_addr);
}

void jackrec_t::service()
{
  size_t xrun(0);
  size_t werror(0);
  while(run_service) {
    {
      std::lock_guard<std::mutex> lock(mtx);
      if(jr && lo_addr){
        lo_send(lo_addr, (prefix + "/rectime").c_str(), "f", (float)(jr->rectime));
        if( jr->xrun > xrun ){
          xrun = jr->xrun;
          lo_send(lo_addr, (prefix + "/xrun").c_str(), "i", xrun);
        }
        if( jr->werror > werror ){
          if( werror == 0 )
            lo_send(lo_addr, (prefix + "/error").c_str(), "s", "Disk write error.");
          werror = jr->werror;
        }
      }
    }
    usleep(200000);
  }
}

void jackrec_t::clearports()
{
  ports.clear();
}

void jackrec_t::addport(const std::string& port)
{
  ports.push_back(port);
}

void jackrec_t::start()
{
  std::lock_guard<std::mutex> lock(mtx);
  if(jr)
    delete jr;
  try {
    std::string ofname_(ofname);
    if(ofname.empty()) {
      time_t rawtime;
      struct tm* timeinfo;
      char buffer[80];
      time(&rawtime);
      timeinfo = localtime(&rawtime);
      strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", timeinfo);
      ofname_ = std::string("rec") + std::string(buffer) + std::string(".wav");
    }
    jr = new jackrec_async_t(ofname_, ports, name, buflen);
    if(lo_addr)
      lo_send(lo_addr, (prefix + "/start").c_str(), "");
  }
  catch(const std::exception& e) {
    std::string msg(e.what());
    msg = std::string("Failure: ") + msg;
    jr = NULL;
    TASCAR::add_warning(msg);
    if(lo_addr)
      lo_send(lo_addr, (prefix + "/error").c_str(), "s", msg.c_str());
  }
}

void jackrec_t::stop()
{
  std::lock_guard<std::mutex> lock(mtx);
  if(jr)
    delete jr;
  jr = NULL;
  if(lo_addr)
    lo_send(lo_addr, (prefix + "/stop").c_str(), "");
}

void jackrec_t::listports()
{
  jackc_portless_t jc(name + "_port");
  std::vector<std::string> lports(
      jc.get_port_names_regexp(".*", JackPortIsOutput));
  if(lo_addr) {
    lo_send(lo_addr, (prefix + "/portlist").c_str(), "");
    for(auto p : lports)
      if(p.find("sync_out") == std::string::npos)
        lo_send(lo_addr, (prefix + "/port").c_str(), "s", p.c_str());
  }
}

void jackrec_t::add_variables(TASCAR::osc_server_t* srv)
{
  std::string prefix_(srv->get_prefix());
  srv->set_prefix(prefix);
  srv->add_string("/name", &ofname);
  srv->add_method("/start", "", &jackrec_t::start, this);
  srv->add_method("/stop", "", &jackrec_t::stop, this);
  srv->add_method("/clear", "", &jackrec_t::clearports, this);
  srv->add_method("/addport", "s", &jackrec_t::addport, this);
  srv->add_method("/listports", "", &jackrec_t::listports, this);
  srv->set_prefix(prefix_);
}

REGISTER_MODULE(jackrec_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
