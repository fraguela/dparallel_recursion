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



#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <utility>
#include <chrono>
#include <dparallel_recursion/SRange.h>
#include "dparallel_recursion/parallel_stack_recursion.h"

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

typedef std::vector<OctTreeLeafNode> OctTreeLeafNode_vec_t;

static OctTreeLeafNode **body;  // the n bodies
static OctTreeLeafNode *Actual_bodies;

int nthreads = 8;
int chunkSize = 2;		// 1, 2 and 3 seems be the best chunk sizes. Higher chunk sizes are also good with greater DCHUNK value
bool runTests = false;
std::vector<int> listBestChunks0;
std::vector<int> listBestChunks1;
std::vector<int> listBestChunks2;
double testChunkTime = 0.0;
double runTime = 0.0;
dpr::AutomaticChunkOptions opt;

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

OctTreeInternalNode *OctTreeInternalNode::head = NULL;
OctTreeInternalNode *OctTreeInternalNode::freelist = NULL;

const int DCHUNK = 8;

static OctTreeInternalNode *groot;
static double gdiameter;

struct ComputeForceBody : public EmptyBody<SRange, void> {

	static void base(const SRange& r) {
		for (int i = r.start; i < r.end; i++) {
			body[i]->ComputeForce(groot, gdiameter);  // compute the acceleration of each body
		}
	}
};

struct AdvancePositionBody : public EmptyBody<SRange, void> {

