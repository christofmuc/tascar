#ifndef ACOUSTICMODEL_H
#define ACOUSTICMODEL_H

#include "delayline.h"
#include "receivermod.h"
#include "dynamicobjects.h"

namespace TASCAR {

  /** \brief Components relevant for the acoustic modelling
   */
  namespace Acousticmodel {
  
    /** \brief Primary source (also base class for mirror sources)
     */
    class pointsource_t {
    public:
      pointsource_t(uint32_t chunksize,double maxdist_,uint32_t sincorder_);
      virtual ~pointsource_t();
      virtual pos_t get_effective_position(const pos_t& receiverp,double& gain);
      virtual pos_t get_physical_position() const { return position; };
      virtual void preprocess();
      void add_rmslevel(wave_t* rmslevel_);
      wave_t audio;
      pos_t position;
      bool active;
      bool direct;
      double maxdist;
      uint32_t sincorder;
      uint32_t ismorder;
      wave_t* rmslevel;
    };

    /**
       \brief Diffraction model
     */
    class diffractor_t : public ngon_t {
    public:
      class state_t {
      public:
        double A1;
        float s1;
        float s2;
        state_t() : A1(0),s1(0),s2(0) {};
      };
      pos_t process(pos_t p_src, const pos_t& p_rec, wave_t& audio, double c, double fs, state_t& state,float drywet);
    };

    class doorsource_t : public pointsource_t, public diffractor_t {
    public:
      doorsource_t(uint32_t chunksize, double maxdist,uint32_t sincorder_);
      virtual pos_t get_effective_position(const pos_t& receiverp,double& gain);
      //void process();
      double inv_falloff;
      double distance;
      bool wnd_sqrt;
    };

    class diffuse_source_t : public shoebox_t {
    public:
      diffuse_source_t(uint32_t chunksize,wave_t& rmslevel_);
      virtual ~diffuse_source_t() {};
      virtual void preprocess();
      amb1rotator_t audio;
      double falloff;
      bool active;
      wave_t& rmslevel;
    };

    class receiver_data_t {
    public:
      receiver_data_t(){};
      virtual ~receiver_data_t(){};
    };

    class boundingbox_t : public dynobject_t {
    public:
      boundingbox_t(xmlpp::Element*);
      void write_xml();
      pos_t size;
      double falloff;
      bool active;
    };

    class mask_t : public shoebox_t {
    public:
      mask_t();
      double gain(const pos_t& p);
      double inv_falloff;
      bool mask_inner;
      bool active;
    };

    class receiver_t : public receivermod_t {
    public:
      receiver_t(xmlpp::Element* xmlsrc);
      void write_xml();
      void prepare(double srate, uint32_t fragsize);
      void clear_output();
      void add_pointsource(const pos_t& prel, const wave_t& chunk, receivermod_base_t::data_t*);
      void add_diffusesource(const amb1wave_t& chunk, receivermod_base_t::data_t*);
      void update_refpoint(const pos_t& psrc_physical, const pos_t& psrc_virtual, pos_t& prel, double& distamnce, double& gain);
      void set_next_gain(double gain);
      void apply_gain();
      void post_proc();
      // configuration/control variables:
      TASCAR::pos_t size;
      bool render_point;
      bool render_diffuse;
      bool render_image;
      uint32_t ismmin;
      uint32_t ismmax;
      bool is_direct;
      bool use_global_mask;
      double diffusegain;
      double falloff;
      double delaycomp;
      // derived / internal / updated variables:
      std::vector<wave_t> outchannels;
      pos_t position;
      zyx_euler_t orientation;
      bool active;
      TASCAR::Acousticmodel::boundingbox_t boundingbox;
      bool gain_zero;
    private:
      double x_gain;
      double dx_gain;
      float dt;
      double next_gain;
    };

    class filter_coeff_t {
    public:
      filter_coeff_t() { c[0] = 1.0; c[1] = 0.0;};
      double c[2];
    };

    class obstacle_t : public diffractor_t {
    public:
      obstacle_t();
      bool active;
      float transmission;
    };

    class reflector_t : public diffractor_t {
    public:
      reflector_t();
      bool active;
      double reflectivity;
      double damping;
      bool edgereflection;
    };

