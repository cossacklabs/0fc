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
#include "json/json.h"

#define KEY_ROTATION_LIMIT 100

namespace pnacl {
  class secure_chat_room_t{
  private:
    typedef std::vector<uint8_t> id_t;
    typedef std::vector<uint8_t> private_key_t;
    typedef std::vector<uint8_t> public_key_t;
    typedef std::vector<uint8_t> room_key_t;
    typedef std::vector<uint8_t> invite_t;

    typedef std::function<void(const std::string& id, const Json::Value& room_data)> save_room_callback_t;
  public:
    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       bool owner,
		       save_room_callback_t save_callback):
		       ppinstance_(ppinstance),
		       owner_(owner),
		       save_callback_(save_callback),
		       socket_(server_id, server_pub_key, ppinstance, std::bind(&secure_chat_room_t::on_receive, this, std::placeholders::_1), std::bind(&secure_chat_room_t::on_error, this, std::placeholders::_1)){}
		       
    virtual ~secure_chat_room_t(){
      fprintf(stderr, "room deleted");
    }
    /*new room*/
    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       save_room_callback_t save_callback,
		       const std::string& readable_name):
		       secure_chat_room_t(ppinstance, url, server_id, server_pub_key, true, save_callback){
      room_data_=Json::Value(Json::objectValue);
      room_data_["readable_name"]=readable_name;
      themispp::secure_key_pair_generator_t<themispp::EC> gen;
      room_data_["public_key"]=helpers::base64_encode(gen.get_pub());
      room_data_["owner_public_key"]=room_data_["public_key"].asString();
      room_data_["private_key"]=helpers::base64_encode(gen.get_priv());
      themispp::secure_rand_t<32> rnd;
      room_data_["room_key"]=helpers::base64_encode(rnd.get());
      room_data_["message_count"]=0;      
      std::string ii=room_data_["public_key"].asString();
      room_encrypter_=std::shared_ptr<themispp::secure_cell_seal_t>(new themispp::secure_cell_seal_t(helpers::base64_decode(room_data_["room_key"].asString()))); 
      socket_.open(url, std::vector<uint8_t>(ii.c_str(), ii.c_str()+ii.length()), helpers::base64_decode(room_data_["private_key"].asString()), [this](){
	  socket_.send(std::string("PUBKEY ")+room_data_["public_key"].asString());
	  socket_.send(std::string("ROOM.CREATE"));
	});
    }

    /*open existing room*/
    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       save_room_callback_t save_callback,
		       const Json::Value& data):
		       secure_chat_room_t(ppinstance, url, server_id, server_pub_key, false, save_callback)
    {
      room_data_=data;
      std::string ii=room_data_["public_key"].asString();//helpers::base64_encode(public_key_);
      room_encrypter_=std::shared_ptr<themispp::secure_cell_seal_t>(new themispp::secure_cell_seal_t(helpers::base64_decode(room_data_["room_key"].asString()))); 
      socket_.open(url, std::vector<uint8_t>(ii.c_str(), ii.c_str()+ii.length()), helpers::base64_decode(room_data_["private_key"].asString()), [this](){
	  socket_.send(std::string("PUBKEY ")+room_data_["public_key"].asString());
	  socket_.send(std::string("ROOM.OPEN ")+room_data_["name"].asString());
	});
    }

    /*open room by invite*/
    secure_chat_room_t(pp::Instance* ppinstance,
		       const std::string& url, 
		       const id_t& server_id,
		       const public_key_t& server_pub_key,
		       save_room_callback_t save_callback,
		       const std::string& readable_name,
		       const std::string& invite):
		       secure_chat_room_t(ppinstance, url, server_id, server_pub_key, false, save_callback){
      room_data_=Json::Value(Json::objectValue);
      room_data_["readable_name"]=readable_name;
      themispp::secure_key_pair_generator_t<themispp::EC> gen;
      room_data_["public_key"]=helpers::base64_encode(gen.get_pub());
      room_data_["private_key"]=helpers::base64_encode(gen.get_priv());
      parse_invite(invite);
      themispp::secure_rand_t<32> rnd;
      room_data_["room_key"]=helpers::base64_encode(rnd.get());
      std::string ii=room_data_["public_key"].asString();
      socket_.open(url, std::vector<uint8_t>(ii.c_str(), ii.c_str()+ii.length()), helpers::base64_decode(room_data_["private_key"].asString()), [this](){
	  socket_.send(std::string("PUBKEY ")+room_data_["public_key"].asString());
	  themispp::secure_message_t sm(helpers::base64_decode(room_data_["private_key"].asString()), helpers::base64_decode(room_data_["owner_public_key"].asString()));
	  socket_.send("ROOM.INVITE.VERIFICATION_REQUEST "+
		       room_data_["owner_public_key"].asString()+" "+
		       room_data_["name"].asString()+" "+
		       room_data_["public_key"].asString()+" "+
		       room_data_["invite_id"].asString()+" "+
		       helpers::base64_encode(sm.encrypt(helpers::base64_decode(room_data_["room_key"].asString()))));
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
	case SCR_ROOM_CREATED:{
	  std::string name;
	  st>>name;
	  room_data_["name"]=name;
	  save_callback_(room_data_["name"].asString(), room_data_);
	  post("open_room", room_data_["readable_name"].asString());
	  break;
	}
	case SCR_ROOM_NOT_EXIST:
	  save_callback_(room_data_["name"].asString(), Json::Value(Json::nullValue));
	  post("remove_room", room_data_["name"].asString());
	  break;
	case SCR_ROOM_OPENED:
	  save_callback_(room_data_["name"].asString(), room_data_);
	  post("open_room", room_data_["readable_name"].asString());
	  break;
	case SCR_ROOM_INVITE_VERIFICATION_REQUEST:{
	  std::string owner_pub_key, name, public_key, invite_id, encrypted_join_key;
	  st>>owner_pub_key>>name>>public_key>>invite_id>>encrypted_join_key;
	  if(owner_pub_key != room_data_["public_key"].asString() || name!=room_data_["name"].asString()){
	    postError("get incorrect invite request");
	    break;
	  }
	  try{
	    themispp::secure_message_t sm(helpers::base64_decode(room_data_["private_key"].asString()), helpers::base64_decode(public_key));
	    std::vector<uint8_t> join_key=sm.decrypt(helpers::base64_decode(encrypted_join_key));
	    themispp::secure_cell_seal_t ss(join_key);
	    socket_.send("ROOM.INVITE.VERIFICATION_RESPONSE "+
			 public_key+" "+
			 name+" "+			 
			 owner_pub_key+" "+
			 helpers::base64_encode(ss.encrypt(helpers::base64_decode(room_data_["room_key"].asString()), helpers::base64_decode(room_data_["invites"][invite_id].asString()))));
	    room_data_["invites"].removeMember(invite_id);
	  }catch(themispp::exception_t& e){
	    postError(e.what());
	  }}
	  break;
	case SCR_ROOM_INVITE_VERIFICATION_RESPONSE:{
	  std::string public_key, name, owner_public_key, encoded_room_key;
	  st>>public_key>>name>>owner_public_key>>encoded_room_key;
	  if(public_key != room_data_["public_key"].asString() || name!=room_data_["name"].asString()){
	    postError("get incorrect invite response");
	  }
	  try{
	    themispp::secure_cell_seal_t ss(helpers::base64_decode(room_data_["room_key"].asString()));
	    room_data_["room_key"]=helpers::base64_encode(ss.decrypt(helpers::base64_decode(encoded_room_key), helpers::base64_decode(room_data_["invite_token"].asString())));
	    room_encrypter_=std::shared_ptr<themispp::secure_cell_seal_t>(new themispp::secure_cell_seal_t(helpers::base64_decode(room_data_["room_key"].asString())));
	    save_callback_(room_data_["name"].asString(), room_data_);
	    post("open_room", room_data_["readable_name"].asString());
	  }catch(themispp::exception_t& e){
	    postError(e.what());
	  }}
	  break;
	case SCR_ROOM_MESSAGE_ACCEPTED:{
	  std::string room, message_token, message;
	  st>>room>>message_token>>message;
	  if(room!=room_data_["name"].asString()){
	    postError("get incorrect message request");
	    break;
	  }
	  try{
	    std::vector<uint8_t> msv=room_encrypter_->decrypt(helpers::base64_decode(message), helpers::base64_decode(message_token));
	    if(!(room_data_["message_count"].isNull())){
	      room_data_["message_count"]=room_data_["message_count"].asInt()+1;
	      if(room_data_["message_count"].asInt()>=KEY_ROTATION_LIMIT){
		try{
		  themispp::secure_rand_t<32> rnd;
		  std::vector<uint8_t> message_token=rnd.get();
		  std::vector<uint8_t> new_room_key=rnd.get();		  
		  socket_.send("ROOM.MESSAGE.KEY.ROTATE "+
			       room_data_["name"].asString()+" "+
			       helpers::base64_encode(message_token)+" "+
			       helpers::base64_encode(room_encrypter_->encrypt(new_room_key, message_token)));
		  room_data_["room_key"]=helpers::base64_encode(new_room_key);
		  room_data_["message_count"]=0;
		  room_encrypter_=std::shared_ptr<themispp::secure_cell_seal_t>(new themispp::secure_cell_seal_t(helpers::base64_decode(room_data_["room_key"].asString())));
		  postInfo("key rotated");
		}catch(themispp::exception_t &e){
		  postError(e.what());
		}
	      }
	    }
	    post("new_message", std::string(msv.begin(), msv.end()));
	  }catch(themispp::exception_t& e){
	    postError(e.what());	    
	  }}
	  break;
	case SCR_ROOM_MESSAGE_KEY_ROTATE:{
	  std::string room, message_token, message;
	  st>>room>>message_token>>message;
	  if(room!=room_data_["name"].asString()){
	    postError("get incorrect message request");
	    break;
	  }
	  try{
	    std::vector<uint8_t> msv=room_encrypter_->decrypt(helpers::base64_decode(message), helpers::base64_decode(message_token));
	    room_data_["room_key"]=helpers::base64_encode(msv);
	    room_encrypter_=std::shared_ptr<themispp::secure_cell_seal_t>(new themispp::secure_cell_seal_t(helpers::base64_decode(room_data_["room_key"].asString())));
	    postInfo("key rotated");
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
      room_data_["invites"][helpers::base64_encode(new_invite_id)]=helpers::base64_encode(new_invite_token);
      post("invite", serialize_invite(new_invite_id, new_invite_token));
    }

    void send(const std::string& message){
      try{
	themispp::secure_rand_t<32> rnd;
	std::vector<uint8_t> message_token=rnd.get(); 
	socket_.send("ROOM.MESSAGE "+
		     room_data_["name"].asString()+" "+
		     helpers::base64_encode(message_token)+" "+
		     helpers::base64_encode(room_encrypter_->encrypt(std::vector<uint8_t>(message.c_str(), message.c_str()+message.length()), message_token)));
      }catch(themispp::exception_t &e){
	postError(e.what());
      }
    }

  private:
    std::string serialize_invite(const std::vector<uint8_t> id, const std::vector<uint8_t> token){
      Json::Value serialized_invite = Json::Value(Json::objectValue);
      serialized_invite["room_name"]=room_data_["name"].asString();
      serialized_invite["readable_name"]=room_data_["readable_name"].asString();
      serialized_invite["public_key"]=room_data_["public_key"].asString();
      serialized_invite["invite"]["id"]=helpers::base64_encode(id);
      serialized_invite["invite"]["token"]=helpers::base64_encode(token);
      std::string serializet_invite_string=Json::FastWriter().write(serialized_invite);
      return helpers::base64_encode(std::vector<uint8_t>(serializet_invite_string.c_str(), serializet_invite_string.c_str()+serializet_invite_string.length()));
    }
    
    void parse_invite(const std::string& invite){
      std::vector<uint8_t> data=helpers::base64_decode(invite);
      std::string invite_str((char*)(&data[0]), data.size());
      Json::Value root;
      Json::Reader().parse(invite_str, root);
      room_data_["name"]=root["room_name"].asString();
      room_data_["readable_name"]=root["readable_name"].asString();
      room_data_["owner_public_key"]=root["public_key"].asString();
      room_data_["invite_id"]=root["invite"]["id"].asString();
      room_data_["invite_token"]=root["invite"]["token"].asString();
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
      SCR_ROOM_INVITE_DECLINED=0x00001000,
      SCR_ROOM_MESSAGE_KEY_ROTATE=0x00002000
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
      {"ROOM.MESSAGE.BYE", SCR_ROOM_MESSAGE_BYE_ACCEPTED},
      {"ROOM.MESSAGE.KEY.ROTATE", SCR_ROOM_MESSAGE_KEY_ROTATE}
    };

    uint32_t acceptable_commands_mask_=0xffffffff;

  private:
    std::map<std::string, invite_t > invites_;
    bool owner_;
    pp::Instance* ppinstance_;
    std::shared_ptr<themispp::secure_cell_seal_t> room_encrypter_;
  private:
    Json::Value room_data_; 

  private:
    pnacl::secure_websocket_api socket_;
    save_room_callback_t save_callback_;

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
