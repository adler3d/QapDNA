struct t_game_slot{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_game_slot
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(string,coder,{})\
  ADD(int,v,0)\
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
  ADD(int,game_id,{})\
  ADD(int,maxtick,20000)\
  ADD(int,stderr_max,1024*64)\
  ADD(double,TL,50.0)\
  ADD(double,TL0,1000.0)\
  //===
  #include "defprovar.inl"
  //===
};
struct t_game_uploaded_ack{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_game_uploaded_ack
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(int,game_id,{})\
  ADD(string,err,{})\
  //===
  #include "defprovar.inl"
  //===
};
struct t_finished_game{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_finished_game
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(int,game_id,{})\
  ADD(vector<double>,slot2ms,{})\
  ADD(vector<double>,slot2score,{})\
  ADD(vector<string>,slot2status,{})\
  ADD(int,tick,0)\
  ADD(int,size,0)\
  //===
  #include "defprovar.inl"
  //===
  //string cdn_file()const{return to_string(game_id);};
  //string serialize()const{return {};}
};
struct t_cdn_game{
  struct t_player{
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_player
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(string,coder,{})\
    ADD(string,v,{})\
    ADD(string,err,{})\
    ADD(vector<double>,tick2ms,{})\
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
  ADD(vector<t_player>,slot2player,{})\
  ADD(vector<vector<string>>,tick2cmds,{})\
  //===
  #include "defprovar.inl"
  //===
  string serialize()const{return QapSaveToStr(*this);}
  int size()const{return serialize().size();}// TODO: need return serialize().size() but optimized!
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