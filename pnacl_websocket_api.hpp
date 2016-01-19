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

#ifndef _PNACL_WEBSOCKET_API_HPP_
#define _PNACL_WEBSOCKET_API_HPP_


#include <string>
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/utility/websocket/websocket_api.h"

namespace pnacl{
  
  class web_socket_receive_listener
  {
  public:
    virtual void on_receive(const std::string& data) = 0;
    virtual void on_connect() =0;
    virtual void on_error(int32_t code, const std::string& reason) = 0;
  };

  class websocket_api
  {
  public:
    websocket_api(pp::Instance* ppinstance, web_socket_receive_listener* recv_listener)
      : socket_(ppinstance),/*instance*/
	callback_factory_(this),
	receive_listener_(recv_listener){}

    websocket_api(pp::Instance* ppinstance, web_socket_receive_listener* recv_listener, const std::string& url)
      : socket_(ppinstance),
	receive_listener_(recv_listener){
      this->open(url);
    }

    virtual ~websocket_api() {
	socket_.Close(PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, "bye...", pp::BlockUntilComplete());
    }
    
    void open(const std::string& url){
      int32_t res=socket_.Connect(url, NULL, 0, callback_factory_.NewCallback(&websocket_api::open_handler));
      if(res!=PP_OK_COMPLETIONPENDING)
	receive_listener_->on_error(res, "socket_.Connect");
    }
    
    void send(const std::string& data) {
      socket_.SendMessage(data);
    }

    void receive(){
      int32_t res=socket_.ReceiveMessage(&received_data_, callback_factory_.NewCallback(&websocket_api::receive_handler));
      if(res!=PP_OK_COMPLETIONPENDING)
	receive_listener_->on_error(res, "socket_.ReceiveMessage");
    }

  private:

    void open_handler(int32_t result){
      if(result!=PP_OK){
	receive_listener_->on_error(result, "socket_.open_handler");
	return;
      }
      receive_listener_->on_connect();
      receive();
    }
    
    void receive_handler(int32_t result){
      if(result!=PP_OK || !received_data_.is_string()){
	receive_listener_->on_error(result, "socket_.receive_handler");
      }
      else{
	receive_listener_->on_receive(received_data_.AsString());
	receive();
      }
    }

    pp::Var received_data_;
    websocket_api(const websocket_api&);
    websocket_api& operator=(const websocket_api&);
    
    web_socket_receive_listener* const receive_listener_;
    pp::WebSocket socket_;
    pp::CompletionCallbackFactory<websocket_api> callback_factory_;
  };

} /*namespace pnacl*/
#endif /* _PNACL_WEBSOCKET_API_HPP_ */
