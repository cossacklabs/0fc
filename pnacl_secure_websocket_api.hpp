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

#ifndef _PNACL_SECURE_WEBSOCKET_API_H_
#define _PNACL_SECURE_WEBSOCKET_API_H_

#include "themispp/secure_session.hpp"
#include "pnacl_websocket_api.hpp"

namespace pnacl {

  class secure_websocket_api{
  public:
    secure_websocket_api(pp::Instance* ppinstance, const std::vector<uint8_t>& server_pub_key, web_socket_receive_listener* recv_listener):
      socket_(ppinstance, this),
      receive_listener_(recv_listener){}

    ~secure_websocket_api(){}


    void open(const std::string& url){
	receive_listener_->on_error(res, "socket_.Connect");
    }
    
    void send(const std::string& data) {
      socket_.send(data);
    }

    void receive(){
      int32_t res=socket_.ReceiveMessage(&received_data_, callback_factory_.NewCallback(&websocket_api::receive_handler));
      if(res!=PP_OK_COMPLETIONPENDING)
	receive_listener_->on_error(res, "socket_.ReceiveMessage");
    }

  private:
    websocket_api socket_;
    web_socket_receive_listener* const receive_listener_;
  };
  
} //end pnacl

#endif /* _PNACL_SECURE_WEBSOCKET_API_H_ */
