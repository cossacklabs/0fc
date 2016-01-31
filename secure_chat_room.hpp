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

#ifndef _SECURE_CHAT_ROOM_HPP_
#define _SECURE_CHAT_ROOM_HPP_

#include <functional>

#include "pnacl_secure_websocket_api.hpp"
#include "themispp/secure_keygen.hpp"
#include "themispp/secure_rand.hpp"

namespace pnacl {
  class secure_chat_room_t{
  private:
    typedef std::vector<uint8_t> id_t;
    typedef std::vector<uint8_t> private_key_t;
    typedef std::vector<uint8_t> public_key_t;
    typedef std::vector<uint8_t> room_key_t;
    typedef std::vector<uint8_t> invite_t;
  public:
    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       const std::string& readable_name):
      readable_name_(readable_name),
      socket_(server_id, server_pub_key, ppinstance, std::bind(&secure_chat_room_t::on_receive, this, _1), std::bind(&secure_chat_room_t::on_error, this, _1))
    {
      themispp::secure_key_pair_generator_t<themispp::EC> gen;
      public_key_=gen.get_pub();
      private_key_=gen.get_priv();
      socket_.open(url, public_key_, public_key_, std::bind(&secure_chat_room_t::on_new_room_connect, this));
    }

    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       const std::string& readable_name,
		       const std::string& name,
		       const private_key_t& private_key,
		       const public_key_t& public_key,
		       const room_key_t& room_key):
      private_key_(private_key),
      public_key_(public_key),
      room_key_(room_key),
      name_(name),
      readable_name_(readable_name),
      socket_(server_id, server_pub_key, ppinstance, std::bind(&secure_chat_room_t::on_receive, this, _1), std::bind(&secure_chat_room_t::on_error, this, _1))
    {
      socket_.open(url, public_key_, public_key_, std::bind(&secure_chat_room_t::on_open_room_connect, this));
    }

    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       const std::string& readable_name,
		       const invite_t& invite):
      readable_name_(readable_name),
      socket_(server_id, server_pub_key, ppinstance, std::bind(&secure_chat_room_t::on_receive, this, _1), std::bind(&secure_chat_room_t::on_error, this, _1))
    {
      themispp::secure_key_pair_generator_t<themispp::EC> gen;
      public_key_=gen.get_pub();
      private_key_=gen.get_priv();
      socket_.open(url, public_key_, public_key_, std::bind(&secure_chat_room_t::on_open_by_invite_room_connect, this));
    }

    void on_new_room_connect(){}

    void on_open_room_connect(){}

    void on_open_by_invite_room_connect(){}
    void on_receive(const std::string& data){
	  fprintf(stderr, "receive");
    }

    void on_error(const std::string& data){
	  fprintf(stderr, "error");      
    }
  private:
    private_key_t private_key_;
    public_key_t public_key_;
    room_key_t room_key_;
    std::string name_;
    std::string readable_name_;
    themispp::secure_cell_seal_t* room_encrypter_;


  private:
    pnacl::secure_websocket_api socket_;
  };
  
} //end pnacl

#endif /* _SECURE_CHAT_ROOM_HPP_ */