	static void base(const SRange& r) {
		for (int i = r.start; i < r.end; i++) {
			body[i]->Advance();  // advance the position and velocity of each body
		}
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
    std::cerr << "file not found: " << filename  << std::endl;
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
    std::cout << "configuration: " << tmp_nbodies << " bodies, " << timesteps << " time steps, " << nreps << " reps" << std::endl;
    body = new OctTreeLeafNode*[nbodies];
    //Actual_bodies = new OctTreeLeafNode[nbodies + nprocs - 1];
    Actual_bodies = new OctTreeLeafNode[nbodies];
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

struct MyReduceBody : public EmptyBody<SRange, a6_t> {

	static a6_t base(const SRange& r) {
		a6_t res{1.0E90, 1.0E90, 1.0E90, -1.0E90, -1.0E90, -1.0E90};
		for (int i = r.start; i < r.end; i++) {
			res = myreduce(res, {Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz, Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz});
		}
		return res;
	}

	static void post(const a6_t& r, a6_t& rr) {
		rr = myreduce(rr, r);
	}
};

static inline void ComputeCenterAndDiameter(const int n, double &diameter, double &centerx, double &centery, double &centerz) {
  a6_t res;
  
  int newChunkSize = chunkSize;
  if (runTests) {
    std::vector<dpr::ResultChunkTest> listChunks;

	//listChunks = parallel_stack_recursion_test<a6_t>(SRange{0, n}, SRangeInfo<DCHUNK>(nthreads), MyReduceBody(), chunkSize, partitioner::simple(), opt);
	//listChunks = psfor_reduce_test<DCHUNK>(0, n, chunkSize, myreduce, [&](int i) { return a6_t{Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz, Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz}; }, opt);
	int i; pr_psfor_reduce_test(i, 0, n, chunkSize, DCHUNK, a6_t({1.0E90, 1.0E90, 1.0E90, -1.0E90, -1.0E90, -1.0E90}), myreduce, listChunks, opt, res = myreduce(res, {Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz, Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz}); );

	if (listChunks.size() > 0) {
      listBestChunks0.push_back(listChunks[0].chunkId);
      //prs_init(nthreads, listChunks[0].chunkId);
      newChunkSize = listChunks[0].chunkId;
    }
  }

  //res = parallel_stack_recursion<a6_t>(SRange{0, n}, SRangeInfo<DCHUNK>(nthreads), MyReduceBody(), newChunkSize, partitioner::simple());
  //res = psfor_reduce<DCHUNK>(0, n, newChunkSize, myreduce, [&](int i) { return a6_t{Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz, Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz}; });
  int i; pr_psfor_reduce(i, 0, n, newChunkSize, DCHUNK, res, a6_t({1.0E90, 1.0E90, 1.0E90, -1.0E90, -1.0E90, -1.0E90}), myreduce, res = myreduce(res, {Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz, Actual_bodies[i].posx, Actual_bodies[i].posy, Actual_bodies[i].posz}); );

  if ((!runTests) && (chunkSize <= 0)) {
	  testChunkTime += dpr::getPsrLastRunExtraInfo().testChunkTime;
	  runTime += dpr::getPsrLastRunExtraInfo().runTime;
	  listBestChunks0.push_back(dpr::getPsrLastRunExtraInfo().chunkSizeUsed);
  }
//  if (runTests) {
//	  prs_init(nthreads, chunkSize);
//  }

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

int main(int argc, char *argv[]) {
	int stackSize = 500000;
	int _partitioner = 0;		//0 = simple, 1 = custom, 2 = automatic. Default: simple
	//int _childgeneration = 0;	//0 = normal, 1 = reference, 2 = normal_aux, 3 = reference_aux. Default: normal
	int limitParallel = 8;

	if (getenv("OMP_NUM_THREADS")) {
		nthreads = atoi(getenv("OMP_NUM_THREADS"));
	}

	if (argc > 4) {
		chunkSize = atoi(argv[4]);
	}

	if (argc > 5) {
		stackSize = atoi(argv[5]);
	}

	if (argc > 6) {
		_partitioner = atoi(argv[6]);
	}

	if (_partitioner == 1) {	//_partitioner = 1 => custom
		if (argc > 7) {
			limitParallel = atoi(argv[7]);
			dpr::setSRangeGlobalLimitParallel(limitParallel);
		}
	}

	if (argc > 8) {
		runTests = true;
		const int testSize = atoi(argv[8]);
		if (testSize < 0) {
			opt.limitTimeOfEachTest = false;
		} else {
			opt.limitTimeOfEachTest = true;
			opt.testSize = testSize;
		}
	}

	if (argc > 9) {
		opt.targetTimePerTest = atof(argv[9]);
	}

	if (argc > 10) {
		opt.maxTime = atof(argv[10]);
	}

	if (argc > 11) {
		opt.initTolerance = atoi(argv[11]);
	}

	if (argc > 12) {
		opt.finalTolerance = atoi(argv[12]);
	}

	if (argc > 13) {
		opt.maxNumChunksTestAllowed = atoi(argv[13]);
	}

	if (argc > 14) {
		opt.mode = atoi(argv[14]);
	}

	if (argc > 15) {
		opt.subMode = atoi(argv[15]);
	}

	if (argc > 16) {
		opt.calcMode = atoi(argv[16]);
	}

	if (argc > 17) {
		opt.verbose = atoi(argv[17]);
	} else {
		opt.verbose = 3;
	}

	prs_init(nthreads, stackSize);

	std::cout << "Lonestar benchmark suite" << std::endl;
	std::cout << "Copyright (C) 2007, 2008 The University of Texas at Austin" << std::endl;
	std::cout << "http://iss.ices.utexas.edu/lonestar/" << std::endl;
	std::cout << "application: BarnesHut v1.0" << std::endl;

	if (argc < 2) {
		std::cerr << std::endl << "arguments: filename [nreps] [output] [chunkSize] [stackSize] [partitioner] [limitParallel] [testSize] [targetTimePerTest] [maxTime] [initTolerance] [finalTolerance] [maxNumChunksTestAllowed] [mode] [subMode] [calcMode] [verbose]" << std::endl;
		exit(-1);
	}

	body = NULL;

	std::chrono::steady_clock::time_point starttime, endtime;
	double runtime, mintime; //double lasttime;

	runtime = 0.0;
	//lasttime = -1;
	mintime = -1.0;

	//while (((run < 3) || (labs(lasttime-runtime)*64 > std::min(lasttime, runtime))) && (run < 7)) {

	ReadInput(argv[1], (argc > 2) ? atoi(argv[2]) : 1);

	SRange input {0, nbodies};

	//lasttime = runtime;
	starttime = std::chrono::steady_clock::now();

	if (!runTests) {
		for (step = 0; step < timesteps; step++) {  // time-step the system
			std::chrono::steady_clock::time_point itime;
			double diameter, centerx, centery, centerz;
			ComputeCenterAndDiameter(nbodies, diameter, centerx, centery, centerz);
			if (chunkSize <= 0) {
				itime = std::chrono::steady_clock::now();
			}

			OctTreeInternalNode *root = OctTreeInternalNode::NewNode(centerx, centery, centerz);  // create the tree's root

			const double radius = diameter * 0.5;
			for (int i = 0; i < nbodies; i++) {
				root->Insert(Actual_bodies + i/*body[i]*/, radius);  // grow the tree by inserting each body
			}

			int curr = 0;
			root->ComputeCenterOfMass(curr);  // summarize subtree info in each internal node (plus restructure tree and sort bodies for performance reasons)

			groot = root;
			gdiameter = diameter;
			if (chunkSize <= 0) {
				runTime += std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - itime).count();
			}

			std::chrono::steady_clock::time_point time1, time2;

			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				parallel_stack_recursion<void>(input, SRangeInfo<DCHUNK>(), ComputeForceBody(), chunkSize, partitioner::custom());
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				parallel_stack_recursion<void>(input, SRangeInfo<DCHUNK>(), ComputeForceBody(), chunkSize, partitioner::automatic());
			} else {
				//_partitioner = 0 => simple
				//parallel_stack_recursion<void>(input, SRangeInfo<DCHUNK>(), ComputeForceBody(), chunkSize, partitioner::simple());
				psfor<DCHUNK>(input.start, input.end, chunkSize, [&](int i) { body[i]->ComputeForce(groot, gdiameter); });
				//int i; pr_psfor(i, 0, nbodies, DCHUNK, chunkSize, body[i]->ComputeForce(groot, gdiameter); );
			}

			if (chunkSize <= 0) {
				testChunkTime += dpr::getPsrLastRunExtraInfo().testChunkTime;
				runTime += dpr::getPsrLastRunExtraInfo().runTime;
				listBestChunks1.push_back(dpr::getPsrLastRunExtraInfo().chunkSizeUsed);
			}

			OctTreeInternalNode::RecycleTree();  // recycle the tree

			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				parallel_stack_recursion<void>(input, SRangeInfo<DCHUNK>(), AdvancePositionBody(), chunkSize, partitioner::custom());
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				parallel_stack_recursion<void>(input, SRangeInfo<DCHUNK>(), AdvancePositionBody(), chunkSize, partitioner::automatic());
			} else {
				//_partitioner = 0 => simple
				parallel_stack_recursion<void>(input, SRangeInfo<DCHUNK>(), AdvancePositionBody(), chunkSize, partitioner::simple());
				//psfor<DCHUNK>(input.start, input.end, chunkSize, [&](int i) { body[i]->Advance(); });
				//int i; pr_psfor(i, 0, nbodies, DCHUNK, chunkSize, body[i]->Advance(); );
			}
			if (chunkSize <= 0) {
				testChunkTime += dpr::getPsrLastRunExtraInfo().testChunkTime ;
				runTime += dpr::getPsrLastRunExtraInfo().runTime;
				listBestChunks2.push_back(dpr::getPsrLastRunExtraInfo().chunkSizeUsed);
			}

		}  // end of time step
	} else {
		for (step = 0; step < timesteps; step++) {  // time-step the system
			std::vector<dpr::ResultChunkTest> listChunks;
			double diameter, centerx, centery, centerz;
			ComputeCenterAndDiameter(nbodies, diameter, centerx, centery, centerz);

			OctTreeInternalNode *root = OctTreeInternalNode::NewNode(centerx, centery, centerz);  // create the tree's root

			const double radius = diameter * 0.5;
			for (int i = 0; i < nbodies; i++) {
				root->Insert(Actual_bodies + i/*body[i]*/, radius);  // grow the tree by inserting each body
			}

			int curr = 0;
			root->ComputeCenterOfMass(curr);  // summarize subtree info in each internal node (plus restructure tree and sort bodies for performance reasons)

			groot = root;
			gdiameter = diameter;

			// Normalmente esta es la parte más pesada de la ejecución
			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				listChunks = parallel_stack_recursion_test<void>(input, SRangeInfo<DCHUNK>(), ComputeForceBody(), chunkSize, partitioner::custom(), opt);
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				listChunks = parallel_stack_recursion_test<void>(input, SRangeInfo<DCHUNK>(), ComputeForceBody(), chunkSize, partitioner::automatic(), opt);
			} else {
				//_partitioner = 0 => simple
				//listChunks = parallel_stack_recursion_test<void>(input, SRangeInfo<DCHUNK>(), ComputeForceBody(), chunkSize, partitioner::simple(), opt);
				listChunks = psfor_test<DCHUNK>(input.start, input.end, chunkSize, [&](int i) { body[i]->ComputeForce(groot, gdiameter); }, opt);
				//int i; pr_psfor_test(i, 0, nbodies, DCHUNK, chunkSize, listChunks, opt, body[i]->ComputeForce(groot, gdiameter); );
			}
			listBestChunks1.push_back(listChunks[0].chunkId);

			OctTreeInternalNode::RecycleTree();  // recycle the tree

			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				listChunks = parallel_stack_recursion_test<void>(input, SRangeInfo<DCHUNK>(), AdvancePositionBody(), chunkSize, partitioner::custom(), opt);
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				listChunks = parallel_stack_recursion_test<void>(input, SRangeInfo<DCHUNK>(), AdvancePositionBody(), chunkSize, partitioner::automatic(), opt);
			} else {
				//_partitioner = 0 => simple
				listChunks = parallel_stack_recursion_test<void>(input, SRangeInfo<DCHUNK>(), AdvancePositionBody(), chunkSize, partitioner::simple(), opt);
				//listChunks = psfor_test<DCHUNK>(input.start, input.end, chunkSize, [&](int i) { body[i]->Advance(); }, opt);
				//int i; pr_psfor_test(i, 0, nbodies, DCHUNK, chunkSize, listChunks, opt, body[i]->Advance(); );
			}
			listBestChunks2.push_back(listChunks[0].chunkId);

		}  // end of time step
	}

	endtime = std::chrono::steady_clock::now();
	runtime = std::chrono::duration_cast<std::chrono::duration<double>>(endtime - starttime).count();

	mintime = runtime;
	//}

	std::cout << "Threads=" << nthreads;
	if (chunkSize > 0) {
		std::cout << " chunkSize=" << chunkSize;
	} else {
		std::cout << " chunkSize=(auto)";
	}
	std::cout << " stackSize=" << stackSize << " partitioner=";
	if (_partitioner == 1) {
		std::cout << "custom limitParallel=" << limitParallel << std::endl;
	} else if (_partitioner == 2) {
		std::cout << "automatic" << std::endl;
	} else {
		std::cout << "simple" << std::endl;
	}
	if (!runTests) {
		//std::cout << "runs: " << run << std::endl;
		std::cout << "compute time: " << mintime << std::endl;
		if (chunkSize <= 0) {
			std::cout << "  (autochunk test time): " << testChunkTime << " (run time): " << runTime << std::endl;
			std::cout << "  list best auto chunks (1): ";
			if (listBestChunks0.size() > 0) {
				std::cout << listBestChunks0[0];
				for (size_t i = 1; i < listBestChunks0.size(); ++i) {
					std::cout << ", " << listBestChunks0[i];
				}
			}
			std::cout << std::endl << "  list best auto chunks (2): ";
			if (listBestChunks1.size() > 0) {
				std::cout << listBestChunks1[0];
				for (size_t i = 1; i < listBestChunks1.size(); ++i) {
					std::cout << ", " << listBestChunks1[i];
				}
			}
			std::cout << std::endl << "  list best auto chunks (3): ";
			if (listBestChunks2.size() > 0) {
				std::cout << listBestChunks2[0];
				for (size_t i = 1; i < listBestChunks2.size(); ++i) {
					std::cout << ", " << listBestChunks2[i];
				}
			}
			std::cout << std::endl;
		}
	} else {
		std::cout << "test time: " << mintime << std::endl;
		std::cout << "test parameters: " << "limitTimeOfEachTest=" << opt.limitTimeOfEachTest << " testSize=" << opt.testSize << " targetTimePerTest=" << opt.targetTimePerTest
		<< " maxTime=" << opt.maxTime << " initTolerance=" << opt.initTolerance << " finalTolerance=" << opt.finalTolerance << " maxNumChunksTestAllowed=" << opt.maxNumChunksTestAllowed
		<< " mode=" << opt.mode	<< " subMode=" << opt.subMode << " calcMode=" << opt.calcMode << " verbose=" << opt.verbose << std::endl;
		std::cout << "list best chunks (1): ";
		if (listBestChunks0.size() > 0) {
			std::cout << listBestChunks0[0];
			for (size_t i = 1; i < listBestChunks0.size(); ++i) {
				std::cout << ", " << listBestChunks0[i];
			}
		}
		std::cout << std::endl << "list best chunks (2): ";
		if (listBestChunks1.size() > 0) {
			std::cout << listBestChunks1[0];
			for (size_t i = 1; i < listBestChunks1.size(); ++i) {
				std::cout << ", " << listBestChunks1[i];
			}
		}
		std::cout << std::endl << "list best chunks (3): ";
		if (listBestChunks2.size() > 0) {
			std::cout << listBestChunks2[0];
			for (size_t i = 1; i < listBestChunks2.size(); ++i) {
				std::cout << ", " << listBestChunks2[i];
			}
		}
		std::cout << std::endl;
	}


	if (argc > 3) {
		dump_positions(argv[3]);
	}

	return 0;
}
