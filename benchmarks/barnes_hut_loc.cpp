/*
    Lonestar BarnesHut: Simulation of the gravitational forces in a
    galactic cluster using the Barnes-Hut n-body algorithm

    Author: Martin Burtscher
    Center for Grid and Distributed Computing
    The University of Texas at Austin

    Copyright (C) 2007, 2008 The University of Texas at Austin

    Licensed under the Eclipse Public License, Version 1.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

      http://www.eclipse.org/legal/epl-v10.html

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

    File: BarnesHut.cpp
    Modified: Feb. 19, 2008 by Martin Burtscher (initial C++ version)
*/



#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <sys/time.h>
#include <algorithm>
#include <dparallel_recursion/dparallel_recursion.h>
#include "dparallel_recursion/DRange.h"
//#include "../tests/NoisyFillableAliasVector.h"
#include "dparallel_recursion/FillableAliasVector.h"

using namespace dpr;

static double dtime;  // length of one time step
static double eps;  // potential softening parameter
static double tol;  // tolerance for stopping recursion, should be less than 0.57 for 3D case to bound error

static double dthf, epssq, itolsq;

static int step;

enum {CELL, BODY};



class OctTreeLeafNode;

static int nbodies;  // number of bodies in system
static int timesteps;  // number of time steps to run

typedef FillableAliasVector<OctTreeLeafNode> OctTreeLeafNode_vec_t;
OctTreeLeafNode_vec_t * Buf;

static OctTreeLeafNode **body;  // the n bodies
static OctTreeLeafNode *Actual_bodies;

int rank, nprocs;
int tasks_per_thread = 1, nwanted_tasks;
int nthreads = 8;

class OctTreeNode {
  public:
    int type;  // CELL or BODY
    double mass;
    double posx;
    double posy;
    double posz;
};



class OctTreeInternalNode: public OctTreeNode {  // the internal nodes are cells that summarize their children's properties
  public:
    static OctTreeInternalNode *NewNode(const double px, const double py, const double pz);
    static void RecycleTree() {freelist = head;}

  private:
    static OctTreeInternalNode *head, *freelist;  // free list for recycling


  public:
    void Insert(OctTreeLeafNode * const b, double r);  // builds the tree
    void ComputeCenterOfMass(int &curr);  // recursively summarizes info about subtrees
  
    OctTreeNode *child[8];

  
  private:
    OctTreeInternalNode *link;  // links all internal tree nodes so they can be recycled
};



class OctTreeLeafNode: public OctTreeNode {  // the tree leaves are the bodies
  public:
    OctTreeLeafNode();
   ~OctTreeLeafNode() {}
  
    void setVelocity(const double x, const double y, const double z) {velx = x;  vely = y;  velz = z;}

    void Advance();  // advances a body's velocity and position by one time step
    void ComputeForce(const OctTreeInternalNode * const root, const double size);  // computes the acceleration and velocity of a body

  private:
    void RecurseForce(const OctTreeNode * const n, double dsq);  // recursively walks the tree to compute the force on a body

    double velx;
    double vely;
    double velz;
    double accx;
    double accy;
    double accz;
};

BOOST_IS_BITWISE_SERIALIZABLE(OctTreeLeafNode);

OctTreeInternalNode *OctTreeInternalNode::head = NULL;
OctTreeInternalNode *OctTreeInternalNode::freelist = NULL;

struct Body : public EmptyBody<Range, OctTreeLeafNode_vec_t> {

  static OctTreeLeafNode_vec_t base(const Range& r) {
    // printf("B[%d, %d) on %p -> %p\n", r.start, r.end, Buf, Buf->data());
    for (int i = r.start; i < r.end; i++) {
      body[i]->Advance();  // advance the position and velocity of each body
      (*Buf)[i] = *body[i];
    }
    return Buf->range(r.start, r.end);
  }
  
