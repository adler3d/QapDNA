/*
* The author of this software is Steven Fortune.  Copyright (c) 1994 by AT&T
* Bell Laboratories.
* Permission to use, copy, modify, and distribute this software for any
* purpose without fee is hereby granted, provided that this entire notice
* is included in all copies of any software which is or includes a copy
* or modification of this software and in all copies of the supporting
* documentation for such software.
* THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
* WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR AT&T MAKE ANY
* REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
* OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
*/

template<class TYPE>
void quick_sort(vector<TYPE>&arr){
  if(arr.empty())return;
  struct t_impl{
    static ptrdiff_t partition(TYPE*arr,ptrdiff_t low,ptrdiff_t high){ 
      auto&pivot=arr[high];int i=(low-1);
      for(int j=low;j<=high-1;j++){
        if(arr[j]<=pivot){i++;std::swap(arr[i],arr[j]);}
      }
      std::swap(arr[i+1],arr[high]);return(i+1);
    };
    static void quickSort(TYPE*arr,ptrdiff_t low,ptrdiff_t high){
      if(low>=high)return;
      ptrdiff_t pivot=partition(arr,low,high);
      quickSort(arr,low,pivot-1);
      quickSort(arr,pivot+1,high);
    }
  };
  t_impl::quickSort(arr.data(),0,arr.size()-1);
}

//template<class FUNC,class TYPE>
//void quick_sort_v2(vector<TYPE>&arr){
//  static_assert(!std::is_same<bool,TYPE>::value,"no way");
//  struct tmp{TYPE v;bool operator<(tmp&ref){return FUNC(v,ref.v);}};
//  quick_sort((vector<tmp>&)arr);
//}
static int vec2dCompare(vec2d&a,vec2d&b){
  if(a.y<b.y)return -1;
  if(a.y>b.y)return 1;
  if(a.x<b.x)return -1;
  if(a.x>b.x)return 1;
  return 0;
}
static bool vec2d_less_or_eq(vec2d&a,vec2d&b){
  return vec2dCompare(a,b)<=0;
}
static int vec2dCompare(const void*p1,const void*p2)
{
  auto&a=*(vec2d*)p1;auto&b=*(vec2d*)p2;
  return vec2dCompare(a,b);
}
struct Freenode{
  Freenode*nextfree;
};

struct FreeNodeArrayList{
  Freenode*memory;
  FreeNodeArrayList*next;
};

struct Freelist{
  Freenode*head;
  int nodesize;
};

struct Site{
  vec2d coord;
  int sitenbr;
  int refcnt;
};

struct VEdge{
  vec2d VA;
  vec2d VB;
  vec2d S0;
  vec2d S1;
};
struct Edge{
  double a,b,c;
  Site*Vertices[2];  // point A,point B
  Site*Sites[2];    // left site,right site
  int edgenbr;
};

struct GraphEdge{
  double x1,y1,x2,y2;
  GraphEdge*next;
};

struct Halfedge{
  Halfedge*ELleft;
  Halfedge*ELright;
  Edge*ELedge;
  int ELrefcnt;
  char ELpm;
  Site*vertex;
  double ystar;
  Halfedge*PQnext;
};

