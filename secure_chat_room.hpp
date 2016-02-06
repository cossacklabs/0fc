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
#include "themispp/secure_message.hpp"

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
		       const public_key_t& server_pub_key):
      ppinstance_(ppinstance),
      socket_(server_id, server_pub_key, ppinstance, std::bind(&secure_chat_room_t::on_receive, this, std::placeholders::_1), std::bind(&secure_chat_room_t::on_error, this, std::placeholders::_1)){}

    virtual ~secure_chat_room_t(){
      fprintf(stderr, "room deleted");
    }
    /*new room*/
    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       const std::string& readable_name):
      secure_chat_room_t(ppinstance, url, server_id, server_pub_key)
    {
      owner_=true;
      readable_name_=readable_name;
      themispp::secure_key_pair_generator_t<themispp::EC> gen;
      public_key_=gen.get_pub();
      owner_public_key_=public_key_;
      private_key_=gen.get_priv();
      themispp::secure_rand_t<32> rnd;
      room_key_=rnd.get();
      std::string ii=helpers::base64_encode(public_key_);
      room_encrypter_=std::shared_ptr<themispp::secure_cell_seal_t>(new themispp::secure_cell_seal_t(room_key_)); 
      socket_.open(url, std::vector<uint8_t>(ii.c_str(), ii.c_str()+ii.length()), private_key_, [this](){
	  socket_.send(std::string("PUBKEY ")+helpers::base64_encode(public_key_));
	  socket_.send(std::string("ROOM.CREATE"));
	});
    }

    /*open existing room*/
    /*    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       const std::string& readable_name,
		       const std::string& data):
      secure_chat_room_t(ppinstance, url, server_id, server_pub_key)
    {
      owner_=false;
      readable_name_=readable_name;
      std::string ii=helpers::base64_encode(public_key_);
      socket_.open(url, std::vector<uint8_t>(ii.c_str(), ii.c_str()+ii.length()), private_key_, std::bind(&secure_chat_room_t::on_open_room_connect, this));
    }
    */
    /*open room by invite*/
    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       const std::string& readable_name,
		       const std::string& invite):
      secure_chat_room_t(ppinstance, url, server_id, server_pub_key)
    {
      owner_=false;
      readable_name_=readable_name;
      themispp::secure_key_pair_generator_t<themispp::EC> gen;
      public_key_=gen.get_pub();
      private_key_=gen.get_priv();
      parse_invite(invite);
      themispp::secure_rand_t<32> rnd;
      room_key_=rnd.get();
      std::string ii=helpers::base64_encode(public_key_);
      socket_.open(url, std::vector<uint8_t>(ii.c_str(), ii.c_str()+ii.length()), private_key_, [this](){
	  socket_.send(std::string("PUBKEY ")+helpers::base64_encode(public_key_));
	  themispp::secure_message_t sm(private_key_, owner_public_key_);
	  socket_.send("ROOM.INVITE.VERIFICATION_REQUEST "+
		       helpers::base64_encode(owner_public_key_)+" "+
		       name_+" "+
		       helpers::base64_encode(public_key_)+" "+
		       helpers::base64_encode(invite_id_)+" "+
		       helpers::base64_encode(sm.encrypt(room_key_)));
	});
    }

    void on_receive(const std::string& data){
      fprintf(stderr, "receive %s", data.c_str());
      std::string command;
      std::istringstream st(data);
      st>>command;
      if(received_command_map_[command]&acceptable_commands_mask_){
	switch(received_command_map_[command]){
	case SCR_ERROR:
	  break;
	case SCR_ROOM_CREATED:
	  st>>name_;
	  post("open_room", readable_name_);
	  break;
	case SCR_ROOM_NOT_EXIST:
	  break;
	case SCR_ROOM_OPENED:
	  post("open_room", readable_name_);
	  break;
	case SCR_ROOM_INVITE_VERIFICATION_REQUEST:{
	  std::string owner_pub_key, name, public_key, invite_id, encrypted_join_key;
	  st>>owner_pub_key>>name>>public_key>>invite_id>>encrypted_join_key;
	  if(helpers::base64_decode(owner_pub_key) != public_key_ || name!=name_){
	    postError("get incorrect invite request");
	    break;
	  }
	  try{
	    themispp::secure_message_t sm(private_key_, helpers::base64_decode(public_key));
	    std::vector<uint8_t> join_key=sm.decrypt(helpers::base64_decode(encrypted_join_key));
	    themispp::secure_cell_seal_t ss(join_key);
	    socket_.send("ROOM.INVITE.VERIFICATION_RESPONSE "+
			 public_key+" "+
			 name+" "+			 
			 owner_pub_key+" "+
			 helpers::base64_encode(ss.encrypt(room_key_, invites_[invite_id])));
	  }catch(themispp::exception_t& e){
	    postError(e.what());
	  }}
	  break;
	case SCR_ROOM_INVITE_VERIFICATION_RESPONSE:{
	  std::string public_key, name, owner_public_key, encoded_room_key;
	  st>>public_key>>name>>owner_public_key>>encoded_room_key;
	  if(helpers::base64_decode(public_key) != public_key_ || name!=name_){
	    postError("get incorrect invite response");
	  }
	  try{
	    themispp::secure_cell_seal_t ss(room_key_);
	    room_key_=ss.decrypt(helpers::base64_decode(encoded_room_key), invite_token_);
	    room_encrypter_=std::shared_ptr<themispp::secure_cell_seal_t>(new themispp::secure_cell_seal_t(room_key_)); 
	    post("open_room", readable_name_);
	  }catch(themispp::exception_t& e){
	    postError(e.what());
	  }}
	  break;
	case SCR_ROOM_MESSAGE_ACCEPTED:{
	  std::string room, message_token, message;
	  st>>room>>message_token>>message;
	  if(room!=name_){
	    postError("get incorrect message request");
	    break;
	  }
	  try{
	    std::vector<uint8_t> msv=room_encrypter_->decrypt(helpers::base64_decode(message), helpers::base64_decode(message_token));
	    post("new_message", std::string(msv.begin(), msv.end()));
	  }catch(themispp::exception_t& e){
	    postError(e.what());	    
	  }}
	  break;
	case SCR_ROOM_MESSAGE_HELLO_ACCEPTED:
	  break;
	case SCR_ROOM_MESSAGE_BYE_ACCEPTED:
	  break;
	default:
	  postError("undefined command received");
	}
      }else{
	postError("unaccceptable command received");
      }
    }
    
    void on_error(const std::string& data){
      fprintf(stderr, "error: %s\n", data.c_str());      
    }

    void generate_invite()
    {
      themispp::secure_rand_t<32> rnd;
      std::vector<uint8_t> new_invite_id=rnd.get();
      std::vector<uint8_t> new_invite_token=rnd.get();
      invites_[helpers::base64_encode(new_invite_id)]=new_invite_token;
      post("invite", serialize_invite(new_invite_id, new_invite_token));
    }

    void send(const std::string& message){
      try{
	themispp::secure_rand_t<32> rnd;
	std::vector<uint8_t> message_token=rnd.get(); 
	socket_.send("ROOM.MESSAGE "+
		     name_+" "+
		     helpers::base64_encode(message_token)+" "+
		     helpers::base64_encode(room_encrypter_->encrypt(std::vector<uint8_t>(message.c_str(), message.c_str()+message.length()), message_token)));
      }catch(themispp::exception_t &e){
	postError(e.what());
      }
    }

  private:
    std::string serialize_invite(const std::vector<uint8_t> id, const std::vector<uint8_t> token){
      std::vector<uint8_t> data(name_.length()+id.size()+token.size()+public_key_.size()+sizeof(int32_t)*4);
      std::vector<uint8_t>::iterator curr=data.begin();
      //name
      uint32_t size=name_.length();
      std::copy((uint8_t*)&size, (uint8_t*)&size+sizeof(size), curr);      
      curr+=4;
      std::copy(name_.c_str(), name_.c_str()+name_.length(), curr);
      curr+=size;
      //public_key
      size=public_key_.size();
      std::copy((uint8_t*)&size, (uint8_t*)&size+sizeof(size), curr);      
      curr+=4;
      std::copy(public_key_.begin(), public_key_.end(), curr);      
      curr+=size;
      //id
      size=id.size();
      std::copy((uint8_t*)&size, (uint8_t*)&size+sizeof(size), curr);      
      curr+=4;
      std::copy(id.begin(), id.end(), curr);      
      curr+=size;
      //token
      size=token.size();
      std::copy((uint8_t*)&size, (uint8_t*)&size+sizeof(size), curr);      
      curr+=4;
      std::copy(token.begin(), token.end(), curr);      
      curr+=size;
      return helpers::base64_encode(data);
    }

    void parse_invite(const std::string& invite){
      std::vector<uint8_t> data=helpers::base64_decode(invite);
      std::vector<uint8_t>::iterator curr=data.begin();
      uint32_t size;
      //name
      std::copy(curr, curr+sizeof(uint32_t), (uint8_t*)&size);
      curr+=4;
      name_=std::string(curr, curr+size);
      curr+=size;
      //owner_public_key_;
      std::copy(curr, curr+sizeof(uint32_t), (uint8_t*)&size);
      curr+=4;
      owner_public_key_.resize(size);
      std::copy(curr, curr+size, owner_public_key_.begin());
      curr+=size;      
      //invite_id_;
      std::copy(curr, curr+sizeof(uint32_t), (uint8_t*)&size);
      curr+=4;
      invite_id_.resize(size);
      std::copy(curr, curr+size, invite_id_.begin());
      curr+=size;      
      //invite_token_;
      std::copy(curr, curr+sizeof(uint32_t), (uint8_t*)&size);
      curr+=4;
      invite_token_.resize(size);
      std::copy(curr, curr+size, invite_token_.begin());
      curr+=size;            
    }

  private:
   enum commands{
      SCR_ERROR=0x00000001,
      SCR_ROOM_CREATED=0x00000002,
      SCR_ROOM_NOT_EXIST=0x00000004,
      SCR_ROOM_OPENED=0x00000010,
      SCR_ROOM_INVITE_VERIFICATION_REQUEST=0x00000020,
      SCR_ROOM_INVITE_VERIFICATION_RESPONSE=0x00000040,
      SCR_ROOM_MESSAGE_ACCEPTED=0x00000100,
      SCR_ROOM_MESSAGE_HELLO_ACCEPTED=0x00000200,
      SCR_ROOM_MESSAGE_BYE_ACCEPTED=0x00000400,
      SCR_ROOM_INVITE_DECLINED=0x00001000
    };
    
    std::map<std::string, uint32_t> received_command_map_={
      {"ERROR", SCR_ERROR},
      {"ROOM.CREATED", SCR_ROOM_CREATED},
      {"ROOM.NOT_EXIST", SCR_ROOM_NOT_EXIST},
      {"ROOM.OPENED", SCR_ROOM_OPENED},
      {"ROOM.INVITE.VERIFICATION_REQUEST", SCR_ROOM_INVITE_VERIFICATION_REQUEST},
      {"ROOM.INVITE.VERIFICATION_RESPONSE", SCR_ROOM_INVITE_VERIFICATION_RESPONSE},
      {"ROOM.INVITE.DECLINED", SCR_ROOM_INVITE_DECLINED},
      {"ROOM.MESSAGE", SCR_ROOM_MESSAGE_ACCEPTED},
      {"ROOM.MESSAGE.HELLO", SCR_ROOM_MESSAGE_HELLO_ACCEPTED},
      {"ROOM.MESSAGE.BYE", SCR_ROOM_MESSAGE_BYE_ACCEPTED}
    };

    uint32_t acceptable_commands_mask_=0xffffffff;
  private:
  std::map<std::string, invite_t > invites_;
  bool owner_;
  pp::Instance* ppinstance_;
  private_key_t private_key_;
  public_key_t public_key_;
  public_key_t owner_public_key_;
  invite_t invite_id_;
  invite_t invite_token_;
  room_key_t room_key_;
  std::string name_;
  std::string readable_name_;
  std::shared_ptr<themispp::secure_cell_seal_t> room_encrypter_;


  private:
    pnacl::secure_websocket_api socket_;

  private:
    void post(const std::string& command, const std::string& param1, const std::string& param2=""){
      pp::VarArray message;
      message.Set(0,command);
      message.Set(1,param1);
      if(param2 != ""){
	message.Set(2,param2);
      }
      ppinstance_->PostMessage(message);
    }

    void postError(const std::string& message, const std::string& details=""){
      post("error", message, details);
    }

    void postInfo(const std::string& message, const std::string& details=""){
      post("info", message, details);
    }

  };
  
} //end pnacl

#endif /* _SECURE_CHAT_ROOM_HPP_ */
