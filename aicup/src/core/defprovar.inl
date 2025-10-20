#define ADD(TYPE,NAME,VALUE)TYPE NAME;
DEF_PRO_VARIABLE(ADD)
#undef ADD
//===
void DoReset()
{
  #define ADD(TYPE,NAME,VALUE)this->NAME=VALUE;
  DEF_PRO_VARIABLE(ADD)
  #undef ADD
}
#ifdef DEF_PRO_CLASSNAME
  #ifndef DEF_PRO_UNSAVEABLE
    struct ProxySys$${
      #ifdef DEF_PRO_TEMPLATE_DATAIO
        template<class TDataIO>
      #endif //DEF_PRO_TEMPLATE_DATAIO
      static void Save(TDataIO&IO,const DEF_PRO_CLASSNAME()&ref)
      {
        #define F(TYPE,NAME,VALUE)Sys$$<TYPE>::Save(IO,ref.NAME);
        DEF_PRO_VARIABLE(F);
        #undef F
      }
      #ifdef DEF_PRO_TEMPLATE_DATAIO
        template<class TDataIO>
      #endif //DEF_PRO_TEMPLATE_DATAIO
      static void Load(TDataIO&IO,DEF_PRO_CLASSNAME()&ref)
      {
        #define F(TYPE,NAME,VALUE)Sys$$<TYPE>::Load(IO,ref.NAME);
        DEF_PRO_VARIABLE(F);
        #undef F
      }
      #ifdef DEF_PRO_TEMPLATE_DATAIO
        #undef DEF_PRO_TEMPLATE_DATAIO
      #endif //DEF_PRO_TEMPLATE_DATAIO
    };
  #endif //DEF_PRO_UNSAVEABLE
  void operator=(DEF_PRO_CLASSNAME()&&ref){oper_set(std::move(ref));}
  DEF_PRO_CLASSNAME()(DEF_PRO_CLASSNAME()&&ref){DoReset();oper_set(std::move(ref));}
  void oper_set(DEF_PRO_CLASSNAME()&&ref)
  {
    #define F(TYPE,NAME,VALUE)this->NAME=std::move(ref.NAME);
    DEF_PRO_VARIABLE(F);
    #undef F
  }
  #ifdef DEF_PRO_COPYABLE
    void operator=(const DEF_PRO_CLASSNAME()&ref){oper_set(ref);}
    DEF_PRO_CLASSNAME()(const DEF_PRO_CLASSNAME()&ref){oper_set(ref);}
    void oper_set(const DEF_PRO_CLASSNAME()&ref)
    {
      #define F(TYPE,NAME,VALUE)this->NAME=ref.NAME;
      DEF_PRO_VARIABLE(F);
      #undef F
    }
    #undef DEF_PRO_COPYABLE
  #endif //DEF_PRO_COPYABLE
  DEF_PRO_CLASSNAME()(){DoReset();}
  #undef DEF_PRO_CLASSNAME
#endif //DEF_PRO_CLASSNAME

#ifdef DEF_PRO_UNSAVEABLE
  #undef DEF_PRO_UNSAVEABLE
#endif

#undef DEF_PRO_VARIABLE