struct Voronoi{
  Voronoi()
  {
    siteidx=0;
    sites.clear();
    allMemoryList=new FreeNodeArrayList;
    allMemoryList->memory=0;
    allMemoryList->next=0;
    currentMemoryBlock=allMemoryList;
    minDistanceBetweenSites=0;
  }
  vector<VEdge> ComputeVoronoiGraph(const vector<vec2d>&p,double minY,double maxY)
  {
    cleanup();
    int i;

    minDistanceBetweenSites=0;

    nsites=p.size();
    plot=0;
    triangulate=0;
    debug=1;
    sorted=0;
    freeinit(&sfl,sizeof(Site));

    sites.resize(nsites);
    xmin=p[0].x;
    ymin=p[0].y;
    xmax=p[0].x;
    ymax=p[0].y;

    for(i=0; i< nsites; i++)
    {
      sites[i].coord=p[i];
      sites[i].sitenbr=i;
      sites[i].refcnt=0;

      if(p[i].x<xmin)
        xmin=p[i].x;
      else if(p[i].x>xmax)
        xmax=p[i].x;

      if(p[i].y<ymin)
        ymin=p[i].y;
      else if(p[i].y>ymax)
        ymax=p[i].y;
    }

    //qsort(sites.data(),nsites,sizeof(*sites.data()),&vec2dCompare);
    
    struct t_hacked_site{Site v;bool operator<=(t_hacked_site&ref){return vec2d_less_or_eq(v.coord,ref.v.coord);}};
    quick_sort((vector<t_hacked_site>&)sites);

    siteidx=0;
    geominit();
    double temp=0;
    if(minY>maxY)
    {
      temp=minY;
      minY=maxY;
      maxY=temp;
    }
    if(minY>maxY)
    {
      temp=minY;
      minY=maxY;
      maxY=temp;
    }
    borderMinX=minY;
    borderMinY=minY;
    borderMaxX=maxY;
    borderMaxY=maxY;

    siteidx=0;
    voronoi(triangulate);

    cleanup();
    clean();
    return total_edges;
  }
private:
  vector<VEdge> total_edges;
  void cleanup()
  {
    FreeNodeArrayList*current=0,*prev=0;

    current=prev=allMemoryList;

    while (current->next!=0)
    {
      prev=current;
      current=current->next;
      free(prev->memory);
      delete prev;
      prev=0;
    }

    if(current!=0&&current->memory!=0)
    {
      free(current->memory);
      delete current;
    }

    allMemoryList=new FreeNodeArrayList;
    allMemoryList->next=0;
    allMemoryList->memory=0;
    currentMemoryBlock=allMemoryList;
  }
  bool  ELinitialize()
  {
    int i;
    freeinit(&hfl,sizeof**ELhash);
    ELhashsize=2*sqrt_nsites;
    ELhash=(Halfedge**)myalloc(sizeof*ELhash*ELhashsize);

    if(ELhash==0)
      return false;

    for(i=0; i<ELhashsize; i += 1)ELhash[i]=(Halfedge*)NULL;
    ELleftend=HEcreate((Edge*)NULL,0);
    ELrightend=HEcreate((Edge*)NULL,0);
    ELleftend->ELleft=(Halfedge*)NULL;
    ELleftend->ELright=ELrightend;
    ELrightend->ELleft=ELleftend;
    ELrightend->ELright=(Halfedge*)NULL;
    ELhash[0]=ELleftend;
    ELhash[ELhashsize-1]=ELrightend;

    return true;
  }
  void geominit()
  {
    double sn;

    freeinit(&efl,sizeof(Edge));
    nvertices=0;
    nedges=0;
    sn=(double)nsites+4;
    sqrt_nsites=(int)sqrt(sn);
    deltay=ymax-ymin;
    deltax=xmax-xmin;
  }
  Halfedge*HEcreate(Edge*e,int pm)
  {
    Halfedge*answer;
    answer=(Halfedge*)getfree(&hfl);
    answer->ELedge=e;
    answer->ELpm=pm;
    answer->PQnext=(Halfedge*)NULL;
    answer->vertex=(Site*)NULL;
    answer->ELrefcnt=0;
    return answer;
  }
  void ELinsert(Halfedge*lb,Halfedge*newHe)
  {
    newHe->ELleft=lb;
    newHe->ELright=lb->ELright;
    (lb->ELright)->ELleft=newHe;
    lb->ELright=newHe;
  }
  Halfedge*ELgethash(int b)
  {
    Halfedge*he;

    if(b<0 || b >= ELhashsize)
      return((Halfedge*)NULL);
    he=ELhash[b];
    if(he==(Halfedge*)NULL || he->ELedge!=(Edge*)-2)
      return (he);

    /*Hash table points to deleted half edge.  Patch as necessary.*/
    ELhash[b]=(Halfedge*)NULL;
    if((he->ELrefcnt -= 1)== 0)
      makefree((Freenode*)he,&hfl);
    return ((Halfedge*)NULL);
  }
  Halfedge*ELleftbnd(vec2d*p)
  {
    int i,bucket;
    Halfedge*he;

    bucket=(int)((p->x-xmin)/ deltax*ELhashsize);

    if(bucket<0)bucket=0;
    if(bucket >= ELhashsize)bucket=ELhashsize-1;

    he=ELgethash(bucket);
    if(he==(Halfedge*)NULL)
    {
      for(i=1; 1; i += 1)
      {
        if((he=ELgethash(bucket-i))!=(Halfedge*)NULL)
          break;
        if((he=ELgethash(bucket+i))!=(Halfedge*)NULL)
          break;
      };
      totalsearch += i;
    };
    ntry += 1;
    if(he==ELleftend || (he!=ELrightend&&right_of(he,p)))
    {
      do
      {
        he=he->ELright;
      } while (he!=ELrightend&&right_of(he,p));
      he=he->ELleft;
    }
    else
      do
      {
        he=he->ELleft;
      } while (he!=ELleftend&&!right_of(he,p));

      if(bucket>0&&bucket <ELhashsize-1)
      {
        if(ELhash[bucket]!=(Halfedge*)NULL)
        {
          ELhash[bucket]->ELrefcnt -= 1;
        }
        ELhash[bucket]=he;
        ELhash[bucket]->ELrefcnt += 1;
      };
      return (he);
  }
  void ELdelete(Halfedge*he){
    (he->ELleft)->ELright=he->ELright;
    (he->ELright)->ELleft=he->ELleft;
    he->ELedge=(Edge*)-2;
  }
  Halfedge*ELright(Halfedge*he)
  {
    return (he->ELright);
  }
  Halfedge*ELleft(Halfedge*he)
  {
    return (he->ELleft);
  }
  Site*leftreg(Halfedge*he)
  {
    if(he->ELedge==(Edge*)NULL)
      return(bottomsite);
    return(he->ELpm==0 ? he->ELedge->Sites[0] : he->ELedge->Sites[1]);
  }
  Site*rightreg(Halfedge*he)
  {
    if(he->ELedge==(Edge*)NULL)
      return(bottomsite);
    return(he->ELpm==0 ? he->ELedge->Sites[1] : he->ELedge->Sites[0]);
  }
  Edge*bisect(Site*s1,Site*s2)
  {
    double dx,dy,adx,ady;
    Edge*newedge;

    newedge=(Edge*)getfree(&efl);

    newedge->Sites[0]=s1; //store the sites that this edge is bisecting
    newedge->Sites[1]=s2;
    ref(s1);
    ref(s2);
    newedge->Vertices[0]=(Site*)NULL; //to begin with,there are no endpoints on the bisector-it goes to infinity
    newedge->Vertices[1]=(Site*)NULL;

    dx=s2->coord.x-s1->coord.x;      //get the difference in x dist between the sites
    dy=s2->coord.y-s1->coord.y;
    adx=dx>0 ? dx : -dx;          //make sure that the difference in positive
    ady=dy>0 ? dy : -dy;
    newedge->c=(double)(s1->coord.x*dx+s1->coord.y*dy+(dx*dx+dy*dy)*0.5);//get the slope of the line

    if(adx>ady)
    {
      newedge->a=1.0; newedge->b=dy/dx; newedge->c/=dx;//set formula of line,with x fixed to 1
    }
    else
    {
      newedge->b=1.0; newedge->a=dx/dy; newedge->c/=dy;//set formula of line,with y fixed to 1
    };

    newedge->edgenbr=nedges;

    nedges += 1;
    return(newedge);
  }
  Site*intersect(Halfedge*el1,Halfedge*el2,vec2d*p=0)
  {
    Edge*e1,*e2,*e;
    Halfedge*el;
    double d,xint,yint;
    int right_of_site;
    Site*v;

    e1=el1->ELedge;
    e2=el2->ELedge;
    if(e1==(Edge*)NULL || e2==(Edge*)NULL)
      return ((Site*)NULL);

    //if the two edges bisect the same parent,return null
    if(e1->Sites[1]==e2->Sites[1])
      return ((Site*)NULL);

    d=e1->a*e2->b-e1->b*e2->a;
    if(-1.0e-10<d&&d<1.0e-10)
      return ((Site*)NULL);

    xint=(e1->c*e2->b-e2->c*e1->b)/ d;
    yint=(e2->c*e1->a-e1->c*e2->a)/ d;

    if((e1->Sites[1]->coord.y<e2->Sites[1]->coord.y)||
      (e1->Sites[1]->coord.y==e2->Sites[1]->coord.y &&
        e1->Sites[1]->coord.x<e2->Sites[1]->coord.x))
    {
      el=el1;
      e=e1;
    }
    else
    {
      el=el2;
      e=e2;
    };

    right_of_site=xint >= e->Sites[1]->coord.x;
    if((right_of_site&&el->ELpm==0)|| (!right_of_site&&el->ELpm==1))
      return ((Site*)NULL);

    //create a new site at the point of intersection-this is a new vector event waiting to happen
    v=(Site*)getfree(&sfl);
    v->refcnt=0;
    v->coord.x=xint;
    v->coord.y=yint;
    return(v);
  }
  int right_of(Halfedge*el,vec2d*p)
  {
    Edge*e;
    Site*topsite;
    int right_of_site,above,fast;
    double dxp,dyp,dxs,t1,t2,t3,yl;

    e=el->ELedge;
    topsite=e->Sites[1];
    right_of_site=p->x>topsite->coord.x;
    if(right_of_site&&el->ELpm==0)return(1);
    if(!right_of_site&&el->ELpm==1)return (0);

    if(e->a==1.0)
    {
      dyp=p->y-topsite->coord.y;
      dxp=p->x-topsite->coord.x;
      fast=0;
      if((!right_of_site & (e->b<0.0))| (right_of_site & (e->b >= 0.0)))
      {
        above=dyp >= e->b*dxp;
        fast=above;
      }
      else
      {
        above=p->x+p->y*e->b>e->c;
        if(e->b<0.0)above=!above;
        if(!above)fast=1;
      };
      if(!fast)
      {
        dxs=topsite->coord.x-(e->Sites[0])->coord.x;
        above=e->b*(dxp*dxp-dyp*dyp)<
          dxs*dyp*(1.0+2.0*dxp/dxs+e->b*e->b);
        if(e->b<0.0)above=!above;
      };
    }
    else  /*e->b==1.0*/
    {
      yl=e->c-e->a*p->x;
      t1=p->y-yl;
      t2=p->x-topsite->coord.x;
      t3=yl-topsite->coord.y;
      above=t1*t1>t2*t2+t3*t3;
    };
    return (el->ELpm==0 ? above : !above);
  }
  void endpoint(Edge*e,int lr,Site*s)
  {
    e->Vertices[lr]=s;
    ref(s);
    if(e->Vertices[1-lr]==(Site*)NULL)
      return;

    clip_line(e);

    deref(e->Sites[0]);
    deref(e->Sites[1]);
    makefree((Freenode*)e,&efl);
  }
  double  dist(Site*s,Site*t)
  {
    double dx,dy;
    dx=s->coord.x-t->coord.x;
    dy=s->coord.y-t->coord.y;
    return (double)(sqrt(dx*dx+dy*dy));
  }
  void makevertex(Site*v)
  {
    v->sitenbr=nvertices;
    nvertices += 1;
  }
  void deref(Site*v)
  {
    v->refcnt -= 1;
    if(v->refcnt==0)
      makefree((Freenode*)v,&sfl);
  }
  void ref(Site*v)
  {
    v->refcnt += 1;
  }
  void PQinsert(Halfedge*he,Site*v,double offset)
  {
    Halfedge*last,*next;

    he->vertex=v;
    ref(v);
    he->ystar=(double)(v->coord.y+offset);
    last=&PQhash[PQbucket(he)];
    while ((next=last->PQnext)!=(Halfedge*)NULL &&
      (he->ystar >next->ystar ||
        (he->ystar==next->ystar&&v->coord.x>next->vertex->coord.x)))
    {
      last=next;
    };
    he->PQnext=last->PQnext;
    last->PQnext=he;
    PQcount += 1;
  }
  void PQdelete(Halfedge*he)
  {
    Halfedge*last;

    if(he->vertex!=(Site*)NULL)
    {
      last=&PQhash[PQbucket(he)];
      while (last->PQnext!=he)
        last=last->PQnext;

      last->PQnext=he->PQnext;
      PQcount -= 1;
      deref(he->vertex);
      he->vertex=(Site*)NULL;
    };
  }
  int PQbucket(Halfedge*he)
  {
    int bucket;

    bucket=(int)((he->ystar-ymin)/ deltay*PQhashsize);
    if(bucket<0)bucket=0;
    if(bucket >= PQhashsize)bucket=PQhashsize-1;
    if(bucket<PQmin)PQmin=bucket;
    return(bucket);
  }
  int PQempty()
  {
    return(PQcount==0);
  }
  vec2d  PQ_min()
  {
    vec2d answer;

    while (PQhash[PQmin].PQnext==(Halfedge*)NULL){ PQmin += 1; };
    answer.x=PQhash[PQmin].PQnext->vertex->coord.x;
    answer.y=PQhash[PQmin].PQnext->ystar;
    return (answer);
  }
  Halfedge*PQextractmin()
  {
    Halfedge*curr;

    curr=PQhash[PQmin].PQnext;
    PQhash[PQmin].PQnext=curr->PQnext;
    PQcount -= 1;
    return(curr);
  }
  bool PQinitialize()
  {
    int i;

    PQcount=0;
    PQmin=0;
    PQhashsize=4*sqrt_nsites;
    PQhash=(Halfedge*)myalloc(PQhashsize*sizeof*PQhash);

    if(PQhash==0)
      return false;

    for(i=0; i<PQhashsize; i += 1)PQhash[i].PQnext=(Halfedge*)NULL;

    return true;
  }
  void freeinit(Freelist*fl,int size)
  {
    fl->head=(Freenode*)NULL;
    fl->nodesize=size;
  }
  char*getfree(Freelist*fl)
  {
    int i;
    Freenode*t;

    if(fl->head==(Freenode*)NULL)
    {
      t=(Freenode*)myalloc(sqrt_nsites*fl->nodesize);

      if(t==0)
        return 0;

      currentMemoryBlock->next=new FreeNodeArrayList;
      currentMemoryBlock=currentMemoryBlock->next;
      currentMemoryBlock->memory=t;
      currentMemoryBlock->next=0;

      for(i=0; i<sqrt_nsites; i += 1)
        makefree((Freenode*)((char*)t+i*fl->nodesize),fl);
    };
    t=fl->head;
    fl->head=(fl->head)->nextfree;
    return((char*)t);
  }
  void makefree(Freenode*curr,Freelist*fl)
  {
    curr->nextfree=fl->head;
    fl->head=curr;
  }
  char*myalloc(unsigned n)
  {
    char*t=0;
    t=(char*)malloc(n);
    total_alloc += n;
    return(t);
  }
  //void line(double x1,double y1,double x2,double y2){pushGraphEdge(x1,y1,x2,y2);}
  void clip_line(Edge*e)
  {
    Site*s1,*s2;
    double x1=0,x2=0,y1=0,y2=0,temp=0;;

    x1=e->Sites[0]->coord.x;
    x2=e->Sites[1]->coord.x;
    y1=e->Sites[0]->coord.y;
    y2=e->Sites[1]->coord.y;

    //if the distance between the two points this line was created from is less than 
    //the square root of 2,then ignore it
    if(sqrt(((x2-x1)*(x2-x1))+((y2-y1)*(y2-y1)))<minDistanceBetweenSites)
    {
      return;
    }
    pxmin=borderMinX;
    pxmax=borderMaxX;
    pymin=borderMinY;
    pymax=borderMaxY;

    if(e->a==1.0&&e->b >= 0.0)
    {
      s1=e->Vertices[1];
      s2=e->Vertices[0];
    }
    else
    {
      s1=e->Vertices[0];
      s2=e->Vertices[1];
    };

    if(e->a==1.0)
    {
      y1=pymin;
      if(s1!=(Site*)NULL&&s1->coord.y>pymin)
        y1=s1->coord.y;
      if(y1>pymax)
        y1=pymax;
      x1=e->c-e->b*y1;
      y2=pymax;
      if(s2!=(Site*)NULL&&s2->coord.y<pymax)
        y2=s2->coord.y;

      if(y2<pymin)
        y2=pymin;
      x2=(e->c)- (e->b)*y2;
      if(((x1> pxmax)& (x2>pxmax))| ((x1<pxmin)&(x2<pxmin)))
        return;
      if(x1> pxmax)
      {
        x1=pxmax; y1=(e->c-x1)/ e->b;
      };
      if(x1<pxmin)
      {
        x1=pxmin; y1=(e->c-x1)/ e->b;
      };
      if(x2>pxmax)
      {
        x2=pxmax; y2=(e->c-x2)/ e->b;
      };
      if(x2<pxmin)
      {
        x2=pxmin; y2=(e->c-x2)/ e->b;
      };
    }
    else
    {
      x1=pxmin;
      if(s1!=(Site*)NULL&&s1->coord.x>pxmin)
        x1=s1->coord.x;
      if(x1>pxmax)
        x1=pxmax;
      y1=e->c-e->a*x1;
      x2=pxmax;
      if(s2!=(Site*)NULL&&s2->coord.x<pxmax)
        x2=s2->coord.x;
      if(x2<pxmin)
      {
        x2=pxmin;
      }
      y2=e->c-e->a*x2;
      if(((y1> pymax)& (y2>pymax))| ((y1<pymin)&(y2<pymin)))
        return;
      if(y1> pymax)
      {
        y1=pymax; x1=(e->c-y1)/ e->a;
      };
      if(y1<pymin)
      {
        y1=pymin; x1=(e->c-y1)/ e->a;
      };
      if(y2>pymax)
      {
        y2=pymax; x2=(e->c-y2)/ e->a;
      };
      if(y2<pymin)
      {
        y2=pymin; x2=(e->c-y2)/ e->a;
      };
    };

    VEdge ee;
    ee.S0=e->Sites[0]->coord;
    ee.S1=e->Sites[1]->coord;
    ee.VA=vec2d(x1,y1);
    ee.VB=vec2d(x2,y2);

    total_edges.push_back(ee);
    //line(x1,y1,x2,y2);
  }
  bool  voronoi(int triangulate)
  {
    Site*newsite,*bot,*top,*temp,*p;
    Site*v;
    vec2d newintstar;
    int pm;
    Halfedge*lbnd,*rbnd,*llbnd,*rrbnd,*bisector;
    Edge*e;

    PQinitialize();
    bottomsite=nextone();
    bool retval=ELinitialize();

    if(!retval)
      return false;

    newsite=nextone();
    while (1)
    {

      if(!PQempty())
        newintstar=PQ_min();
      if(newsite!=(Site*)NULL&&(PQempty()|| newsite->coord.y<newintstar.y
        || (newsite->coord.y==newintstar.y&&newsite->coord.x<newintstar.x)))
      {
        lbnd=ELleftbnd(&(newsite->coord));
        rbnd=ELright(lbnd);
        bot=rightreg(lbnd);
        e=bisect(bot,newsite);
        bisector=HEcreate(e,0);
        ELinsert(lbnd,bisector);

        if((p=intersect(lbnd,bisector))!=(Site*)NULL)
        {
          PQdelete(lbnd);
          PQinsert(lbnd,p,dist(p,newsite));
        };
        lbnd=bisector;
        bisector=HEcreate(e,1);
        ELinsert(lbnd,bisector);

        if((p=intersect(bisector,rbnd))!=(Site*)NULL)
        {
          PQinsert(bisector,p,dist(p,newsite));
        };
        newsite=nextone();
      }
      else if(!PQempty())
      {
        lbnd=PQextractmin();
        llbnd=ELleft(lbnd);
        rbnd=ELright(lbnd);
        rrbnd=ELright(rbnd);
        bot=leftreg(lbnd);
        top=rightreg(rbnd);
        v=lbnd->vertex;
        makevertex(v);
        endpoint(lbnd->ELedge,lbnd->ELpm,v);
        endpoint(rbnd->ELedge,rbnd->ELpm,v);
        ELdelete(lbnd);
        PQdelete(rbnd);
        ELdelete(rbnd);
        pm=0;
        if(bot->coord.y>top->coord.y)
        {
          temp=bot;
          bot=top;
          top=temp;
          pm=1;
        }
        e=bisect(bot,top);
        bisector=HEcreate(e,pm);
        ELinsert(llbnd,bisector);
        endpoint(e,1-pm,v);
        deref(v);
        if((p=intersect(llbnd,bisector))!=(Site*)NULL)
        {
          PQdelete(llbnd);
          PQinsert(llbnd,p,dist(p,bot));
        };

        //if right HE and the new bisector don't intersect,then reinsert it 
        if((p=intersect(bisector,rrbnd))!=(Site*)NULL)
        {
          PQinsert(bisector,p,dist(p,bot));
        };
      }
      else break;
    };

    for(lbnd=ELright(ELleftend); lbnd!=ELrightend; lbnd=ELright(lbnd))
    {
      e=lbnd->ELedge;

      clip_line(e);
    };

    cleanup();
    return true;
  }
  Site*nextone()
  {
    Site*s;
    if(siteidx<nsites)
    {
      s=&sites[siteidx];
      siteidx += 1;
      return(s);
    }
    else
      return((Site*)NULL);
  }

  void clean()
  {
    delete[] PQhash;
    delete[] currentMemoryBlock;
    delete[] ELhash;
  }

  Freelist  hfl;
  Halfedge*ELleftend,*ELrightend;
  double xmin,xmax,ymin,ymax,deltax,deltay;
  vector<Site> sites;
  Freelist sfl;
  Site*bottomsite;
  Freelist efl;
  Halfedge*PQhash;
  int ntry,totalsearch,total_alloc,PQmin,PQcount,
    PQhashsize,nedges,nsites,siteidx,sqrt_nsites,nvertices,triangulate,sorted,
    plot,debug,ELhashsize;
  double  pxmin,pxmax,pymin,pymax,cradius;
  double borderMinX,borderMaxX,borderMinY,borderMaxY;
  FreeNodeArrayList*allMemoryList;
  FreeNodeArrayList*currentMemoryBlock;
  Halfedge**ELhash;
  double minDistanceBetweenSites;
};