    /** \brief A mirrored source.
     */
    class mirrorsource_t : public pointsource_t {
    public:
      mirrorsource_t(pointsource_t* src,reflector_t* reflector);
      pos_t get_effective_position(const pos_t& receiverp,double& gain);
      virtual pos_t get_physical_position() const { return src_->get_physical_position();};
      void process();
      reflector_t* get_reflector() const { return reflector_;};
      //pos_t get_mirror_position() const { return mirror_position;};
    public:
      pointsource_t* src_;
      reflector_t* reflector_;
    private:
      //Acousticmodel::filter_coeff_t flt_current;
      //double dt;
      //double g, dg;
      double lpstate;
    public:
      pos_t p_img;
      pos_t p_cut;
      bool b_0_14;
    };

    /** \brief Create mirror sources from primary sources and reflectors.
     */
    class mirror_model_t {
    public:
      mirror_model_t(const std::vector<pointsource_t*>& pointsources,
                     const std::vector<reflector_t*>& reflectors,uint32_t order,bool b_0_14);
      ~mirror_model_t();
      /** \brief Process all mirror sources
       */
      void process();
      std::vector<mirrorsource_t*> get_mirror_sources();
      std::vector<pointsource_t*> get_sources();
    private:
      std::vector<mirrorsource_t*> mirrorsource;
    };

    /** \brief A model for a sound wave propagating from a point source to a receiver
     *
     * Processing includes delay, gain, air absorption, and optional
     * obstacles.
     */
    class acoustic_model_t {
    public:
      acoustic_model_t(double c,double fs,uint32_t chunksize,pointsource_t* src,receiver_t* receiver,const std::vector<obstacle_t*>& obstacles = std::vector<obstacle_t*>(0u,NULL));
      ~acoustic_model_t();
      /** \brief Read audio from source, process and add to receiver.
       */
      uint32_t process();
      double get_gain() const { return gain;};
    protected:
      double c_;
      double fs_;
    public:
      pointsource_t* src_;
      receiver_t* receiver_;
      pos_t effective_srcpos;
    protected:
      receivermod_base_t::data_t* receiver_data;
      std::vector<obstacle_t*> obstacles_;
      std::vector<diffractor_t::state_t> vstate;
      wave_t audio;
      uint32_t chunksize;
      double dt;
      double distance;
      double gain;
      double dscale;
      double air_absorption;
      varidelay_t delayline;
      float airabsorption_state;
      //bool first;
    };

    /** \brief A model for a sound wave propagating from a point source to a receiver
     *
     * Processing includes delay, gain, air absorption, and optional
     * obstacles.
     */
    class diffuse_acoustic_model_t {
    public:
      diffuse_acoustic_model_t(double fs,uint32_t chunksize,diffuse_source_t* src,receiver_t* receiver);
      ~diffuse_acoustic_model_t();
      /** \brief Read audio from source, process and add to receiver.
       */
      uint32_t process();
    protected:
      diffuse_source_t* src_;
      receiver_t* receiver_;
      receivermod_base_t::data_t* receiver_data;
      amb1rotator_t audio;
      uint32_t chunksize;
      double dt;
      double gain;
    };

    /** \brief The render model of an acoustic scenario.
     *
     * A world creates a set of acoustic models, one for each
     * combination of a sound source (primary or mirrored) and a receiver.
     */
    class world_t {
    public:
      /** \brief Create a world of acoustic models.
       *
       * \param sources Pointers to primary sound sources
       * \param reflectors Pointers to reflector objects
       * \param receivers Pointers to render receivers
       *
       * A mirror model is created from the reflectors and primary sources.
       */
      world_t(double c,double fs,uint32_t chunksize,const std::vector<pointsource_t*>& sources,const std::vector<diffuse_source_t*>& diffusesources,const std::vector<reflector_t*>& reflectors,const std::vector<obstacle_t*>& obstacles,const std::vector<receiver_t*>& receivers,const std::vector<mask_t*>& masks,uint32_t mirror_order,bool b_0_14);
      ~world_t();
      /** \brief Process the mirror model and all acoustic models.
       */
      void process();
      uint32_t get_active_pointsource() const {return active_pointsource;};
      uint32_t get_active_diffusesource() const {return active_diffusesource;};
      uint32_t get_total_pointsource() const {return acoustic_model.size();};
      uint32_t get_total_diffusesource() const {return diffuse_acoustic_model.size();};
    protected:
      mirror_model_t mirrormodel;
    public:
      std::vector<acoustic_model_t*> acoustic_model;
      std::vector<diffuse_acoustic_model_t*> diffuse_acoustic_model;
      std::vector<receiver_t*> receivers_;
      std::vector<mask_t*> masks_;
      uint32_t active_pointsource;
      uint32_t active_diffusesource;
    };

  }

}

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