  static OctTreeLeafNode_vec_t post(const Range& r, OctTreeLeafNode_vec_t* res) {
    //printf("J%d[%d, %d)\n", r.nchildren, r.start, r.end);
    res->shallowResize(r.exclusiveSize());
    return *res;
  }
  
};

OctTreeInternalNode *OctTreeInternalNode::NewNode(const double px, const double py, const double pz)
{
  OctTreeInternalNode *in;

  if (freelist == NULL) {
    in = new OctTreeInternalNode();
    in->link = head;
    head = in;
  } else {  // get node from freelist
    in = freelist;
    freelist = freelist->link;
  }

  in->type = CELL;
  in->mass = 0.0;
  in->posx = px;
  in->posy = py;
  in->posz = pz;
  for (int i = 0; i < 8; i++) in->child[i] = NULL;

  return in;
}


void OctTreeInternalNode::Insert(OctTreeLeafNode * const b, double r)  // builds the tree
{
  OctTreeInternalNode *p = this;
  do {
    int i = 0;
    double x = 0.0, y = 0.0, z = 0.0;
    
    if (p->posx < b->posx) {i = 1;  x = r;}
    if (p->posy < b->posy) {i += 2;  y = r;}
    if (p->posz < b->posz) {i += 4;  z = r;}
    
    if (p->child[i] == NULL) {
      p->child[i] = b;
      return;
    }
    
    r = 0.5 * r;
    
    if (p->child[i]->type == CELL) {
      p = (OctTreeInternalNode *)(p->child[i]);
    } else {
      OctTreeInternalNode * const cell = NewNode(p->posx - r + x, p->posy - r + y, p->posz - r + z);
      cell->Insert((OctTreeLeafNode *)(p->child[i]), r);
      p->child[i] = cell;
      //cell->Insert(b, rh);
      p = cell;
    }
  } while(1);
}

void OctTreeInternalNode::ComputeCenterOfMass(int &curr)  // recursively summarizes info about subtrees
{
  double m, px = 0.0, py = 0.0, pz = 0.0;
  OctTreeNode *ch;

  int j = 0;
  mass = 0.0;
  for (int i = 0; i < 8; i++) {
    ch = child[i];
    if (ch != NULL) {
      child[i] = NULL;  // move non-NULL children to the front (needed to make other code faster)
      child[j++] = ch;

      if (ch->type == BODY) {
        body[curr++] = (OctTreeLeafNode *)ch;  // sort bodies in tree order (approximation of putting nearby nodes together for locality)
      } else {
        ((OctTreeInternalNode *)ch)->ComputeCenterOfMass(curr);
      }
      m = ch->mass;
      mass += m;
      px += ch->posx * m;
      py += ch->posy * m;
      pz += ch->posz * m;
    }
  }

  m = 1.0 / mass;
  posx = px * m;
  posy = py * m;
  posz = pz * m;
}

OctTreeLeafNode::OctTreeLeafNode()
{
  type = BODY;
  mass = 0.0;
  posx = 0.0;
  posy = 0.0;
  posz = 0.0;
  velx = 0.0;
  vely = 0.0;
  velz = 0.0;
  accx = 0.0;
  accy = 0.0;
  accz = 0.0;
}

void OctTreeLeafNode::Advance()  // advances a body's velocity and position by one time step
{
  double dvelx, dvely, dvelz;
  double velhx, velhy, velhz;

  dvelx = accx * dthf;
  dvely = accy * dthf;
  dvelz = accz * dthf;

  velhx = velx + dvelx;
  velhy = vely + dvely;
  velhz = velz + dvelz;

  posx += velhx * dtime;
  posy += velhy * dtime;
  posz += velhz * dtime;

  velx = velhx + dvelx;
  vely = velhy + dvely;
  velz = velhz + dvelz;
}


