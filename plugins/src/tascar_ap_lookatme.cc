#include "audioplugin.h"
#include <lo/lo.h>

class lookatme_t : public TASCAR::audioplugin_base_t {
public:
  lookatme_t( const TASCAR::audioplugin_cfg_t& cfg );
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos, const TASCAR::zyx_euler_t&, const TASCAR::transport_t& tp);
  void configure();
  void add_variables( TASCAR::osc_server_t* srv );
  ~lookatme_t();
private:
  lo_address lo_addr;
  double tau;
  double fadelen;
  double threshold;
  std::string animation;
  std::string url;
  std::vector<std::string> paths;
  std::string thresholdpath;
  std::string levelpath;
  TASCAR::pos_t pos_onset;
  TASCAR::pos_t pos_offset;
  std::string self_;

  double lpc1;
  double rms;
  bool waslooking;
  bool active;
  bool discordantLS;
};

lookatme_t::lookatme_t( const TASCAR::audioplugin_cfg_t& cfg )
  : audioplugin_base_t( cfg ),
    tau(1),
    fadelen(1),
    threshold(0.01),
    url("osc.udp://localhost:9999/"),
    self_(cfg.parentname),
    lpc1(0.0),
    rms(0.0),
    waslooking(false),
    active(true),
    discordantLS(false)
{
  GET_ATTRIBUTE(tau);
  GET_ATTRIBUTE(fadelen);
  GET_ATTRIBUTE_DBSPL(threshold);
  GET_ATTRIBUTE(url);
  GET_ATTRIBUTE(paths);
  GET_ATTRIBUTE(animation);
  GET_ATTRIBUTE(thresholdpath);
  GET_ATTRIBUTE(levelpath);
  GET_ATTRIBUTE(pos_onset);
  GET_ATTRIBUTE(pos_offset);
  if( url.empty() )
    url = "osc.udp://localhost:9999/";
  lo_addr = lo_address_new_from_url(url.c_str());
}

void lookatme_t::add_variables( TASCAR::osc_server_t* srv )
{
  srv->add_bool("/active",&active);    
  srv->add_bool("/discordantLS",&discordantLS);    
}

void lookatme_t::configure()
{
  audioplugin_base_t::configure();
  lpc1 = exp(-1.0/(tau*f_fragment));
  rms = 0;
  waslooking = false;
}

lookatme_t::~lookatme_t()
{
  lo_address_free(lo_addr);
}

void lookatme_t::ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos, const TASCAR::zyx_euler_t&, const TASCAR::transport_t& tp)
{
  rms = lpc1*rms + (1.0-lpc1)*chunk[0].rms();
  if( !levelpath.empty() )
    lo_send( lo_addr, levelpath.c_str(), "f",  20*log10(rms) );
  if(rms > threshold ){
    if(!waslooking ){
      // send lookatme values to osc target:
      if( active ){
        if( !pos_onset.is_null() ){
          for(std::vector<std::string>::iterator s=paths.begin();s!=paths.end();++s)
            lo_send( lo_addr, s->c_str(), "sffff", "/lookAt", pos_onset.x, pos_onset.y, pos_onset.z, fadelen );
        }else{
          for(std::vector<std::string>::iterator s=paths.begin();s!=paths.end();++s)
            lo_send( lo_addr, s->c_str(), "sffff", "/lookAt", pos.x, pos.y, pos.z, fadelen );
        }
        if( !animation.empty() )
          lo_send( lo_addr, self_.c_str(), "ss", "/animation", animation.c_str() );
      }
      if( !thresholdpath.empty() )
        lo_send( lo_addr, thresholdpath.c_str(), "f", 1.0f );
      if( discordantLS )
        lo_send( lo_addr, self_.c_str(), "sf", "/discordantLS", 1.0 );
      waslooking = true;
    }
  }else{
    if( waslooking ){
      if( active ){
        if( !pos_offset.is_null() ){
          for(std::vector<std::string>::iterator s=paths.begin();s!=paths.end();++s)
            lo_send( lo_addr, s->c_str(), "sffff", "/lookAt", pos_offset.x, pos_offset.y, pos_offset.z, fadelen );
        }
      }
      if( !thresholdpath.empty() )
        lo_send( lo_addr, thresholdpath.c_str(), "f", 0.0f );
      //if( discordantLS )
      lo_send( lo_addr, self_.c_str(), "sf", "/discordantLS", 0.0 );
    }
    // below threshold, release:
    waslooking = false;
  }
}

REGISTER_AUDIOPLUGIN(lookatme_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
