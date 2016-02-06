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
#include "helpers/base64.hpp"

namespace pnacl {

  class callback: public themispp::secure_session_callback_interface_t{
  public:
    callback(const std::vector<uint8_t>& server_id, const std::vector<uint8_t>& server_pub_key):
      server_id_(server_id),
      server_public_key_(server_pub_key){}
    virtual const std::vector<uint8_t> get_pub_key_by_id(const std::vector<uint8_t>& id){
      if(id==server_id_)
	return server_public_key_;
      return std::vector<uint8_t>(0);
    }
  private:
    std::vector<uint8_t> server_id_;
    std::vector<uint8_t> server_public_key_;
  };
  
  class secure_websocket_api: public web_socket_receive_listener {
  private:
    typedef std::function<void()> connect_callback_t;
    typedef std::function<void(const std::string&)> error_callback_t;
    typedef std::function<void(const std::string&)> receive_callback_t;
    
    
  public:
    secure_websocket_api(const std::vector<uint8_t>& server_id,
			 const std::vector<uint8_t>& server_pub_key,
			 pp::Instance* ppinstance,
			 receive_callback_t on_receive_,
			 error_callback_t on_error_):
      socket_(ppinstance, this),
      callback_(server_id, server_pub_key),
      session_(NULL),
      receive_callback_(on_receive_),
      error_callback_(on_error_)
    {}

    ~secure_websocket_api(){}


    void open(const std::string& url,
	      const std::vector<uint8_t>& id,
	      const std::vector<uint8_t>& priv_key,
	      connect_callback_t on_connect){
      connect_callback_=on_connect;
      try{
	session_ = std::shared_ptr<themispp::secure_session_t>(new themispp::secure_session_t(id, priv_key, &callback_));
	socket_.open(url);
      }catch (themispp::exception_t& e){
	error_callback_(e.what());
      }
    }
    
    void send(const std::string& data) {
      try{
	socket_.send(helpers::base64_encode(session_->wrap(std::vector<uint8_t>(data.c_str(), data.c_str()+data.length()))));
      }catch(themispp::exception_t& e){
	error_callback_(e.what());
      }
    }

    virtual void on_receive(const std::string& data){
      try{
	std::cerr<<data<<std::endl;
	std::vector<uint8_t> d=session_->unwrap(helpers::base64_decode(data));
	if(!(session_->is_established()))
	  socket_.send(helpers::base64_encode(d));
	else if(d.size()==0)
	  connect_callback_();
	else
	  receive_callback_(std::string((const char*)(&d[0]), d.size()));
      }catch(themispp::exception_t& e){
	error_callback_(e.what());
      }
    }
    
    virtual void on_connect(){
      try{
	socket_.send(helpers::base64_encode(session_->init()));
      }catch(themispp::exception_t& e){
	error_callback_(e.what());
      }
    }
    
    virtual void on_error(int32_t code, const std::string& reason){\
      error_callback_(reason);
    }
    
  private:
    websocket_api socket_;
    callback callback_;
    std::shared_ptr<themispp::secure_session_t> session_;
    connect_callback_t connect_callback_;
    error_callback_t error_callback_;
    receive_callback_t receive_callback_;
  };
  
} //end pnacl

#endif /* _PNACL_SECURE_WEBSOCKET_API_H_ */
