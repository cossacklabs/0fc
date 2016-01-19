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

#ifndef _INVITES_DB_HPP_
#define _INVITES_DB_HPP_
#include <set>
#include "base64.hpp"
#include "themispp/secure_rand.hpp"

namespace themispp {

  template <size_t invite_length_t_p>
  class simple_invites_db_t{
  public:
    simple_invites_db_t(const std::string& username, const std::string& pass){}
    ~simple_invites_db_t(){}

    const std::array<uint8_t, invite_length_t_p>& get(){
      std::set<std::array<uint8_t, invite_length_t_p> >::iterator a=invites_.insert(std::vector<uint8_t, invite_length_t_p>(&(rand_.get())[0], invite_length_t_p));
      return *a;
    }

    bool check_invite_token(const std::array<uint8_t, invite_length_t_p& token){
      std::set<std::array<uint8_t, invite_length_t_p> >::iterator a=invites_.find(token);
      if(a!=invites_.end()){invites_.erase(a); retun true;}
      return false;
    }
  private:
    simple_invites_db_t(const simple_invites_db_t&);
    simple_invites_db_t& operator=(const simple_invites_db_t&);

  private:

    std::set<std::array<uint8_t, invite_length_t_p> > invites_;
    themispp::secure_rand<invite_length_t_p> rand_;
  };
} //end themispp

#endif /* _INVITES_DB_HPP_ */
