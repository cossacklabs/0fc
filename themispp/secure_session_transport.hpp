/*
 * Copyright (c) 2015 Cossack Labs Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SECURE_SESSION_TRANSPORT_HPP_
#define _SECURE_SESSION_TRANSPORT_HPP_

#include "secure_session.hpp"

namespace themispp {

  template <class id_to_pub_key_db_t_p>
  class callback: public themis::secure_session_callback_interface{
  public:
    callback(id_to_pub_key_db& db):
      db_(db){}
      
    const std::vector<uint8_t> get_pub_key_by_id(const std::vector<uint8_t>& id){
      try{
	return db_.find(id);
      }catch (...){}
      return std::vector<uint8_t>(0);
    }
  private:
    callback(const callback&);
    callback& operator=(const callback&);

  private:
    id_to_pub_key_t_p& db_;
  };

  template <class transport_t_p, class id_to_pub_key_db_t_p>
  class secure_session_transport_t_t{
  private:

  public:
    secure_session_transport_t_t(const std::string& endpoint, const id_to_pub_key_db_t_p& db):
      transport_(endpoint),
      callback_(db)
    {}

  private:
    transport_t_p transport_;
    callback<id_to_pub_key_db_t_p> callback_;
  };
  
  
} //end themispp


#endif /* _SECURE_SESSION_TRANSPORT_HPP_ */
