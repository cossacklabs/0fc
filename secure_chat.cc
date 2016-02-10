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

#include <cstdio>
#include <string>
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/directory_entry.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/simple_thread.h"
#include "nacl_io/nacl_io.h"


#include "helpers/base64.hpp"
#include "pnacl_fileio_api.hpp"
#include "secure_chat_room.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#define UI_STRING_PARAM(array, i) array.Get(i).AsString()
#define UI_INT_PARAM(array, i) array.Get(i).AsInteger()

#define STR_2_VEC(str) std::vector<uint8_t>((str).c_str(), (str).c_str()+(str).length())
#define B64_2_VEC(str) helpers::base64_decode(str)


class secure_chat_instance : public pp::Instance{
public:
  explicit secure_chat_instance(PP_Instance instance) : 
    pp::Instance(instance),
    store_(NULL),
    url_(""),
    user_name_(""),
    server_id_(""),
    server_pub_(""){
    nacl_io_init_ppapi(instance, pp::Module::Get()->get_browser_interface());
  }

  virtual ~secure_chat_instance() {}
  
  virtual bool Init(uint32_t argc, const char * argn[], const char * argv[]) {
    std::string server_id="";
    std::string server_pub="";
    for(uint32_t i=0;i<argc;++i){
      if(strcmp(argn[i], "url")==0){
	url_=argv[i];
      }
      if(strcmp(argn[i], "server_pub")==0){
	server_pub_=argv[i];
      }
      if(strcmp(argn[i], "server_id")==0){
	server_id_=argv[i];
      }      
    }
    if(url_=="" || server_pub_=="" || server_id_==""){
      return false;
    }
    return true;
  }
      
private:
  pp::Instance* ppinstance_;
  std::shared_ptr<pnacl::fileio_api> store_;
  std::string url_;
  std::string user_name_;
  std::string server_id_;
  std::string server_pub_;
  std::shared_ptr<pnacl::secure_chat_room_t> room_;
  Json::Value user_data_;

public:  
  /*  virtual void on_receive(const std::string& data){
    std::string command;
    std::istringstream st(data);
    postInfo(data);
    st>>command;
    if(command=="ERROR"){
      st.ignore(1);
      postError(st.str());
    }else if(command=="ERRORROOM"){
      std::string new_room_name;
      st>>new_room_name;
      remove_room(new_room_name);
    }else if(command=="NEWROOM"){
      std::string new_room_name;
      st>>new_room_name;
      on_new_room(new_room_name);
    }else if(command=="OPENROOM"){
      std::string history=st.str().substr(9, st.str().length()+1-9);
      on_room_ready(history);
    }
    else if(command=="INVITE"){
      std::string pub_key, my_pub_key, room, invite;
      st>>pub_key>>room>>my_pub_key>>invite;
      std::vector<std::string>::iterator it=std::find(invites_.begin(), invites_.end(), invite);
      if(it!=invites_.end()){
	socket_.send(std::string("INVITE_RESP ")+pub_key+" "+room+" "+webthemispp::base64_encode(room_key_));
	invites_.erase(it);
      }
    }else if(command=="INVITE_RESP"){
      std::string pub_key, room, room_key;
      st>>pub_key>>room>>room_key;;
      room_name_=room;
      store_.mk_sub_key(room_name_);
      store_.save(room_name_+"/pub", public_key_);
      store_.save(room_name_+"/priv", private_key_);
      room_key_=webthemispp::base64_decode(room_key);
      store_.save(room_name_+"/key", room_key_);
      on_room_load();
    }else if(command=="MESSAGE"){
      std::string room, ms;
      st>>room>>ms;
      std::vector<uint8_t> msv=room_encrypter_->decrypt(webthemispp::base64_decode(ms)).get();
      post("new_message", std::string(msv.begin(), msv.end()));
    }else {
      postError("receive malformed response from server");
    }
  }

  virtual void on_error(const std::string& reason){
    post("connection_lost", "reconnecting...");
    postError("connecion lost");
    socket_.open(url_);
  }
  */
private:
  void invite_generate()
  {
    if(!room_)
      postError("generating invite in undefined room");
    room_->generate_invite();
  }

