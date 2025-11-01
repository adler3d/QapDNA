struct t_game_uploaded_ack{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_game_uploaded_ack
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(uint64_t,game_id,{})\
  ADD(string,err,{})\
  //===
  #include "defprovar.inl"
  //===
};
struct t_game_slot{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_game_slot
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(uint64_t,uid,{})\
  ADD(string,coder,{})\
  ADD(uint64_t,v,0)\
  ADD(string,cdn_bin_file,{})\
  //===
  #include "defprovar.inl"
  //===
};
struct t_game_decl{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_game_decl
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(vector<t_game_slot>,arr,{})\
  ADD(string,world,"t_splinter")\
  ADD(uint32_t,seed_initial,{})\
  ADD(uint32_t,seed_strategies,{})\
  ADD(string,config,{})\
  ADD(uint64_t,game_id,{})\
  ADD(uint64_t,season,{})\
  ADD(uint64_t,phase,{})\
  ADD(uint64_t,wave,{})\
  ADD(uint64_t,maxtick,20000)\
  ADD(uint64_t,stderr_max,1024*64)\
  ADD(double,TL,750.0)\
  ADD(double,TL0,1000.0)\
  //===
  #include "defprovar.inl"
  //===
};
struct t_finished_game{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_finished_game
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(uint64_t,game_id,{})\
  ADD(vector<double>,slot2ms,{})\
  ADD(vector<double>,slot2score,{})\
  ADD(vector<string>,slot2status,{})\
  ADD(uint64_t,tick,0)\
  ADD(uint64_t,size,0)\
  //===
  #include "defprovar.inl"
  //===
};
struct t_cdn_game{
  struct t_elem{
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_elem
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(string,cmd,{})\
    ADD(float,ms,{})\
    //===
    #include "defprovar.inl"
    //===
  };
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_cdn_game
  #define DEF_PRO_VARIABLE(ADD) \
  ADD(string,version,"t_cdn_game_v0")\
  ADD(t_game_decl,gd,{})\
  ADD(t_finished_game,fg,{})\
  ADD(vector<string>,slot2err,{})\
  ADD(vector<vector<t_elem>>,slot2tick2elem,{})\
  //===
  #include "defprovar.inl"
  //===
  string serialize()const{return QapSaveToStr(*this);}
  uint64_t size()const{return serialize().size();}// TODO: need return serialize().size() but optimized!
};
struct t_game{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_game
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(t_game_decl,gd,{})\
  ADD(t_finished_game,fg,{})\
  ADD(string,ordered_at,{})\
  ADD(string,finished_at,{})\
  ADD(string,status,"new")\
  ADD(string,author,{})\
  //===
  #include "defprovar.inl"
  //===
};
#include "t_cdn_game_stream.inl"