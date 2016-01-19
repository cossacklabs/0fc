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

#ifndef _SECURE_CHAT_CLIENT_HPP_
#define _SECURE_CHAT_CLIENT_HPP_

#include "helpers/base64.hpp"
#include "themispp/secure_cell.hpp"
#include "themispp/secure_rand.hpp"

#define MESSAGE_TOKEN_LENGTH 32

namespace themispp{

  namespace chat {

    class secure_chat_listener_t{
      virtual void on_receive_message(const std::string& data) = 0;
      virtual void on_change_room_pass(const std::string& data) = 0;
      virtual void on_get_history(const std::string& data) = 0;
      virtual void on_open() =0;
      virtual void on_error(int32_t code, const std::string& reason) = 0;
    };
    
    template <class secure_transport_t_p, class secure_transport_listener_t_p>
    class secure_chat_client_transport_t: public secure_transport_listener_t_p{
    public:
      secure_chat_client_transport_t(secure_chat_listener_t* recv_listener):
	transport_(this),
	room_pass(""),
	encrypter_(NULL){
      }
      ~chat_client_transport_t(){
	room_pass_="";
	if(encrypter){
	  delete encrypter; encrypter=NULL;}
      }
      
      void open(const std::string& url, cosnt std::string& username, const std::string& room_pass) {
	encrypter=new themispp::secure_cell_seal(std::vector<uint8_t>(room_pass.data(), room_pass.data()+room_data.length());
	transport_->open(url, username);
      }
      
      void send(const std::string& message){
	  std::vector<uint8_t>& message_token(message_token_generator_.get());
	  std::vector<uint8_t> encrypted_message=encrypter_.encrypt(std::vector<uint8_t>(message.data(), message.length()+1), message_token.begin(), message_token.end());
	  encrypted_message.insert(encrypted_message.begin(), message_token.begin(), message_token.end());
	  transport_->send(base64_encode(encrypted_message));
      }
      
    protected:
      virtual void on_open() {
	listener_->on_open();
      }
      
      virtual void on_receive(const std::string& message){
	std::vector<uint8_t> encrypted_message=base64_decode(message);
	std::vector<uint8_t> encrypter_.decrypt();
      }

      virtual void on_error(int32_t code, const std::string& reason){
	listener_->on_error(code, reason);
      }
      
    private:
      chat_client_transport_t(const chat_client_transport_t&);
      chat_client_transport_t& operator=(const chat_client_transport_t&);

      std::string user_name_;
      secure_chat_listener_t* listener_;
      secure_transport_t_p transport_;
      themispp::secure_cell_seal* encrypter_;
      themispp::secure_rand<MESSAGE_TOKEN_LENGTH> message_token_generator_;
    };
    
    
  } //end chat

} //themispp

#endif /* _SECURE_CHAT_CLIENT_HPP_ */