void OctTreeLeafNode::ComputeForce(const OctTreeInternalNode * const root, const double size)  // computes the acceleration and velocity of a body
{
  double ax, ay, az;

  ax = accx;
  ay = accy;
  az = accz;

  accx = 0.0;
  accy = 0.0;
  accz = 0.0;

  RecurseForce(root, size * size * itolsq);

  if (step > 0) {
    velx += (accx - ax) * dthf;
    vely += (accy - ay) * dthf;
    velz += (accz - az) * dthf;
  }
}


void OctTreeLeafNode::RecurseForce(const OctTreeNode * const n, double dsq)  // recursively walks the tree to compute the force on a body
{
  double drx, dry, drz, drsq, nphi, scale, idr;

  drx = n->posx - posx;
  dry = n->posy - posy;
  drz = n->posz - posz;
  drsq = drx*drx + dry*dry + drz*drz;
  if (drsq < dsq) {
    if (n->type == CELL) {
      OctTreeInternalNode *in = (OctTreeInternalNode *)n;
      dsq *= 0.25;
      if (in->child[0] != NULL) {
        RecurseForce(in->child[0], dsq);
        if (in->child[1] != NULL) {
          RecurseForce(in->child[1], dsq);
          if (in->child[2] != NULL) {
            RecurseForce(in->child[2], dsq);
            if (in->child[3] != NULL) {
              RecurseForce(in->child[3], dsq);
              if (in->child[4] != NULL) {
                RecurseForce(in->child[4], dsq);
                if (in->child[5] != NULL) {
                  RecurseForce(in->child[5], dsq);
                  if (in->child[6] != NULL) {
                    RecurseForce(in->child[6], dsq);
                    if (in->child[7] != NULL) {
                      RecurseForce(in->child[7], dsq);
                    }
                  }
                }
              }
            }
          }
        }
      }
    } else {  // n is a body
      if (n != this) {
        drsq += epssq;
        idr = 1 / sqrt(drsq);
        nphi = n->mass * idr;
        scale = nphi * idr * idr;
        accx += drx * scale;
        accy += dry * scale;
        accz += drz * scale;
      }
    }
  } else {  // node is far enough away, don't recurse any deeper
    drsq += epssq;
    idr = 1 / sqrt(drsq);
    nphi = n->mass * idr;
    scale = nphi * idr * idr;
    accx += drx * scale;
    accy += dry * scale;
    accz += drz * scale;
  }
}

static inline void ReadInput(char *filename, int nreps)
{
  double vx, vy, vz;
  FILE *f;
  int tmp_nbodies;
  
  f = fopen(filename, "r+t");
  if (f == NULL) {
    fprintf(stderr, "file not found: %s\n", filename);
    exit(-1);
  }

  fscanf(f, "%d", &tmp_nbodies);
  fscanf(f, "%d", &timesteps);
  fscanf(f, "%lf", &dtime);
  fscanf(f, "%lf", &eps);
  fscanf(f, "%lf", &tol);

  nbodies = tmp_nbodies * nreps;
  dthf = 0.5 * dtime;
  epssq = eps * eps;
  itolsq = 1.0 / (tol * tol);

  if (body == NULL) {
    if (rank == 0) {
      printf("configuration: %d bodies, %d time steps, %d reps\n", tmp_nbodies, timesteps, nreps);
    }
    body = new OctTreeLeafNode*[nbodies];
    //The extra nprocs - 1 is to be able to make a MPI_Allgather
    Actual_bodies = new OctTreeLeafNode[nbodies + nprocs - 1];
  }

  for (int i = 0; i < nbodies; i++) body[i] = &Actual_bodies[i];

  for (int i = 0; i < tmp_nbodies; i++) {
    fscanf(f, "%lE", &(body[i]->mass));
    fscanf(f, "%lE", &(body[i]->posx));
    fscanf(f, "%lE", &(body[i]->posy));
    fscanf(f, "%lE", &(body[i]->posz));
    fscanf(f, "%lE", &vx);
    fscanf(f, "%lE", &vy);
    fscanf(f, "%lE", &vz);
    body[i]->setVelocity(vx, vy, vz);
  }

  fclose(f);
  
  srand(1234);
  for (int i = tmp_nbodies; i < nbodies; i++) {
    *(body[i]) = *(body[i - tmp_nbodies]);
    body[i]->posx *= (1280 - 128 + (rand() & 0xff)) / 1280.0;
    body[i]->posy *= (1280 - 128 + (rand() & 0xff)) / 1280.0;
    body[i]->posz *= (1280 - 128 + (rand() & 0xff)) / 1280.0;
  }
  
}

