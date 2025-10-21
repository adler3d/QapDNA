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
  int size()const{return serialize().size();}// TODO: need return serialize().size() but optimized!
};
struct t_cdn_game_stream{
  struct t_gdfgs2e{
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_gdfgs2e
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(t_game_decl,gd,{})\
    ADD(t_finished_game,fg,{})\
    ADD(vector<string>,slot2err,{})\
    //===
    #include "defprovar.inl"
    //===
  };
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_cdn_game_stream
  #define DEF_PRO_VARIABLE(ADD) \
  ADD(string,version,"t_cdn_game_stream_v0")\
  ADD(string,gdfgs2e,{})\
  ADD(string,tick2slot2elem,{})\
  //===
  #include "defprovar.inl"
  //===
  string serialize()const{return QapSaveToStr(*this);}
  int size()const{return serialize().size();}
  void save_to(t_cdn_game&ref){

  }
  void load_from(const t_cdn_game&ref){
    t_gdfgs2e gfs;
    gfs.gd=ref.gd;
    gfs.fg=ref.fg;
    gfs.slot2err=ref.slot2err;
    gdfgs2e=QapSaveToStr(gfs);
    vector<vector<t_cdn_game::t_elem>> tick2slot2elem;
    int max_ticks=0;
    for(const auto& tick2elem:ref.slot2tick2elem){
      if(tick2elem.size()>max_ticks)max_ticks=tick2elem.size();
    }
    vector<vector<t_cdn_game::t_elem>> tick2slot2elem(max_ticks);
    for(auto&vec:tick2slot2elem){
      vec.resize(ref.slot2tick2elem.size());
    }
    for(int slot=0;slot<ref.slot2tick2elem.size();slot++){
      auto&tick2elem=ref.slot2tick2elem[slot];
      for(int tick=0;tick<tick2elem.size();tick++){
        auto&elem=tick2elem[tick];
        tick2slot2elem[tick][slot]=elem;
      }
    }
    TDataIO IO;
    IO.save(tick2slot2elem.size());
    for(auto&slot2elem:tick2slot2elem){
      QapAssert(slot2elem.size()==ref.gd.arr.size());// because of this we don't write ref.gd.arr.size() here
      for(auto&elem:slot2elem){
        QapSave(IO,elem);
      }
    }
    this->tick2slot2elem=QapSaveToStr(IO.IO.mem);
  }
};
//struct t_cdn_game_builder{
//  t_cdn_game&out;
//  //... add here some fields needed for streaming like buffers/status
//  void feed(const string&fragment){/*implement this*/}
//};
struct t_cdn_game_builder {
  t_cdn_game& out;
  std::string buffer;    // накопление данных
  size_t parse_pos = 0;  // позиция чтения в буфере

  int32_t tick_count = -1;
  int32_t current_tick = 0;
  int32_t current_slot = 0;
  std::vector<int32_t> slot_counts;
  std::vector<std::vector<t_cdn_game::t_elem>> parsed_data;

  static bool read_int32(const std::string& s, size_t& pos, int32_t& val) {
    if (pos + 4 > s.size()) return false;
    memcpy(&val, s.data() + pos, 4);
    pos += 4;
    return true;
  }
  static bool read_float(const std::string& s, size_t& pos, float& val) {
    if (pos + 4 > s.size()) return false;
    memcpy(&val, s.data() + pos, 4);
    pos += 4;
    return true;
  }
  static bool read_string(const std::string& s, size_t& pos, std::string& val) {
    int32_t sz;
    if (!read_int32(s, pos, sz)) return false;
    if (pos + sz > s.size()) return false;
    val.assign(s.data() + pos, sz);
    pos += sz;
    return true;
  }
  static bool deserialize_elem(const std::string& s, size_t& pos, t_cdn_game::t_elem& elem) {
    if (!read_string(s, pos, elem.cmd)) return false;
    if (!read_float(s, pos, elem.ms)) return false;
    return true;
  }

  bool feed(const std::string& fragment) {
    buffer.append(fragment);

    // Читаем tick_count, если не прочитан
    if (tick_count == -1) {
      if (!read_int32(buffer, parse_pos, tick_count)) {
        return true; // ждём больше данных
      }
      parsed_data.resize(tick_count);
    }

    // Читаем размеры внутренних векторов slot_counts
    while ((int)slot_counts.size() < tick_count) {
      int32_t slot_count = 0;
      if (!read_int32(buffer, parse_pos, slot_count)) {
        return true; // ждём больше данных
      }
      slot_counts.push_back(slot_count);
      parsed_data[slot_counts.size()-1].resize(slot_count);
    }

    // Следующие этапы - чтение элементов по current_tick и current_slot пока хватает данных...

    // Тут будет продолжение кода

    return true; // пока структура не завершена
  }
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