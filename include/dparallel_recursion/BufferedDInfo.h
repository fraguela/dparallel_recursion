/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015-2016 Carlos H. Gonzalez, Basilio B. Fraguela. Universidade da Coruna
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

///
/// \file     BufferedDInfo.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_BUFFEREDDINFO_H_
#define DPR_BUFFEREDDINFO_H_

#include "dparallel_recursion/DInfo.h"

namespace dpr {
  
/// DInfo object that also provides buffers for the reception of the communications
template<typename T, int NCHILDREN, typename Result>
class BufferedDInfo : public DInfo<T, NCHILDREN> {
  
protected:
  
  typedef std::vector<Result> buffer_t;
  
  buffer_t resultBuffer_;
  
  virtual bool prepare_reception(void *ptr, int required) {
    return this->prepare_reception(*(reinterpret_cast<buffer_t *>(ptr)), required);
  }
  
  virtual void save_reception_buffer(void *ptr) {
    this->save_reception_buffer(*(reinterpret_cast<buffer_t *>(ptr)));
  }
  
public:
  
  typedef DInfo<T, NCHILDREN> super;
  
  BufferedDInfo() :
  DInfo<T, NCHILDREN>()
  { }
  
  BufferedDInfo(const buffer_t& b) :
  DInfo<T, NCHILDREN>(),
  resultBuffer_(b)
  { }

  
  BufferedDInfo(buffer_t&& b) :
  DInfo<T, NCHILDREN>(),
  resultBuffer_(std::move(b))
  { }

  BufferedDInfo(int n) :
  DInfo<T, NCHILDREN>(n)
  { }
  
  BufferedDInfo(int n, const buffer_t& b) :
  DInfo<T, NCHILDREN>(n),
  resultBuffer_(b)
  { }
  
  BufferedDInfo(int n, buffer_t&& b) :
  DInfo<T, NCHILDREN>(n),
  resultBuffer_(std::move(b))
  { }
  
  BufferedDInfo(const BufferedDInfo<T, NCHILDREN, Result>& other) :
  DInfo<T, NCHILDREN>(other),
  resultBuffer_(other.resultBuffer_)
  {}

  /// \internal Not public API. Used from dparallel_recursion.h classes
  bool prepare_reception(std::vector<Result>& local_results, int required)
  { bool success;
    
    // This will be also attempted in DInfo::prepare_reception if invoked from there,
    // but it should do nothing
    local_results.resize(required);
    
    // Useless for bitwise serializable data
    if (boost::serialization::is_bitwise_serializable<Result>::value) {
      success = true; //So that save_reception_buffer is not called later
    } else {
      success = !resultBuffer_.empty();
      
      if(success) {
        // Once stablished, the buffer size should be fixed for this object
        assert(resultBuffer_.size() == required);
        
        // The copy constructor of Result should build shallow copies
        for (int i = super::nLocalElems(); i < required; i++)
          new (&(local_results[i])) Result(resultBuffer_[i]);
      }
    }

    return success;
  }
  
  /// \internal Not public API. Used from dparallel_recursion.h classes
  void save_reception_buffer(std::vector<Result>& local_results)
  {
    // Useless for bitwise serializable data
    if (boost::serialization::is_bitwise_serializable<Result>::value) {
      return;
    } else {
      // This should only happen once. Otherwise this method should be incremental, i.e.
      //move from current resultBuffer_.size() to local_results.size()
      assert(resultBuffer_.empty());
      
      // (1) make resultBuffer_ large enough to hold all local_results in future invocations
      const int n = local_results.size();
      resultBuffer_.resize(n);
      
      // (2) steal the ownership of the non-local elements by means of moves
      for (int i = super::nLocalElems(); i < n; i++) {
        resultBuffer_[i] = std::move(local_results[i]);
      }
      
      // (3) re-populate local_results with shallow copies made from resultBuffer_
      prepare_reception(local_results, n);
    }
  }

  /// \internal Not public API. Used from dparallel_recursion.h classes
  template<bool RECV_RESULTS, typename Body>
  std::enable_if_t<(NCHILDREN * sizeof(Result)) != 0>
  distributed_reduce_tree(const int rank, const Behavior& behavior, Body& body, std::vector<Result> *local_results) {
    Behavior tmp_behavior(behavior);
    tmp_behavior |= SafeLoad;
    this->super::template distributed_reduce_tree<RECV_RESULTS>(rank, tmp_behavior, body, local_results);
  }

};

/*
namespace internal {

template<typename T>
struct InfoProvidesResultsBuffer {
    static const bool value = false;
};

template<typename T, int NCHILDREN, typename Result>
struct InfoProvidesResultsBuffer<BufferedDInfo<T, NCHILDREN, Result>> {
  static const bool value = true;
};

}
*/

} // namespace dpr

#endif /* DPR_BUFFEREDDINFO_H_ */
