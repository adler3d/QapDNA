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
  void save_to(t_cdn_game&ref){QapNoWay();}
  void load_from(const t_cdn_game&ref){
    t_gdfgs2e gfs;
    gfs.gd=ref.gd;
    gfs.fg=ref.fg;
    gfs.slot2err=ref.slot2err;
    gdfgs2e=QapSaveToStr(gfs);
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
    int32_t ticks=tick2slot2elem.size();
    IO.save(ticks);
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
  bool feed_to_version() {
    // Проверяем и считываем фиксированную версию из буфера
    // Предположим, версия сериализована как строка с размером
    if (!read_string(buffer, parse_pos, version_str))
      return true; // ждем дополнений
    // Тут можно проверить version_str, например сравнить с "t_cdn_game_stream_v0"
    // Пока просто не передаем в out, но можно сохранить в поле
    return false;
  }

  bool feed_to_gdfgs2e() {
    // Предположим, сериализация gdfgs2e тоже как строка с размером
    if (!read_string(buffer, parse_pos, gdfgs2e_str))
      return true;
    // Десериализуем gdfgs2e из строки
    t_cdn_game_stream::t_gdfgs2e gdfgs2e_obj;
    bool ok = QapLoadFromStr(gdfgs2e_obj, gdfgs2e_str);
    if (!ok)
      throw std::runtime_error("Failed to deserialize gdfgs2e");
    out.gd = gdfgs2e_obj.gd;
    out.fg = gdfgs2e_obj.fg;
    out.slot2err = std::move(gdfgs2e_obj.slot2err);
    slot_count = static_cast<int32_t>(out.gd.arr.size());
    return false;
  }

  bool feed(const std::string& fragment) {
    buffer.append(fragment);

    while (true) {
      switch (state) {
      case State::Version:
        if (feed_to_version())
          return true; // ждём продолжения
        state = State::GDFGS2E;
        break;
      case State::GDFGS2E:
        if (feed_to_gdfgs2e())
          return true;
        state = State::Slot2Tick2Elem;
        break;
      case State::Slot2Tick2Elem:
        if (feed_to_s2t2e(fragment)) // feed для slot2tick2elem
          return true;
        state = State::Done;
        break;
      case State::Done:
        return false; // загрузка полная
      }
    }
  }
  t_cdn_game& out;

  std::string buffer;
  size_t parse_pos = 0;

  enum class State {
    Version,
    GDFGS2E,
    Slot2Tick2Elem,
    Done
  } state = State::Version;

  std::string version_str;
  std::string gdfgs2e_str;

  int32_t tick_count = -1;
  int32_t current_tick = 0;
  int32_t current_slot = 0;
  int32_t slot_count = -1;

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

  bool feed_to_s2t2e(const std::string& fragment) {
    buffer.append(fragment);

    if (tick_count == -1) {
      if (!read_int32(buffer, parse_pos, tick_count)) return true; // ждём данных
      slot_count = static_cast<int32_t>(out.gd.arr.size());
    }

    // Добавляем новые слоты в out.slot2tick2elem при необходимости
    while ((int)out.slot2tick2elem.size() <= current_slot) {
      out.slot2tick2elem.emplace_back(); // создаём новую строку (слот)
    }
    // Добавляем тики во внутренний вектор слота по необходимости
    while ((int)out.slot2tick2elem[current_slot].size() <= current_tick) {
      out.slot2tick2elem[current_slot].emplace_back(); // по одному элементу (тик)
    }

    // Читаем элементы пока хватает данных
    while (current_tick < tick_count) {
      while (current_slot < slot_count) {
        // Проверяем опять размер вектора на текущий слот и тик
        if ((int)out.slot2tick2elem.size() <= current_slot) {
          out.slot2tick2elem.emplace_back();
        }
        if ((int)out.slot2tick2elem[current_slot].size() <= current_tick) {
          out.slot2tick2elem[current_slot].emplace_back();
        }
        t_cdn_game::t_elem& elem = out.slot2tick2elem[current_slot][current_tick];
        if (!deserialize_elem(buffer, parse_pos, elem)) {
          return true; // ждем следующих данных
        }
        ++current_slot;

        // При переходе к следующему тикту сбрасываем слот
        if (current_slot == slot_count) {
          current_slot = 0;
          ++current_tick;
        }
      }
    }

    // Всё успешно прочитано
    return false;
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