struct a6_t { double minx, miny, minz, maxx, maxy, maxz; };

a6_t myreduce(a6_t mm, const a6_t& b) noexcept
{
  if(mm.minx > b.minx) mm.minx = b.minx;
  if(mm.miny > b.miny) mm.miny = b.miny;
  if(mm.minz > b.minz) mm.minz = b.minz;

  if(mm.maxx < b.maxx) mm.maxx = b.maxx;
  if(mm.maxy < b.maxy) mm.maxy = b.maxy;
  if(mm.maxz < b.maxz) mm.maxz = b.maxz;

  return mm;
}

static inline void ComputeCenterAndDiameter(const int n, double &diameter, double &centerx, double &centery, double &centerz)
{ a6_t res;
  
  int i;
  pr_pfor_reduce(i, 0, n, nwanted_tasks, res, a6_t({1.0E90, 1.0E90, 1.0E90, -1.0E90, -1.0E90, -1.0E90}), myreduce,
                 res = myreduce(res, {Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz, Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz}); );

  diameter = res.maxx - res.minx;
  if (diameter < (res.maxy - res.miny)) diameter = (res.maxy - res.miny);
  if (diameter < (res.maxz - res.minz)) diameter = (res.maxz - res.minz);
  
  centerx = (res.maxx + res.minx) * 0.5;
  centery = (res.maxy + res.miny) * 0.5;
  centerz = (res.maxz + res.minz) * 0.5;
}

void print_double(FILE *f, double d, bool newline)
{
  int i;
  char str[16];
  
  sprintf(str, "%.4lE", d);
  
  i = 0;
  while ((i < 16) && (str[i] != 0)) {
    if ((str[i] == 'E') && (str[i+1] == '-') && (str[i+2] == '0') && (str[i+3] == '0')) {
      fprintf(f, "E00");
      i += 3;
    } else if (str[i] != '+') {
      fprintf(f, "%c", str[i]);
    }
    i++;
  }
  
  fprintf(f, newline ? "\n" : " ");
}

void dump_positions(const char *filename)
{ FILE *f;
  
  f = fopen(filename, "wt");
  for (int i = 0; i < nbodies; i++) {
    print_double(f, Actual_bodies[i].posx, false);
    print_double(f, Actual_bodies[i].posy, false);
    print_double(f, Actual_bodies[i].posz, true);
  }
  fclose(f);
}

static OctTreeInternalNode *groot;
static double gdiameter;