  void room_message_send(const std::string& message){
    if(!room_){
      postError("send message to undefined room");
      return;
    }
    room_->send(message);
  }
  
public:
  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_array()){
      postError("non string messages not supported");
      return;
    }
    const pp::VarArray params(var_message);
    if (UI_STRING_PARAM(params,0) == "login"){ //username, password
      std::string user_name=UI_STRING_PARAM(params,2);
      store_=std::shared_ptr<pnacl::fileio_api>(new pnacl::fileio_api(this, UI_STRING_PARAM(params,2), [this, user_name](){
	    postInfo("file_system_created");
	    store_->load(user_name, [this](const Json::Value& data){
		user_data_=data;
		std::string rooms="{";
		for(int32_t i=user_data_["rooms"].size()-1; i>=0; --i){
		  rooms+="\""+user_data_["rooms"][i]["id"].asString()+"\":\""+user_data_["rooms"][i]["name"].asString()+"\""+(i?",":"");
		}
		rooms+="}";
		post("available_chat_list", rooms);
	      }, [this](int){
		postError("can`t open file ...");
		user_data_=Json::Value(Json::objectValue);
		user_data_["name"]=user_name_;
		user_data_["rooms"]=Json::Value(Json::objectValue);
		post("available_chat_list", "");
	      });	    
	  }, [this](int){
	    postError("can`t open file system");
	    post("available_chat_list", "");
	  }));
    } else if (UI_STRING_PARAM(params,0) == "logout"){
      room_.reset();
    }else if (UI_STRING_PARAM(params,0) == "room.create"){ //room_name
      room_=std::shared_ptr<pnacl::secure_chat_room_t>(new pnacl::secure_chat_room_t(this, url_, STR_2_VEC(server_id_), B64_2_VEC(server_pub_), std::bind(&secure_chat_instance::save_room, this, std::placeholders::_1, std::placeholders::_2), UI_STRING_PARAM(params,1)));
    }else if (UI_STRING_PARAM(params,0) == "room.open"){ //room_name
      room_=std::shared_ptr<pnacl::secure_chat_room_t>(new pnacl::secure_chat_room_t(this, url_, STR_2_VEC(server_id_), B64_2_VEC(server_pub_), std::bind(&secure_chat_instance::save_room, this, std::placeholders::_1, std::placeholders::_2), load_room(UI_STRING_PARAM(params,1))));
    }else if (UI_STRING_PARAM(params,0) == "room.invite.generate"){
      invite_generate();
    }else if (UI_STRING_PARAM(params,0) == "room.open_by_invite"){ //room_name, invite
      room_=std::shared_ptr<pnacl::secure_chat_room_t>(new pnacl::secure_chat_room_t(this, url_, STR_2_VEC(server_id_), B64_2_VEC(server_pub_), std::bind(&secure_chat_instance::save_room, this, std::placeholders::_1, std::placeholders::_2), "aaa",UI_STRING_PARAM(params,1)));
    }else if (UI_STRING_PARAM(params,0) == "room.message.send"){ // room, message
      room_message_send(UI_STRING_PARAM(params,1));
    }else
      postError(std::string("openation ")+UI_STRING_PARAM(params,0)+" not supported");
  }

private:
  const Json::Value& load_room(const std::string& room_id){
    return user_data_["rooms"][room_id];
  }
  
  void save_room(const std::string& room_id, const Json::Value& val){
    user_data_["rooms"][room_id]=val;
    store_->save(user_name_, user_data_, [this](){
	postInfo("data saved successfully");
      },[this](int error_code){
	postError("data save error");
      });
  }
  
  void post(const std::string& command, const std::string& param1, const std::string& param2=""){
    pp::VarArray message;
    message.Set(0,command);
    message.Set(1,param1);
    if(param2 != ""){
      message.Set(2,param2);
    }
    PostMessage(message);
  }

  void postError(const std::string& message, const std::string& details=""){
    post("error", message, details);
  }

  void postInfo(const std::string& message, const std::string& details=""){
    post("info", message, details);
  }
};


class secure_chat_module : public pp::Module {
public:
  secure_chat_module() : pp::Module() {}
  virtual ~secure_chat_module() {}
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new secure_chat_instance(instance);
  }
};

namespace pp {
  Module* CreateModule() {
    return new secure_chat_module();
  }
} // namespace pp