int main(int argc, char *argv[])
{
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
  if (getenv("OMP_NUM_THREADS"))
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  
  pr_init(nthreads);

  if (argc > 4)
    tasks_per_thread = atoi(argv[4]);
  
  if (rank == 0) {
    printf("Lonestar benchmark suite\n");
    printf("Copyright (C) 2007, 2008 The University of Texas at Austin\n");
    printf("http://iss.ices.utexas.edu/lonestar/\n");
    printf("application: BarnesHut v1.0\n");

    if (argc < 2) {
      fprintf(stderr, "\narguments: filename [nreps] [output] [tasks_per_thread]\n");
      exit(-1);
    }
  }
  
  body = NULL;

  timeval starttime, endtime;
  long runtime, lasttime, mintime;
  int run;

  runtime = 0;
  lasttime = -1;
  mintime = -1;
  run = 0;

  nwanted_tasks = nthreads * tasks_per_thread;

  //while (((run < 3) || (labs(lasttime-runtime)*64 > std::min(lasttime, runtime))) && (run < 7)) {

    ReadInput(argv[1], (argc > 2) ? atoi(argv[2]) : 1);

    Range input {0, nbodies};
    Buf = new OctTreeLeafNode_vec_t(nbodies);
    OctTreeLeafNode_vec_t mydest(Actual_bodies, nbodies);
    ExclusiveRangeDInfo myinfo(input, nprocs, nwanted_tasks);
  
    lasttime = runtime;
    gettimeofday(&starttime, NULL);

    for (step = 0; step < timesteps; step++) {  // time-step the system
      double diameter, centerx, centery, centerz;
      ComputeCenterAndDiameter(nbodies, diameter, centerx, centery, centerz);

      // printf("[%d] Actual_bodies %p Buf %p -> %p (iB %p)\n", rank, Actual_bodies, Buf, Buf->data(), myinfo.resultBufferPtr());
      // printf("[%d] (%lf %lf %lf) (%lf %lf %lf)\n", rank, Actual_bodies[0].posx, Actual_bodies[0].posy, Actual_bodies[0].posz, Actual_bodies[nbodies-1].posx, Actual_bodies[nbodies-1].posy, Actual_bodies[nbodies-1].posz);
      
      OctTreeInternalNode *root = OctTreeInternalNode::NewNode(centerx, centery, centerz);  // create the tree's root

      const double radius = diameter * 0.5;
      for (int i = 0; i < nbodies; i++) {
        root->Insert(Actual_bodies + i/*body[i]*/, radius);  // grow the tree by inserting each body
      }

      int curr = 0;
      root->ComputeCenterOfMass(curr);  // summarize subtree info in each internal node (plus restructure tree and sort bodies for performance reasons)
      
      groot = root;
      gdiameter = diameter;

      //const int chunk = ((last_body - first_body) <= nwanted_tasks) ? 1 : ((last_body - first_body) / nwanted_tasks);
      
      int i;
      dpr_pfor(i, 0, nbodies, nwanted_tasks, body[i]->ComputeForce(groot, gdiameter) );
      /*
//#pragma omp parallel for firstprivate(groot, gdiameter) schedule(dynamic, chunk)
      for (int i = first_body; i < last_body; i++) {  // the iterations are independent
        body[i]->ComputeForce(groot, gdiameter);  // compute the acceleration of each body
      }
      */
      
      OctTreeInternalNode::RecycleTree();  // recycle the tree

      dparallel_recursion<OctTreeLeafNode_vec_t>(input, myinfo, Body(), partitioner::simple(), ReplicatedInput | ReusableGather | ReplicateOutput, mydest);
     /*
//#pragma omp parallel for schedule(dynamic, chunk)
      for (int i = first_body; i < last_body; i++) {  // the iterations are independent
        body[i]->Advance();  // advance the position and velocity of each body
        mynodes[i-first_body] = *body[i];
      }
      
      // This reorders the Actual_bodies vector!!
      MPI_Allgather(mynodes, sizeof(OctTreeLeafNode) * bodies_per_rank, MPI_BYTE, Actual_bodies, sizeof(OctTreeLeafNode) * bodies_per_rank, MPI_BYTE, MPI_COMM_WORLD);
      */
      
    }  // end of time step

    delete Buf;
    
    gettimeofday(&endtime, NULL);
    runtime = (long)(endtime.tv_sec*1000.0 + endtime.tv_usec/1000.0 - starttime.tv_sec*1000.0 - starttime.tv_usec/1000.0 + 0.5);
    //if (rank == 0) printf("Run %d time: %f\n", run, runtime / 1000.f);

    if ((runtime < mintime) || (run == 0)) mintime = runtime;
    run++;
  //}

  if (rank == 0) {
    printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
    printf("runs: %d\n", run);
    printf("compute time: %f\n", mintime / 1000.f);

    if (argc > 3) {
      dump_positions(argv[3]);
    }
  }

  MPI_Finalize();

  return 0;
}
